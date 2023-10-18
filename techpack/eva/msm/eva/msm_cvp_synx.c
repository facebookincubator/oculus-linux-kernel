// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_core.h"
#include "msm_cvp_dsp.h"
#include "cvp_comm_def.h"


#ifdef CVP_SYNX_ENABLED

#ifdef CVP_CONFIG_SYNX_V2

static int cvp_sess_init_synx_v2(struct msm_cvp_inst *inst)
{

	struct synx_initialization_params params = { 0 };
	params.name = "cvp-kernel-client";
	params.id = SYNX_CLIENT_EVA_CTX0;
	inst->synx_session_id = synx_initialize(&params);
	
	if (IS_ERR_OR_NULL(&inst->synx_session_id)) {
		dprintk(CVP_ERR, "%s synx_initialize failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static int cvp_sess_deinit_synx_v2(struct msm_cvp_inst *inst)
{
	if (!inst) {
		dprintk(CVP_ERR, "Used invalid sess in deinit_synx\n");
		return -EINVAL;
	}
	synx_uninitialize(inst->synx_session_id);
	return 0;
}

static void cvp_dump_fence_queue_v2(struct msm_cvp_inst *inst)
{
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f;
	struct synx_session *ssid;
	int i;

	q = &inst->fence_cmd_queue;
	ssid = inst->synx_session_id;
	mutex_lock(&q->lock);
	dprintk(CVP_WARN, "inst %x fence q mode %d, ssid %pK\n",
			hash32_ptr(inst->session), q->mode, ssid);

	dprintk(CVP_WARN, "fence cmdq wait list:\n");
	list_for_each_entry(f, &q->wait_list, list) {
		dprintk(CVP_WARN, "frame pkt type 0x%x\n", f->pkt->packet_type);
		for (i = 0; i < f->output_index; i++)
			dprintk(CVP_WARN, "idx %d client hdl %d, state %d\n",
				i, f->synx[i],
				synx_get_status(ssid, f->synx[i]));

	}

	dprintk(CVP_WARN, "fence cmdq schedule list:\n");
	list_for_each_entry(f, &q->sched_list, list) {
		dprintk(CVP_WARN, "frame pkt type 0x%x\n", f->pkt->packet_type);
		for (i = 0; i < f->output_index; i++)
			dprintk(CVP_WARN, "idx %d client hdl %d, state %d\n",
				i, f->synx[i],
				synx_get_status(ssid, f->synx[i]));

	}
	mutex_unlock(&q->lock);
}

static int cvp_import_synx_v2(struct msm_cvp_inst *inst,
		struct cvp_fence_command *fc,
		u32 *fence)
{
	int rc = 0, rr = 0;
	int i;
	struct eva_kmd_fence *fs;
	struct synx_import_params params = {0};
	u32 h_synx;
	struct synx_session *ssid;
	fs = (struct eva_kmd_fence *)fence;
	ssid = inst->synx_session_id;

	for (i = 0; i < fc->num_fences; ++i) {
		h_synx = fs[i].h_synx;

		if (h_synx) {
			params.type = SYNX_IMPORT_INDV_PARAMS;
			params.indv.fence = &h_synx;
			params.indv.flags = SYNX_IMPORT_SYNX_FENCE
					| SYNX_IMPORT_LOCAL_FENCE;
			params.indv.new_h_synx = &fc->synx[i];

			rc = synx_import(ssid, &params);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: %u synx_import failed\n",
					__func__, h_synx);
				rr = rc;
			}
		}
	}

	return rr;
}

static int cvp_release_synx_v2(struct msm_cvp_inst *inst,
		struct cvp_fence_command *fc)
{
	int rc = 0;
	int i;
	u32 h_synx;
	struct synx_session *ssid;

	ssid = inst->synx_session_id;
	for (i = 0; i < fc->num_fences; ++i) {
		h_synx = fc->synx[i];
		if (h_synx) {
			rc = synx_release(ssid, h_synx);
			if (rc)
				dprintk(CVP_ERR,
				"%s: synx_release %d, %d failed\n",
				__func__, h_synx, i);
		}
	}
	return rc;
}

static int cvp_cancel_synx_impl(struct msm_cvp_inst *inst,
			enum cvp_synx_type type,
			struct cvp_fence_command *fc,
			int synx_state)
{
	int rc = 0;
	int i;
	u32 h_synx;
	struct synx_session *ssid;
	int start = 0, end = 0;

	ssid = inst->synx_session_id;

	if (type == CVP_INPUT_SYNX) {
		start = 0;
		end = fc->output_index;
	} else if (type == CVP_OUTPUT_SYNX) {
		start = fc->output_index;
		end = fc->num_fences;
	} else {
		dprintk(CVP_ERR, "%s Incorrect synx type\n", __func__);
		return -EINVAL;
	}

	for (i = start; i < end; ++i) {
		h_synx = fc->synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			dprintk(CVP_SYNX, "Cancel synx %d session %llx\n",
					h_synx, inst);
			if (rc)
				dprintk(CVP_ERR,
					"%s: synx_signal %d %d %d failed\n",
				__func__, h_synx, i, synx_state);
		}
	}

	return rc;
}

static int cvp_cancel_synx_v2(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, int synx_state)
{
	return cvp_cancel_synx_impl(inst, type, fc, synx_state);
}

static int cvp_wait_synx(struct synx_session *ssid, u32 *synx, u32 num_synx,
		u32 *synx_state)
{
	int i = 0, rc = 0;
	unsigned long timeout_ms = 2000;
	u32 h_synx;
	while (i < num_synx) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_wait(ssid, h_synx, timeout_ms);
			if (rc) {
				*synx_state = synx_get_status(ssid, h_synx);
				if(*synx_state == SYNX_STATE_SIGNALED_SUCCESS)
				{
					dprintk(CVP_DBG, "%s: SYNX SIGNAl STATE SUCCESS \n", __func__);
					rc=0;
					i++;
					continue;
				}
				else if (*synx_state == SYNX_STATE_SIGNALED_CANCEL) {
					dprintk(CVP_SYNX,
					"%s: synx_wait %d cancel %d state %d\n",
					current->comm, i, rc, *synx_state);
				} else {
					dprintk(CVP_ERR,
					"%s: synx_wait %d failed %d state %d\n",
					current->comm, i, rc, *synx_state);
					*synx_state = SYNX_STATE_SIGNALED_CANCEL;
				}
				return rc;
			} else {
				rc = 0;	/* SYNX_STATE_SIGNALED_SUCCESS = 2 */
			}

			dprintk(CVP_SYNX, "Wait synx %u returned succes\n",
					h_synx);
		}
		++i;
	}
	return rc;
}

static int cvp_signal_synx(struct synx_session *ssid, u32 *synx, u32 num_synx,
		u32 synx_state)
{
	int i = 0, rc = 0;
	u32 h_synx;
	while (i < num_synx) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: synx_signal %u %d failed\n",
					current->comm, h_synx, i);
				synx_state = SYNX_STATE_SIGNALED_CANCEL;
			}
			dprintk(CVP_SYNX, "Signaled synx %u state %d\n",
				h_synx, synx_state);
		}
		++i;
	}
	return rc;
}

static int cvp_synx_ops_v2(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, u32 *synx_state)
{
	struct synx_session *ssid;
	if (fc->signature == 0xB0BABABE)
		return 0;
	ssid = inst->synx_session_id;

	if (type == CVP_INPUT_SYNX) {
		return cvp_wait_synx(ssid, fc->synx, fc->output_index,
				synx_state);
	} else if (type == CVP_OUTPUT_SYNX) {
		return cvp_signal_synx(ssid, &fc->synx[fc->output_index],
				(fc->num_fences - fc->output_index),
				*synx_state);
	} else {
		dprintk(CVP_ERR, "%s Incorrect SYNX type\n", __func__);
		return -EINVAL;
	}
}

static struct msm_cvp_synx_ops cvp_synx = {
	.cvp_sess_init_synx = cvp_sess_init_synx_v2,
	.cvp_sess_deinit_synx = cvp_sess_deinit_synx_v2,
	.cvp_release_synx = cvp_release_synx_v2,
	.cvp_import_synx = cvp_import_synx_v2,
	.cvp_synx_ops = cvp_synx_ops_v2,
	.cvp_cancel_synx = cvp_cancel_synx_v2,
	.cvp_dump_fence_queue = cvp_dump_fence_queue_v2,
};


#else
static int cvp_sess_init_synx_v1(struct msm_cvp_inst *inst)
{
	struct synx_initialization_params params;
	params.name = "cvp-kernel-client";
	if (synx_initialize(&inst->synx_session_id, &params)) {

		dprintk(CVP_ERR, "%s synx_initialize failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static int cvp_sess_deinit_synx_v1(struct msm_cvp_inst *inst)
{
	if (!inst) {
		dprintk(CVP_ERR, "Used invalid sess in deinit_synx\n");
		return -EINVAL;
	}
	synx_uninitialize(inst->synx_session_id);
	return 0;
}

static void cvp_dump_fence_queue_v1(struct msm_cvp_inst *inst)
{
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f;
	struct synx_session ssid;
	int i;

	q = &inst->fence_cmd_queue;
	ssid = inst->synx_session_id;
	mutex_lock(&q->lock);
	dprintk(CVP_WARN, "inst %x fence q mode %d, ssid %d\n",
			hash32_ptr(inst->session), q->mode, ssid.client_id);

	dprintk(CVP_WARN, "fence cmdq wait list:\n");
	list_for_each_entry(f, &q->wait_list, list) {
		dprintk(CVP_WARN, "frame pkt type 0x%x\n", f->pkt->packet_type);
		for (i = 0; i < f->output_index; i++)
			dprintk(CVP_WARN, "idx %d client hdl %d, state %d\n",
				i, f->synx[i],
				synx_get_status(ssid, f->synx[i]));
	}

	dprintk(CVP_WARN, "fence cmdq schedule list:\n");
	list_for_each_entry(f, &q->sched_list, list) {
		dprintk(CVP_WARN, "frame pkt type 0x%x\n", f->pkt->packet_type);
		for (i = 0; i < f->output_index; i++)
			dprintk(CVP_WARN, "idx %d client hdl %d, state %d\n",
				i, f->synx[i],
				synx_get_status(ssid, f->synx[i]));
	}
	mutex_unlock(&q->lock);
}
static int cvp_import_synx_v1(struct msm_cvp_inst *inst,
		struct cvp_fence_command *fc,
		u32 *fence)
{
	int rc = 0, rr = 0;
	int i;
	struct eva_kmd_fence *fs;
	struct synx_import_params params;
	s32 h_synx;
	struct synx_session ssid;

	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s Deprecated synx path\n", __func__);
		return -EINVAL;
	}

	fs = (struct eva_kmd_fence *)fence;
	ssid = inst->synx_session_id;

	for (i = 0; i < fc->num_fences; ++i) {
		h_synx = fs[i].h_synx;

		if (h_synx) {
			params.h_synx = h_synx;
			params.secure_key = fs[i].secure_key;
			params.new_h_synx = &fc->synx[i];

			rc = synx_import(ssid, &params);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: %d synx_import failed\n",
					__func__, h_synx);
				rr = rc;
			}
		}
	}

	return rr;
}

static int cvp_release_synx_v1(struct msm_cvp_inst *inst,
		struct cvp_fence_command *fc)
{
	int rc = 0;
	int i;
	s32 h_synx;
	struct synx_session ssid;
	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s deprecated synx_path\n", __func__);
		return -EINVAL;
	}

	ssid = inst->synx_session_id;
	for (i = 0; i < fc->num_fences; ++i) {
		h_synx = fc->synx[i];
		if (h_synx) {
			rc = synx_release(ssid, h_synx);
			if (rc)
				dprintk(CVP_ERR,
				"%s: synx_release %d, %d failed\n",
				__func__, h_synx, i);
		}
	}
	return rc;
}

static int cvp_cancel_synx_impl(struct msm_cvp_inst *inst,
			enum cvp_synx_type type,
			struct cvp_fence_command *fc,
			int synx_state)
{
	int rc = 0;
	int i;
	int h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;
	ssid = inst->synx_session_id;

	if (type == CVP_INPUT_SYNX) {
		start = 0;
		end = fc->output_index;
	} else if (type == CVP_OUTPUT_SYNX) {
		start = fc->output_index;
		end = fc->num_fences;
	} else {
		dprintk(CVP_ERR, "%s Incorrect synx type\n", __func__);
		return -EINVAL;
	}

	for (i = start; i < end; ++i) {
		h_synx = fc->synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			dprintk(CVP_SYNX, "Cancel synx %d session %llx\n",
					h_synx, inst);
			if (rc)
				dprintk(CVP_ERR,
					"%s: synx_signal %d %d %d failed\n",
				__func__, h_synx, i, synx_state);
		}
	}

	return rc;


}

static int cvp_cancel_synx_v1(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, int synx_state)
{
	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s deprecated synx path\n", __func__);
			return -EINVAL;
		}

	return cvp_cancel_synx_impl(inst, type, fc, synx_state);
}

static int cvp_wait_synx(struct synx_session ssid, u32 *synx, u32 num_synx,
		u32 *synx_state)
{
	int i = 0, rc = 0;
	unsigned long timeout_ms = 2000;
	int h_synx;

	while (i < num_synx) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_wait(ssid, h_synx, timeout_ms);
			if (rc) {
				*synx_state = synx_get_status(ssid, h_synx);
				if (*synx_state == SYNX_STATE_SIGNALED_CANCEL) {
					dprintk(CVP_SYNX,
					"%s: synx_wait %d cancel %d state %d\n",
					current->comm, i, rc, *synx_state);
				} else {
					dprintk(CVP_ERR,
					"%s: synx_wait %d failed %d state %d\n",
					current->comm, i, rc, *synx_state);
					*synx_state = SYNX_STATE_SIGNALED_ERROR;
				}
				return rc;
			}
			dprintk(CVP_SYNX, "Wait synx %d returned succes\n",
					h_synx);
		}
		++i;
	}
	return rc;
}

static int cvp_signal_synx(struct synx_session ssid, u32 *synx, u32 num_synx,
		u32 synx_state)
{
	int i = 0, rc = 0;
	int h_synx;

	while (i < num_synx) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: synx_signal %d %d failed\n",
					current->comm, h_synx, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
			dprintk(CVP_SYNX, "Signaled synx %d\n", h_synx);
		}
		++i;
	}
	return rc;
}

static int cvp_synx_ops_v1(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, u32 *synx_state)
{
	struct synx_session ssid;

	ssid = inst->synx_session_id;

	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s deprecated synx, type %d\n", __func__);
				return -EINVAL;
	}

	if (type == CVP_INPUT_SYNX) {
		return cvp_wait_synx(ssid, fc->synx, fc->output_index,
				synx_state);
	} else if (type == CVP_OUTPUT_SYNX) {
		return cvp_signal_synx(ssid, &fc->synx[fc->output_index],
				(fc->num_fences - fc->output_index),
				*synx_state);
	} else {
		dprintk(CVP_ERR, "%s Incorrect SYNX type\n", __func__);
		return -EINVAL;
	}
}

static struct msm_cvp_synx_ops cvp_synx = {
	.cvp_sess_init_synx = cvp_sess_init_synx_v1,
	.cvp_sess_deinit_synx = cvp_sess_deinit_synx_v1,
	.cvp_release_synx = cvp_release_synx_v1,
	.cvp_import_synx = cvp_import_synx_v1,
	.cvp_synx_ops = cvp_synx_ops_v1,
	.cvp_cancel_synx = cvp_cancel_synx_v1,
	.cvp_dump_fence_queue = cvp_dump_fence_queue_v1,
};

#endif	/* End of CVP_CONFIG_SYNX_V2 */
#else
static int cvp_sess_init_synx_stub(struct msm_cvp_inst *inst)
{
	return 0;
}

static int cvp_sess_deinit_synx_stub(struct msm_cvp_inst *inst)
{
	return 0;
}

static int cvp_release_synx_stub(struct msm_cvp_inst *inst,
		struct cvp_fence_command *fc)
{
	return 0;
}

static int cvp_import_synx_stub(struct msm_cvp_inst *inst,
		struct cvp_fence_command *fc,
		u32 *fence)
{
	return 0;
}

static int cvp_synx_ops_stub(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, u32 *synx_state)
{
	return 0;
}

static int cvp_cancel_synx_stub(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, int synx_state)
{
	return 0;
}

static void cvp_dump_fence_queue_stub(struct msm_cvp_inst *inst)
{
}

static struct msm_cvp_synx_ops cvp_synx = {
	.cvp_sess_init_synx = cvp_sess_init_synx_stub,
	.cvp_sess_deinit_synx = cvp_sess_deinit_synx_stub,
	.cvp_release_synx = cvp_release_synx_stub,
	.cvp_import_synx = cvp_import_synx_stub,
	.cvp_synx_ops = cvp_synx_ops_stub,
	.cvp_cancel_synx = cvp_cancel_synx_stub,
	.cvp_dump_fence_queue = cvp_dump_fence_queue_stub,
};


#endif	/* End of CVP_SYNX_ENABLED */

void cvp_synx_ftbl_init(struct msm_cvp_core *core)
{
	if (!core)
		return;
	/* Synx API version check below if needed */
	core->synx_ftbl = &cvp_synx;
}
