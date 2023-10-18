// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */
/* Copyright (c) 2022. Qualcomm Innovation Center, Inc. All rights reserved. */

#include "msm_vidc_control.h"
#include "msm_vidc_debug.h"
#include "hfi_packet.h"
#include "hfi_property.h"
#include "venus_hfi.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_driver.h"
#include "msm_venc.h"
#include "msm_vidc_platform.h"

#define CAP_TO_8BIT_QP(a) {          \
	if ((a) < MIN_QP_8BIT)                 \
		(a) = MIN_QP_8BIT;             \
}

extern struct msm_vidc_core *g_core;

static bool is_priv_ctrl(u32 id)
{
	bool private = false;

	if (IS_PRIV_CTRL(id))
		return true;

	/*
	 * Treat below standard controls as private because
	 * we have added custom values to the controls
	 */
	switch (id) {
	/*
	 * TODO: V4L2_CID_MPEG_VIDEO_HEVC_PROFILE is std ctrl. But
	 * V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10_STILL_PICTURE support is not
	 * available yet. Hence, make this as private ctrl for time being
	 */
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
	/*
	 * TODO: V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE is
	 * std ctrl. But needs some fixes in v4l2-ctrls.c. Hence,
	 * make this as private ctrl for time being
	 */
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE:
	/*
	 * TODO: treat below std ctrls as private ctrls until
	 * all below ctrls are available in upstream
	 */
	case V4L2_CID_MPEG_VIDEO_AU_DELIMITER:
	case V4L2_CID_MPEG_VIDEO_LTR_COUNT:
	case V4L2_CID_MPEG_VIDEO_FRAME_LTR_INDEX:
	case V4L2_CID_MPEG_VIDEO_USE_LTR_FRAMES:
	case V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY:
	case V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY_ENABLE:
		private = true;
		break;
	default:
		private = false;
		break;
	}

	return private;
}

static const char *const mpeg_video_rate_control[] = {
	"VBR",
	"CBR",
	"CBR VFR",
	"MBR",
	"MBR VFR",
	"CQ",
	NULL,
};

static const char *const mpeg_video_stream_format[] = {
	"NAL Format Start Codes",
	"NAL Format One NAL Per Buffer",
	"NAL Format One Byte Length",
	"NAL Format Two Byte Length",
	"NAL Format Four Byte Length",
	NULL,
};

static const char *const mpeg_video_blur_types[] = {
	"Blur None",
	"Blur External",
	"Blur Adaptive",
	NULL,
};

static const char *const mpeg_video_avc_coding_layer[] = {
	"B",
	"P",
	NULL,
};

static const char *const mpeg_video_hevc_profile[] = {
	"Main",
	"Main Still Picture",
	"Main 10",
	"Main 10 Still Picture",
	NULL,
};

static const char * const av1_profile[] = {
	"Main",
	"High",
	"Professional",
	NULL,
};

static const char * const av1_level[] = {
	"2.0",
	"2.1",
	"2.2",
	"2.3",
	"3.0",
	"3.1",
	"3.2",
	"3.3",
	"4.0",
	"4.1",
	"4.2",
	"4.3",
	"5.0",
	"5.1",
	"5.2",
	"5.3",
	"6.0",
	"6.1",
	"6.2",
	"6.3",
	"7.0",
	"7.1",
	"7.2",
	"7.3",
	NULL,
};

static const char * const av1_tier[] = {
	"Main",
	"High",
	NULL,
};

static const char *const mpeg_video_vidc_ir_type[] = {
	"Random",
	"Cyclic",
	NULL,
};

static const char *const mpeg_vidc_delivery_modes[] = {
	"Frame Based Delivery Mode",
	"Slice Based Delivery Mode",
	NULL,
};

u32 msm_vidc_get_port_info(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id)
{
	struct msm_vidc_inst_capability *capability = inst->capabilities;

	if (capability->cap[cap_id].flags & CAP_FLAG_INPUT_PORT &&
		capability->cap[cap_id].flags & CAP_FLAG_OUTPUT_PORT) {
		if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
			return get_hfi_port(inst, INPUT_PORT);
		else
			return get_hfi_port(inst, OUTPUT_PORT);
	}

	if (capability->cap[cap_id].flags & CAP_FLAG_INPUT_PORT)
		return get_hfi_port(inst, INPUT_PORT);
	else if (capability->cap[cap_id].flags & CAP_FLAG_OUTPUT_PORT)
		return get_hfi_port(inst, OUTPUT_PORT);
	else
		return HFI_PORT_NONE;
}

static const char * const * msm_vidc_get_qmenu_type(
		struct msm_vidc_inst *inst, u32 control_id)
{
	switch (control_id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
		return mpeg_video_rate_control;
	case V4L2_CID_MPEG_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD:
		return mpeg_video_stream_format;
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_TYPES:
		return mpeg_video_blur_types;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE:
		return mpeg_video_avc_coding_layer;
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		return mpeg_video_hevc_profile;
	case V4L2_CID_MPEG_VIDEO_AV1_PROFILE:
		return av1_profile;
	case V4L2_CID_MPEG_VIDEO_AV1_LEVEL:
		return av1_level;
	case V4L2_CID_MPEG_VIDEO_AV1_TIER:
		return av1_tier;
	case V4L2_CID_MPEG_VIDEO_VIDC_INTRA_REFRESH_TYPE:
		return mpeg_video_vidc_ir_type;
	case V4L2_CID_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE:
		return mpeg_vidc_delivery_modes;
	case V4L2_CID_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE:
		return mpeg_vidc_delivery_modes;
	default:
		i_vpr_e(inst, "%s: No available qmenu for ctrl %#x\n",
			__func__, control_id);
		return NULL;
	}
}

static inline bool has_parents(struct msm_vidc_inst_cap *cap)
{
	return !!cap->parents[0];
}

static inline bool has_childrens(struct msm_vidc_inst_cap *cap)
{
	return !!cap->children[0];
}

static inline bool is_root(struct msm_vidc_inst_cap *cap)
{
	return !has_parents(cap);
}

static inline bool is_valid_cap_id(enum msm_vidc_inst_capability_type cap_id)
{
	return cap_id > INST_CAP_NONE && cap_id < INST_CAP_MAX;
}

static inline bool is_valid_cap(struct msm_vidc_inst_cap *cap)
{
	return is_valid_cap_id(cap->cap_id);
}

static inline bool is_all_parents_visited(
	struct msm_vidc_inst_cap *cap, bool lookup[INST_CAP_MAX]) {
	bool found = true;
	int i;

	for (i = 0; i < MAX_CAP_PARENTS; i++) {
		if (cap->parents[i] == INST_CAP_NONE)
			continue;

		if (!lookup[cap->parents[i]]) {
			found = false;
			break;
		}
	}
	return found;
}

static int add_node_list(struct list_head *list, enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst_cap_entry *entry = NULL;

	rc = msm_vidc_vmem_alloc(sizeof(struct msm_vidc_inst_cap_entry),
			(void **)&entry, __func__);
	if (rc)
		return rc;

	INIT_LIST_HEAD(&entry->list);
	entry->cap_id = cap_id;
	list_add_tail(&entry->list, list);

	return rc;
}

static int add_node(
	struct list_head *list, struct msm_vidc_inst_cap *rcap, bool lookup[INST_CAP_MAX])
{
	int rc = 0;

	if (lookup[rcap->cap_id])
		return 0;

	rc = add_node_list(list, rcap->cap_id);
	if (rc)
		return rc;

	lookup[rcap->cap_id] = true;
	return 0;
}

static int swap_node(struct msm_vidc_inst_cap *rcap,
	struct list_head *src_list, bool src_lookup[INST_CAP_MAX],
	struct list_head *dest_list, bool dest_lookup[INST_CAP_MAX])
{
	struct msm_vidc_inst_cap_entry *entry, *temp;
	bool found = false;

	/* cap must be available in src and not present in dest */
	if (!src_lookup[rcap->cap_id] || dest_lookup[rcap->cap_id]) {
		d_vpr_e("%s: not found in src or already found in dest for cap %s\n",
			__func__, cap_name(rcap->cap_id));
		return -EINVAL;
	}

	/* check if entry present in src_list */
	list_for_each_entry_safe(entry, temp, src_list, list) {
		if (entry->cap_id == rcap->cap_id) {
			found = true;
			break;
		}
	}

	if (!found) {
		d_vpr_e("%s: cap %s not found in src list\n",
			__func__, cap_name(rcap->cap_id));
		return -EINVAL;
	}

	/* remove from src_list */
	list_del_init(&entry->list);
	src_lookup[rcap->cap_id] = false;

	/* add it to dest_list */
	list_add_tail(&entry->list, dest_list);
	dest_lookup[rcap->cap_id] = true;

	return 0;
}

static int msm_vidc_packetize_control(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id, u32 payload_type,
	void *hfi_val, u32 payload_size, const char *func)
{
	int rc = 0;
	u64 payload = 0;

	if (payload_size > sizeof(u32)) {
		i_vpr_e(inst, "%s: payload size is more than u32 for cap[%d] %s\n",
			func, cap_id, cap_name(cap_id));
		return -EINVAL;
	}

	if (payload_size == sizeof(u32))
		payload = *(u32 *)hfi_val;
	else if (payload_size == sizeof(u8))
		payload = *(u8 *)hfi_val;
	else if (payload_size == sizeof(u16))
		payload = *(u16 *)hfi_val;

	i_vpr_h(inst, FMT_STRING_SET_CAP,
		cap_name(cap_id), inst->capabilities->cap[cap_id].value, payload);

	rc = venus_hfi_session_property(inst,
		inst->capabilities->cap[cap_id].hfi_id,
		HFI_HOST_FLAGS_NONE,
		msm_vidc_get_port_info(inst, cap_id),
		payload_type,
		hfi_val,
		payload_size);
	if (rc) {
		i_vpr_e(inst, "%s: failed to set cap[%d] %s to fw\n",
			func, cap_id, cap_name(cap_id));
		return rc;
	}

	return 0;
}

static enum msm_vidc_inst_capability_type msm_vidc_get_cap_id(
	struct msm_vidc_inst *inst, u32 id)
{
	enum msm_vidc_inst_capability_type i = INST_CAP_NONE + 1;
	struct msm_vidc_inst_capability *capability;
	enum msm_vidc_inst_capability_type cap_id = INST_CAP_NONE;

	capability = inst->capabilities;
	do {
		if (capability->cap[i].v4l2_id == id) {
			cap_id = capability->cap[i].cap_id;
			break;
		}
		i++;
	} while (i < INST_CAP_MAX);

	return cap_id;
}

static int msm_vidc_add_capid_to_fw_list(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id)
{
	struct msm_vidc_inst_cap_entry *entry = NULL;
	int rc = 0;

	/* skip adding if cap_id already present in firmware list */
	list_for_each_entry(entry, &inst->firmware_list, list) {
		if (entry->cap_id == cap_id) {
			i_vpr_l(inst,
				"%s: cap[%d] %s already present in fw list\n",
				__func__, cap_id, cap_name(cap_id));
			return 0;
		}
	}

	rc = add_node_list(&inst->firmware_list, cap_id);
	if (rc)
		return rc;

	return 0;
}

static int msm_vidc_add_children(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id)
{
	struct msm_vidc_inst_cap *cap;
	int i, rc = 0;

	cap = &inst->capabilities->cap[cap_id];

	for (i = 0; i < MAX_CAP_CHILDREN; i++) {
		if (!cap->children[i])
			break;

		if (!is_valid_cap_id(cap->children[i]))
			continue;

		rc = add_node_list(&inst->children_list, cap->children[i]);
		if (rc)
			return rc;
	}

	return rc;
}

static bool is_parent_available(struct msm_vidc_inst *inst,
	u32 cap_id, u32 check_parent, const char *func)
{
	int i = 0;
	u32 cap_parent;

	while (i < MAX_CAP_PARENTS &&
		inst->capabilities->cap[cap_id].parents[i]) {
		cap_parent = inst->capabilities->cap[cap_id].parents[i];
		if (cap_parent == check_parent) {
			return true;
		}
		i++;
	}

	i_vpr_e(inst,
		"%s: missing parent %s for %s\n",
		func, cap_name(check_parent), cap_name(cap_id));
	return false;
}

int msm_vidc_update_cap_value(struct msm_vidc_inst *inst, u32 cap_id,
	s32 adjusted_val, const char *func)
{
	int prev_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	prev_value = inst->capabilities->cap[cap_id].value;

	if (is_meta_cap(cap_id)) {
		/*
		 * cumulative control value if client set same metadata
		 * control multiple times.
		 */
		if (adjusted_val & V4L2_MPEG_VIDC_META_ENABLE) {
			/* enable metadata */
			inst->capabilities->cap[cap_id].value |= adjusted_val;
		} else {
			/* disable metadata */
			inst->capabilities->cap[cap_id].value &= ~adjusted_val;
		}
	} else {
		inst->capabilities->cap[cap_id].value = adjusted_val;
	}

	if (prev_value != inst->capabilities->cap[cap_id].value) {
		i_vpr_h(inst,
			"%s: updated database: name: %s, value: %#x -> %#x\n",
			func, cap_name(cap_id),
			prev_value, inst->capabilities->cap[cap_id].value);
	}

	return 0;
}

int msm_vidc_get_parent_value(struct msm_vidc_inst* inst,
	u32 cap_id, u32 parent, s32 *value, const char *func)
{
	int rc = 0;

	if (is_parent_available(inst, cap_id, parent, func)) {
		switch (parent) {
		case BITRATE_MODE:
			*value = inst->hfi_rc_type;
			break;
		case LAYER_TYPE:
			*value = inst->hfi_layer_type;
			break;
		default:
			*value = inst->capabilities->cap[parent].value;
			break;
		}
	} else {
		rc = -EINVAL;
	}

	return rc;
}

static int msm_vidc_adjust_hevc_qp(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id)
{
	struct msm_vidc_inst_capability *capability;
	s32 pix_fmt = -1;

	capability = inst->capabilities;

	if (!(inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)) {
		i_vpr_e(inst,
			"%s: incorrect cap[%d] %s entry in database, fix database\n",
			__func__, cap_id, cap_name(cap_id));
		return -EINVAL;
	}

	if (msm_vidc_get_parent_value(inst, cap_id,
		PIX_FMTS, &pix_fmt, __func__))
		return -EINVAL;

	if (pix_fmt == MSM_VIDC_FMT_P010 || pix_fmt == MSM_VIDC_FMT_TP10C)
		goto exit;

	CAP_TO_8BIT_QP(capability->cap[cap_id].value);
	if (cap_id == MIN_FRAME_QP) {
		CAP_TO_8BIT_QP(capability->cap[I_FRAME_MIN_QP].value);
		CAP_TO_8BIT_QP(capability->cap[P_FRAME_MIN_QP].value);
		CAP_TO_8BIT_QP(capability->cap[B_FRAME_MIN_QP].value);
	} else if (cap_id == MAX_FRAME_QP) {
		CAP_TO_8BIT_QP(capability->cap[I_FRAME_MAX_QP].value);
		CAP_TO_8BIT_QP(capability->cap[P_FRAME_MAX_QP].value);
		CAP_TO_8BIT_QP(capability->cap[B_FRAME_MAX_QP].value);
	}

exit:
	return 0;
}

static int msm_vidc_adjust_cap(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id,
	struct v4l2_ctrl *ctrl, const char *func)
{
	struct msm_vidc_inst_cap *cap;
	int rc = 0;

	/* validate cap_id */
	if (!is_valid_cap_id(cap_id))
		return 0;

	/* validate cap */
	cap = &inst->capabilities->cap[cap_id];
	if (!is_valid_cap(cap))
		return 0;

	/* check if adjust supported */
	if (!cap->adjust) {
		if (ctrl)
			msm_vidc_update_cap_value(inst, cap_id, ctrl->val, func);
		return 0;
	}

	/* call adjust */
	rc = cap->adjust(inst, ctrl);
	if (rc) {
		i_vpr_e(inst, "%s: adjust cap failed for %s\n", func, cap_name(cap_id));
		return rc;
	}

	return rc;
}

static int msm_vidc_set_cap(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id,
	const char *func)
{
	struct msm_vidc_inst_cap *cap;
	int rc = 0;

	/* validate cap_id */
	if (!is_valid_cap_id(cap_id))
		return 0;

	/* validate cap */
	cap = &inst->capabilities->cap[cap_id];
	if (!is_valid_cap(cap))
		return 0;

	/* check if set supported */
	if (!cap->set)
		return 0;

	/* call set */
	rc = cap->set(inst, cap_id);
	if (rc) {
		i_vpr_e(inst, "%s: set cap failed for %s\n", func, cap_name(cap_id));
		return rc;
	}

	return rc;
}

static int msm_vidc_adjust_dynamic_property(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_cap_entry *entry = NULL, *temp = NULL;
	struct msm_vidc_inst_capability *capability;
	s32 prev_value;
	int rc = 0;

	if (!inst || !inst->capabilities || !ctrl) {
		d_vpr_e("%s: invalid param\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	/* sanitize cap_id */
	if (!is_valid_cap_id(cap_id)) {
		i_vpr_e(inst, "%s: invalid cap_id %u\n", __func__, cap_id);
		return -EINVAL;
	}

	if (!(capability->cap[cap_id].flags & CAP_FLAG_DYNAMIC_ALLOWED)) {
		i_vpr_h(inst,
			"%s: dynamic setting of cap[%d] %s is not allowed\n",
			__func__, cap_id, cap_name(cap_id));
		return -EBUSY;
	}
	i_vpr_h(inst, "%s: cap[%d] %s\n", __func__, cap_id, cap_name(cap_id));

	prev_value = capability->cap[cap_id].value;
	rc = msm_vidc_adjust_cap(inst, cap_id, ctrl, __func__);
	if (rc)
		return rc;

	if (capability->cap[cap_id].value == prev_value && cap_id == GOP_SIZE) {
		/*
		 * Ignore setting same GOP size value to firmware to avoid
		 * unnecessary generation of IDR frame.
		 */
		return 0;
	}

	/* add cap_id to firmware list always */
	rc = msm_vidc_add_capid_to_fw_list(inst, cap_id);
	if (rc)
		goto error;

	/* Add children only if cap value modified except EARLY_NOTIFY_LINE_COUNT cap ID
	 *
	 * Allow to invoke EARLY_NOTIFY_LINE_COUNT cap ID child(EARLY_NOTIFY_FENCE_COUNT)
	 * adjust function even if parent cap value is not modified, otherwise Early notify
	 * use-case will fail in some scenario.
	 *
	 * Ex: When client requesting two interrupt per frame
	 * Before IPSC:
	 *      HEIGHT: 240 (This is default value initialized for new session)
	 *      adjusted_value(Line count): 256(This should be multiple of 256)
	 *      Fence count: 1
	 * After IPSC:
	 *      HEIGHT: 480 (Height is updated after IPSC)
	 *      adjusted_value(Line count): 256 (This will remain same because client is expecting
	 *      two interrupt notification for 480p resolution)
	 *      Fence count: 1
	 *
	 * After IPSC, Fence count should be updated as 2 for 480p resolution
	 * But this is not happening because Line count(parent) does't change, so corresponding
	 * child(EARLY_NOTIFY_FENCE_COUNT) adjust function will not be invoked.
	 *
	 * To handle this use-case need to call child adjust function
	 * even if parent cap value does't changed.
	 */
	if (capability->cap[cap_id].value == prev_value && cap_id != EARLY_NOTIFY_LINE_COUNT)
		return 0;

	rc = msm_vidc_add_children(inst, cap_id);
	if (rc)
		goto error;

	list_for_each_entry_safe(entry, temp, &inst->children_list, list) {
		if (!is_valid_cap_id(entry->cap_id)) {
			rc = -EINVAL;
			goto error;
		}

		if (!capability->cap[entry->cap_id].adjust) {
			i_vpr_e(inst, "%s: child cap must have ajdust function %s\n",
				__func__, cap_name(entry->cap_id));
			rc = -EINVAL;
			goto error;
		}

		prev_value = capability->cap[entry->cap_id].value;
		rc = msm_vidc_adjust_cap(inst, entry->cap_id, NULL, __func__);
		if (rc)
			goto error;

		/* add children if cap value modified */
		if (capability->cap[entry->cap_id].value != prev_value) {
			/* add cap_id to firmware list always */
			rc = msm_vidc_add_capid_to_fw_list(inst, entry->cap_id);
			if (rc)
				goto error;

			rc = msm_vidc_add_children(inst, entry->cap_id);
			if (rc)
				goto error;
		}

		list_del_init(&entry->list);
		msm_vidc_vmem_free((void **)&entry);
	}

	/* expecting children_list to be empty */
	if (!list_empty(&inst->children_list)) {
		i_vpr_e(inst, "%s: child_list is not empty\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	return 0;
error:
	list_for_each_entry_safe(entry, temp, &inst->children_list, list) {
		i_vpr_e(inst, "%s: child list: %s\n", __func__, cap_name(entry->cap_id));
		list_del_init(&entry->list);
		msm_vidc_vmem_free((void **)&entry);
	}
	list_for_each_entry_safe(entry, temp, &inst->firmware_list, list) {
		i_vpr_e(inst, "%s: fw list: %s\n", __func__, cap_name(entry->cap_id));
		list_del_init(&entry->list);
		msm_vidc_vmem_free((void **)&entry);
	}

	return rc;
}

static int msm_vidc_set_dynamic_property(struct msm_vidc_inst *inst)
{
	struct msm_vidc_inst_cap_entry *entry = NULL, *temp = NULL;
	int rc = 0;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	i_vpr_h(inst, "%s()\n", __func__);

	list_for_each_entry_safe(entry, temp, &inst->firmware_list, list) {
		rc = msm_vidc_set_cap(inst, entry->cap_id, __func__);
		if (rc)
			goto error;

		list_del_init(&entry->list);
		msm_vidc_vmem_free((void **)&entry);
	}

	return 0;
error:
	list_for_each_entry_safe(entry, temp, &inst->firmware_list, list) {
		i_vpr_e(inst, "%s: fw list: %s\n", __func__, cap_name(entry->cap_id));
		list_del_init(&entry->list);
		msm_vidc_vmem_free((void **)&entry);
	}

	return rc;
}

void msm_vidc_add_volatile_flag(struct v4l2_ctrl *ctrl)
{
	if (ctrl->id == V4L2_CID_MIN_BUFFERS_FOR_OUTPUT ||
		ctrl->id == V4L2_CID_MIN_BUFFERS_FOR_CAPTURE ||
		ctrl->id == V4L2_CID_MPEG_VIDC_AV1D_FILM_GRAIN_PRESENT ||
		ctrl->id == V4L2_CID_MPEG_VIDC_SW_FENCE_FD)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
}

int msm_vidc_ctrl_handler_deinit(struct msm_vidc_inst *inst)
{
	if (!inst) {
		d_vpr_e("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}
	i_vpr_h(inst, "%s(): num ctrls %d\n", __func__, inst->num_ctrls);
	v4l2_ctrl_handler_free(&inst->ctrl_handler);
	memset(&inst->ctrl_handler, 0, sizeof(struct v4l2_ctrl_handler));
	msm_vidc_vmem_free((void **)&inst->ctrls);
	inst->ctrls = NULL;

	return 0;
}

int msm_vidc_ctrl_handler_update(struct msm_vidc_inst *inst) {
	int rc = 0;
	struct v4l2_ctrl_ref *ref, *next_ref;
	struct v4l2_ctrl *ctrl, *next_ctrl;
	struct v4l2_subscribed_event *sev, *next_sev;
	struct v4l2_ctrl_handler *inst_hdl = &inst->ctrl_handler;

	mutex_lock(inst_hdl->lock);
	/* Free all nodes */
	list_for_each_entry_safe(ref, next_ref, &inst_hdl->ctrl_refs, node) {
		list_del(&ref->node);
		kfree(ref);
	}
	/* Free all controls owned by the handler */
	list_for_each_entry_safe(ctrl, next_ctrl, &inst_hdl->ctrls, node) {
		list_del(&ctrl->node);
		list_for_each_entry_safe(sev, next_sev, &ctrl->ev_subs, node)
			list_del(&sev->node);
		kvfree(ctrl);
	}
	inst_hdl->cached = NULL;
	inst_hdl->error = 0;
	INIT_LIST_HEAD(&inst_hdl->ctrls);
	INIT_LIST_HEAD(&inst_hdl->ctrl_refs);
	memset(inst_hdl->buckets, 0, (inst_hdl->nr_of_buckets * sizeof(inst_hdl->buckets[0])));
	mutex_unlock(inst_hdl->lock);

	/* free the previous codec inst controls memory*/
	msm_vidc_vmem_free((void **)&inst->ctrls);
	inst->ctrls = NULL;

	/* update the ctrl handler with new codec controls*/
	rc = msm_vidc_ctrl_handler_init(inst, false);
	if (rc)
		return rc;

	return rc;


}

int msm_vidc_ctrl_handler_init(struct msm_vidc_inst *inst, bool init)
{
	int rc = 0;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_core *core;
	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg = {0};
	int num_ctrls = 0, ctrl_idx = 0;

	if (!inst || !inst->core || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	capability = inst->capabilities;

	if (!core->v4l2_ctrl_ops) {
		i_vpr_e(inst, "%s: no control ops\n", __func__);
		return -EINVAL;
	}

	for (idx = 0; idx < INST_CAP_MAX; idx++) {
		if (capability->cap[idx].v4l2_id)
			num_ctrls++;
	}
	if (!num_ctrls) {
		i_vpr_e(inst, "%s: no ctrls available in cap database\n",
			__func__);
		return -EINVAL;
	}
	rc = msm_vidc_vmem_alloc(num_ctrls * sizeof(struct v4l2_ctrl *),
			(void **)&inst->ctrls, __func__);
	if (rc)
		return rc;

	if (init) {
		rc = v4l2_ctrl_handler_init(&inst->ctrl_handler, num_ctrls);
		if (rc) {
			i_vpr_e(inst, "control handler init failed, %d\n",
					inst->ctrl_handler.error);
			goto error;
		}
	}

	for (idx = 0; idx < INST_CAP_MAX; idx++) {
		struct v4l2_ctrl *ctrl;

		if (!capability->cap[idx].v4l2_id)
			continue;

		if (ctrl_idx >= num_ctrls) {
			i_vpr_e(inst,
				"%s: invalid ctrl %#x, max allowed %d\n",
				__func__, capability->cap[idx].v4l2_id,
				num_ctrls);
			rc = -EINVAL;
			goto error;
		}
		i_vpr_l(inst,
			"%s: cap[%d] %24s, value %d min %d max %d step_or_mask %#x flags %#x v4l2_id %#x hfi_id %#x\n",
			__func__, idx, cap_name(idx),
			capability->cap[idx].value,
			capability->cap[idx].min,
			capability->cap[idx].max,
			capability->cap[idx].step_or_mask,
			capability->cap[idx].flags,
			capability->cap[idx].v4l2_id,
			capability->cap[idx].hfi_id);

		memset(&ctrl_cfg, 0, sizeof(struct v4l2_ctrl_config));

		if (is_priv_ctrl(capability->cap[idx].v4l2_id)) {
			/* add private control */
			ctrl_cfg.def = capability->cap[idx].value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = capability->cap[idx].v4l2_id;
			ctrl_cfg.max = capability->cap[idx].max;
			ctrl_cfg.min = capability->cap[idx].min;
			ctrl_cfg.ops = core->v4l2_ctrl_ops;
			if (capability->cap[idx].flags & CAP_FLAG_MENU)
				ctrl_cfg.type = V4L2_CTRL_TYPE_MENU;
			else if (capability->cap[idx].flags & CAP_FLAG_BITMASK)
				ctrl_cfg.type = V4L2_CTRL_TYPE_BITMASK;
			else
				ctrl_cfg.type = V4L2_CTRL_TYPE_INTEGER;
			if (is_meta_cap(idx)) {
				/* bitmask is expected to be enabled for meta controls */
				if (ctrl_cfg.type != V4L2_CTRL_TYPE_BITMASK) {
					i_vpr_e(inst,
						"%s: missing bitmask for cap %s\n",
						__func__, cap_name(idx));
					rc = -EINVAL;
					goto error;
				}
			}
			if (ctrl_cfg.type == V4L2_CTRL_TYPE_MENU) {
				ctrl_cfg.menu_skip_mask =
					~(capability->cap[idx].step_or_mask);
				ctrl_cfg.qmenu = msm_vidc_get_qmenu_type(inst,
					capability->cap[idx].v4l2_id);
			} else {
				ctrl_cfg.step =
					capability->cap[idx].step_or_mask;
			}
			ctrl_cfg.name = cap_name(capability->cap[idx].cap_id);
			if (!ctrl_cfg.name) {
				i_vpr_e(inst, "%s: %#x ctrl name is null\n",
					__func__, ctrl_cfg.id);
				rc = -EINVAL;
				goto error;
			}
			ctrl = v4l2_ctrl_new_custom(&inst->ctrl_handler,
					&ctrl_cfg, NULL);
		} else {
			if (capability->cap[idx].flags & CAP_FLAG_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(
					&inst->ctrl_handler,
					core->v4l2_ctrl_ops,
					capability->cap[idx].v4l2_id,
					capability->cap[idx].max,
					~(capability->cap[idx].step_or_mask),
					capability->cap[idx].value);
			} else {
				ctrl = v4l2_ctrl_new_std(&inst->ctrl_handler,
					core->v4l2_ctrl_ops,
					capability->cap[idx].v4l2_id,
					capability->cap[idx].min,
					capability->cap[idx].max,
					capability->cap[idx].step_or_mask,
					capability->cap[idx].value);
			}
		}
		if (!ctrl) {
			i_vpr_e(inst, "%s: invalid ctrl %#x cap %24s\n", __func__,
				capability->cap[idx].v4l2_id, cap_name(idx));
			rc = -EINVAL;
			goto error;
		}

		rc = inst->ctrl_handler.error;
		if (rc) {
			i_vpr_e(inst,
				"error adding ctrl (%#x) to ctrl handle, %d\n",
				capability->cap[idx].v4l2_id,
				inst->ctrl_handler.error);
			goto error;
		}

		/*
		 * TODO(AS)
		 * ctrl->flags |= capability->cap[idx].flags;
		 */
		msm_vidc_add_volatile_flag(ctrl);
		ctrl->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
		inst->ctrls[ctrl_idx] = ctrl;
		ctrl_idx++;
	}
	inst->num_ctrls = num_ctrls;
	i_vpr_h(inst, "%s(): num ctrls %d\n", __func__, inst->num_ctrls);

	return 0;
error:
	msm_vidc_ctrl_handler_deinit(inst);

	return rc;
}

static int msm_vidc_update_buffer_count_if_needed(struct msm_vidc_inst* inst,
	struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	bool update_input_port = false, update_output_port = false;

	if (!inst || !ctrl) {
		d_vpr_e("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE:
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER:
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING:
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER:
		update_input_port = true;
		break;
	case V4L2_CID_MPEG_VIDC_THUMBNAIL_MODE:
	case V4L2_CID_MPEG_VIDC_PRIORITY:
		update_input_port = true;
		update_output_port = true;
		break;
	default:
		update_input_port = false;
		update_output_port = false;
		break;
	}

	if (update_input_port) {
		rc = msm_vidc_update_buffer_count(inst, INPUT_PORT);
		if (rc)
			return rc;
	}
	if (update_output_port) {
		rc = msm_vidc_update_buffer_count(inst, OUTPUT_PORT);
		if (rc)
			return rc;
	}

	return rc;
}

static int msm_vidc_allow_secure_session(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_inst *i;
	struct msm_vidc_core *core;
	u32 count = 0;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;

	if (!core->capabilities) {
		i_vpr_e(inst, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core_lock(core, __func__);
	list_for_each_entry(i, &core->instances, list) {
		if (i->capabilities) {
			if (i->capabilities->cap[SECURE_MODE].value)
				count++;
		}
	}

	if (count > core->capabilities[MAX_SECURE_SESSION_COUNT].value) {
		i_vpr_e(inst,
			"%s: total secure sessions %d exceeded max limit %d\n",
			__func__, count,
			core->capabilities[MAX_SECURE_SESSION_COUNT].value);
		rc = -EINVAL;
	}
	core_unlock(core, __func__);

	return rc;
}

int msm_v4l2_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst *inst;

	if (!ctrl) {
		d_vpr_e("%s: invalid ctrl parameter\n", __func__);
		return -EINVAL;
	}

	inst = container_of(ctrl->handler,
			    struct msm_vidc_inst, ctrl_handler);
	inst = get_inst_ref(g_core, inst);
	if (!inst) {
		d_vpr_e("%s: could not find inst for ctrl %s id %#x\n",
			__func__, ctrl->name, ctrl->id);
		return -EINVAL;
	}
	client_lock(inst, __func__);
	inst_lock(inst, __func__);

	rc = msm_vidc_get_control(inst, ctrl);
	if (rc) {
		i_vpr_e(inst, "%s: failed for ctrl %s id %#x\n",
			__func__, ctrl->name, ctrl->id);
		goto unlock;
	} else {
		i_vpr_h(inst, "%s: ctrl %s id %#x, value %d\n",
			__func__, ctrl->name, ctrl->id, ctrl->val);
	}

unlock:
	inst_unlock(inst, __func__);
	client_unlock(inst, __func__);
	put_inst(inst);
	return rc;
}

static int msm_vidc_update_static_property(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id, struct v4l2_ctrl *ctrl)
{
	int rc = 0;

	if (!inst || !ctrl) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* update value to db */
	msm_vidc_update_cap_value(inst, cap_id, ctrl->val, __func__);

	if (ctrl->id == V4L2_CID_MPEG_VIDC_CLIENT_ID) {
		rc = msm_vidc_update_debug_str(inst);
		if (rc)
			return rc;
	}

	if (ctrl->id == V4L2_CID_MPEG_VIDC_SECURE) {
		if (ctrl->val) {
			rc = msm_vidc_allow_secure_session(inst);
			if (rc)
				return rc;
		}
	}

	if (ctrl->id == V4L2_CID_ROTATE) {
		struct v4l2_format *output_fmt;

		output_fmt = &inst->fmts[OUTPUT_PORT];
		rc = msm_venc_s_fmt_output(inst, output_fmt);
		if (rc)
			return rc;
	}

	if (ctrl->id == V4L2_CID_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE ||
		ctrl->id == V4L2_CID_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE) {
		struct v4l2_format *output_fmt;

		output_fmt = &inst->fmts[OUTPUT_PORT];
		rc = msm_venc_s_fmt_output(inst, output_fmt);
		if (rc)
			return rc;
	}

	if (ctrl->id == V4L2_CID_MPEG_VIDC_MIN_BITSTREAM_SIZE_OVERWRITE) {
		rc = msm_vidc_update_bitstream_buffer_size(inst);
		if (rc)
			return rc;
	}

	/* call this explicitly to adjust client priority */
	if (ctrl->id == V4L2_CID_MPEG_VIDC_PRIORITY) {
		rc = msm_vidc_adjust_session_priority(inst, ctrl);
		if (rc)
			return rc;
	}

	if (ctrl->id == V4L2_CID_MPEG_VIDC_CRITICAL_PRIORITY)
		msm_vidc_update_cap_value(inst, PRIORITY, 0, __func__);

	if (ctrl->id == V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER) {
		u32 enable;

		/* enable LAYER_ENABLE cap if HEVC_HIER enh layers > 0 */
		if (ctrl->val > 0)
			enable = 1;
		else
			enable = 0;

		msm_vidc_update_cap_value(inst, LAYER_ENABLE, enable, __func__);
	}
	if (is_meta_cap(cap_id)) {
		rc = msm_vidc_update_meta_port_settings(inst);
		if (rc)
			return rc;
	}

	rc = msm_vidc_update_buffer_count_if_needed(inst, ctrl);
	if (rc)
		return rc;

	return rc;
}

int msm_v4l2_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst *inst;
	enum msm_vidc_inst_capability_type cap_id;
	struct msm_vidc_inst_capability *capability;
	u32 port;

	if (!ctrl) {
		d_vpr_e("%s: invalid ctrl parameter\n", __func__);
		return -EINVAL;
	}

	inst = container_of(ctrl->handler,
		struct msm_vidc_inst, ctrl_handler);
	inst = get_inst_ref(g_core, inst);
	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid parameters for inst\n", __func__);
		return -EINVAL;
	}

	client_lock(inst, __func__);
	inst_lock(inst, __func__);
	capability = inst->capabilities;

	i_vpr_h(inst, FMT_STRING_SET_CTRL,
		__func__, state_name(inst->state), ctrl->name, ctrl->id, ctrl->val);

	if (!msm_vidc_allow_s_ctrl(inst, ctrl->id)) {
		rc = -EINVAL;
		goto unlock;
	}

	cap_id = msm_vidc_get_cap_id(inst, ctrl->id);
	if (!is_valid_cap_id(cap_id)) {
		i_vpr_e(inst, "%s: could not find cap_id for ctrl %s\n",
			__func__, ctrl->name);
		rc = -EINVAL;
		goto unlock;
	}

	if (ctrl->id == V4L2_CID_MPEG_VIDC_INPUT_METADATA_FD) {
		if (ctrl->val == INVALID_FD || ctrl->val == INT_MAX) {
			i_vpr_e(inst,
				"%s: client configured invalid input metadata fd %d\n",
				__func__, ctrl->val);
			rc = 0;
			goto unlock;
		}
		if (!capability->cap[INPUT_META_VIA_REQUEST].value) {
			i_vpr_e(inst,
				"%s: input metadata not enabled via request\n", __func__);
			rc = -EINVAL;
			goto unlock;
		}
		rc = msm_vidc_create_input_metadata_buffer(inst, ctrl->val);
		if (rc)
			goto unlock;
	}

	/* mark client set flag */
	capability->cap[cap_id].flags |= CAP_FLAG_CLIENT_SET;

	port = is_encode_session(inst) ? OUTPUT_PORT : INPUT_PORT;
	if (!inst->bufq[port].vb2q->streaming) {
		/* static case */
		rc = msm_vidc_update_static_property(inst, cap_id, ctrl);
		if (rc)
			goto unlock;
	} else {
		/* dynamic case */
		rc = msm_vidc_adjust_dynamic_property(inst, cap_id, ctrl);
		if (rc)
			goto unlock;

		rc = msm_vidc_set_dynamic_property(inst);
		if (rc)
			goto unlock;
	}

unlock:
	inst_unlock(inst, __func__);
	client_unlock(inst, __func__);
	put_inst(inst);
	return rc;
}

int msm_vidc_adjust_entropy_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 profile = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	/* ctrl is always NULL in streamon case */
	adjusted_value = ctrl ? ctrl->val :
		capability->cap[ENTROPY_MODE].value;

	if (inst->codec != MSM_VIDC_H264) {
		i_vpr_e(inst,
			"%s: incorrect entry in database. fix the database\n",
			__func__);
		return 0;
	}

	if (msm_vidc_get_parent_value(inst, ENTROPY_MODE,
		PROFILE, &profile, __func__))
		return -EINVAL;

	if (profile == V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE ||
		profile == V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE)
		adjusted_value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC;

	msm_vidc_update_cap_value(inst, ENTROPY_MODE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_bitrate_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	int lossless, frame_rc, bitrate_mode, frame_skip;
	u32 hfi_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	bitrate_mode = capability->cap[BITRATE_MODE].value;
	lossless = capability->cap[LOSSLESS].value;
	frame_rc = capability->cap[FRAME_RC_ENABLE].value;
	frame_skip = capability->cap[FRAME_SKIP_MODE].value;

	if (lossless || (msm_vidc_lossless_encode &&
		inst->codec == MSM_VIDC_HEVC)) {
		hfi_value = HFI_RC_LOSSLESS;
		goto update;
	}

	if (!frame_rc && !is_image_session(inst)) {
		hfi_value = HFI_RC_OFF;
		goto update;
	}

	if (bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) {
		hfi_value = HFI_RC_VBR_CFR;
	} else if (bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR) {
		if (frame_skip)
			hfi_value = HFI_RC_CBR_VFR;
		else
			hfi_value = HFI_RC_CBR_CFR;
	} else if (bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ) {
		hfi_value = HFI_RC_CQ;
	}

update:
	inst->hfi_rc_type = hfi_value;
	i_vpr_h(inst, "%s: hfi rc type: %#x\n",
		__func__, inst->hfi_rc_type);

	return 0;
}

int msm_vidc_adjust_profile(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 pix_fmt = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[PROFILE].value;

	/* PIX_FMTS dependency is common across all chipsets.
	 * Hence, PIX_FMTS must be specified as Parent for HEVC profile.
	 * Otherwise it would be a database error that should be fixed.
	 */
	if (msm_vidc_get_parent_value(inst, PROFILE, PIX_FMTS,
		&pix_fmt, __func__))
		return -EINVAL;

	/* 10 bit profile for 10 bit color format */
	if (pix_fmt == MSM_VIDC_FMT_TP10C || pix_fmt == MSM_VIDC_FMT_P010) {
		if (is_image_session(inst))
			adjusted_value = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10_STILL_PICTURE;
		else
			adjusted_value = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10;
	} else {
		/* 8 bit profile for 8 bit color format */
		if (is_image_session(inst))
			adjusted_value = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE;
		else
			adjusted_value = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN;
	}

	msm_vidc_update_cap_value(inst, PROFILE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_ltr_count(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1, all_intra = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[LTR_COUNT].value;

	if (msm_vidc_get_parent_value(inst, LTR_COUNT, BITRATE_MODE,
		&rc_type, __func__) ||
		msm_vidc_get_parent_value(inst, LTR_COUNT, ALL_INTRA,
		&all_intra, __func__))
		return -EINVAL;

	if ((rc_type != HFI_RC_OFF &&
		rc_type != HFI_RC_CBR_CFR &&
		rc_type != HFI_RC_CBR_VFR) ||
		all_intra) {
		adjusted_value = 0;
		i_vpr_h(inst,
			"%s: ltr count unsupported, rc_type: %#x, all_intra %d\n",
			__func__,rc_type, all_intra);
	}

	msm_vidc_update_cap_value(inst, LTR_COUNT,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_use_ltr(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value, ltr_count;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[USE_LTR].value;

	/*
	 * Since USE_LTR is only set dynamically, and LTR_COUNT is static
	 * control, no need to make LTR_COUNT as parent for USE_LTR as
	 * LTR_COUNT value will always be updated when dynamically USE_LTR
	 * is set
	 */
	ltr_count = capability->cap[LTR_COUNT].value;
	if (!ltr_count)
		return 0;

	if (adjusted_value <= 0 ||
		adjusted_value > ((1 << ltr_count) - 1)) {
		/*
		 * USE_LTR is bitmask value, hence should be
		 * > 0 and <= (2 ^ LTR_COUNT) - 1
		 */
		i_vpr_e(inst, "%s: invalid value %d\n",
			__func__, adjusted_value);
		return 0;
	}

	/* USE_LTR value is a bitmask value */
	msm_vidc_update_cap_value(inst, USE_LTR,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_mark_ltr(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value, ltr_count;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[MARK_LTR].value;

	/*
	 * Since MARK_LTR is only set dynamically, and LTR_COUNT is static
	 * control, no need to make LTR_COUNT as parent for MARK_LTR as
	 * LTR_COUNT value will always be updated when dynamically MARK_LTR
	 * is set
	 */
	ltr_count = capability->cap[LTR_COUNT].value;
	if (!ltr_count)
		return 0;

	if (adjusted_value < 0 ||
		adjusted_value > (ltr_count - 1)) {
		/* MARK_LTR value should be >= 0 and <= (LTR_COUNT - 1) */
		i_vpr_e(inst, "%s: invalid value %d\n",
			__func__, adjusted_value);
		return 0;
	}

	msm_vidc_update_cap_value(inst, MARK_LTR,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_ir_period(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value, all_intra = 0, roi_enable = 0,
		pix_fmts = MSM_VIDC_FMT_NONE;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[IR_PERIOD].value;

	if (msm_vidc_get_parent_value(inst, IR_PERIOD, ALL_INTRA,
		&all_intra, __func__) ||
		msm_vidc_get_parent_value(inst, IR_PERIOD, META_ROI_INFO,
		&roi_enable, __func__))
		return -EINVAL;

	if (all_intra) {
		adjusted_value = 0;
		i_vpr_h(inst, "%s: intra refresh unsupported, all intra: %d\n",
			__func__, all_intra);
		goto exit;
	}

	if (roi_enable) {
		i_vpr_h(inst,
			"%s: intra refresh unsupported with roi metadata\n",
			__func__);
		adjusted_value = 0;
		goto exit;
	}

	if (inst->codec == MSM_VIDC_HEVC) {
		if (msm_vidc_get_parent_value(inst, IR_PERIOD,
			PIX_FMTS, &pix_fmts, __func__))
			return -EINVAL;

		if (is_10bit_colorformat(pix_fmts)) {
			i_vpr_h(inst,
				"%s: intra refresh is supported only for 8 bit\n",
				__func__);
			adjusted_value = 0;
			goto exit;
		}
	}

	/*
	 * BITRATE_MODE dependency is NOT common across all chipsets.
	 * Hence, do not return error if not specified as one of the parent.
	 */
	if (is_parent_available(inst, IR_PERIOD, BITRATE_MODE, __func__) &&
		inst->hfi_rc_type != HFI_RC_CBR_CFR &&
		inst->hfi_rc_type != HFI_RC_CBR_VFR)
		adjusted_value = 0;

exit:
	msm_vidc_update_cap_value(inst, IR_PERIOD,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_delta_based_rc(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[TIME_DELTA_BASED_RC].value;

	if (msm_vidc_get_parent_value(inst, TIME_DELTA_BASED_RC,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	if (rc_type == HFI_RC_OFF ||
		rc_type == HFI_RC_CQ)
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, TIME_DELTA_BASED_RC,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_output_order(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	struct msm_vidc_inst_capability *capability;
	s32 tn_mode = -1, display_delay = -1, display_delay_enable = -1;
	u32 adjusted_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[OUTPUT_ORDER].value;

	if (msm_vidc_get_parent_value(inst, OUTPUT_ORDER, THUMBNAIL_MODE,
			&tn_mode, __func__) ||
		msm_vidc_get_parent_value(inst, OUTPUT_ORDER, DISPLAY_DELAY,
			&display_delay, __func__) ||
		msm_vidc_get_parent_value(inst, OUTPUT_ORDER, DISPLAY_DELAY_ENABLE,
			&display_delay_enable, __func__))
		return -EINVAL;

	if (tn_mode || (display_delay_enable && !display_delay))
		adjusted_value = 1;

	msm_vidc_update_cap_value(inst, OUTPUT_ORDER,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_input_buf_host_max_count(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	struct msm_vidc_inst_capability *capability;
	u32 adjusted_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[INPUT_BUF_HOST_MAX_COUNT].value;

	if (msm_vidc_is_super_buffer(inst) || is_image_session(inst))
		adjusted_value = DEFAULT_MAX_HOST_BURST_BUF_COUNT;

	msm_vidc_update_cap_value(inst, INPUT_BUF_HOST_MAX_COUNT,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_output_buf_host_max_count(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	struct msm_vidc_inst_capability *capability;
	u32 adjusted_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[OUTPUT_BUF_HOST_MAX_COUNT].value;

	if (msm_vidc_is_super_buffer(inst) || is_image_session(inst) ||
		is_enc_slice_delivery_mode(inst))
		adjusted_value = DEFAULT_MAX_HOST_BURST_BUF_COUNT;

	msm_vidc_update_cap_value(inst, OUTPUT_BUF_HOST_MAX_COUNT,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_transform_8x8(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 profile = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[TRANSFORM_8X8].value;

	if (inst->codec != MSM_VIDC_H264) {
		i_vpr_e(inst,
			"%s: incorrect entry in database. fix the database\n",
			__func__);
		return 0;
	}

	if (msm_vidc_get_parent_value(inst, TRANSFORM_8X8,
		PROFILE, &profile, __func__))
		return -EINVAL;

	if (profile != V4L2_MPEG_VIDEO_H264_PROFILE_HIGH &&
		profile != V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH)
		adjusted_value = V4L2_MPEG_MSM_VIDC_DISABLE;

	msm_vidc_update_cap_value(inst, TRANSFORM_8X8,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_chroma_qp_index_offset(void *instance,
	struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[CHROMA_QP_INDEX_OFFSET].value;

	if (adjusted_value != MIN_CHROMA_QP_OFFSET)
		adjusted_value = MAX_CHROMA_QP_OFFSET;

	msm_vidc_update_cap_value(inst, CHROMA_QP_INDEX_OFFSET,
		adjusted_value, __func__);

	return 0;
}

static bool msm_vidc_check_all_layer_bitrate_set(struct msm_vidc_inst *inst)
{
	bool layer_bitrate_set = true;
	u32 cap_id = 0, i, enh_layer_count;
	u32 layer_br_caps[6] = {L0_BR, L1_BR, L2_BR, L3_BR, L4_BR, L5_BR};

	enh_layer_count = inst->capabilities->cap[ENH_LAYER_COUNT].value;

	for (i = 0; i <= enh_layer_count; i++) {
		if (i >= ARRAY_SIZE(layer_br_caps))
			break;
		cap_id = layer_br_caps[i];
		if (!(inst->capabilities->cap[cap_id].flags & CAP_FLAG_CLIENT_SET)) {
			layer_bitrate_set = false;
			break;
		}
	}

	return layer_bitrate_set;
}

static u32 msm_vidc_get_cumulative_bitrate(struct msm_vidc_inst *inst)
{
	int i;
	u32 cap_id = 0;
	u32 cumulative_br = 0;
	s32 enh_layer_count;
	u32 layer_br_caps[6] = {L0_BR, L1_BR, L2_BR, L3_BR, L4_BR, L5_BR};

	enh_layer_count = inst->capabilities->cap[ENH_LAYER_COUNT].value;

	for (i = 0; i <= enh_layer_count; i++) {
		if (i >= ARRAY_SIZE(layer_br_caps))
			break;
		cap_id = layer_br_caps[i];
		cumulative_br += inst->capabilities->cap[cap_id].value;
	}

	return cumulative_br;
}

int msm_vidc_adjust_slice_count(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	struct msm_vidc_inst_capability *capability;
	struct v4l2_format *output_fmt;
	s32 adjusted_value, rc_type = -1, slice_mode, all_intra, enh_layer_count = 0;
	u32 slice_val, mbpf = 0, mbps = 0, max_mbpf = 0, max_mbps = 0, bitrate = 0;
	u32 update_cap, max_avg_slicesize, output_width, output_height;
	u32 min_width, min_height, max_width, max_height, fps;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	slice_mode = ctrl ? ctrl->val :
		capability->cap[SLICE_MODE].value;

	if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE)
		return 0;

	if (msm_vidc_get_parent_value(inst, SLICE_MODE,
		BITRATE_MODE, &rc_type, __func__) ||
		msm_vidc_get_parent_value(inst, SLICE_MODE,
		ALL_INTRA, &all_intra, __func__) ||
		msm_vidc_get_parent_value(inst, SLICE_MODE,
		ENH_LAYER_COUNT, &enh_layer_count, __func__))
		return -EINVAL;

	if (capability->cap[BIT_RATE].flags & CAP_FLAG_CLIENT_SET) {
		bitrate = capability->cap[BIT_RATE].value;
	} else if (msm_vidc_check_all_layer_bitrate_set(inst)) {
		bitrate = msm_vidc_get_cumulative_bitrate(inst);
	} else {
		adjusted_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE;
		update_cap = SLICE_MODE;
		i_vpr_h(inst,
			"%s: client did not set bitrate & layerwise bitrates\n",
			__func__);
		goto exit;
	}

	fps = capability->cap[FRAME_RATE].value >> 16;
	if (fps > MAX_SLICES_FRAME_RATE ||
		(rc_type != HFI_RC_OFF &&
		rc_type != HFI_RC_CBR_CFR &&
		rc_type != HFI_RC_CBR_VFR &&
		rc_type != HFI_RC_VBR_CFR) ||
		all_intra) {
		adjusted_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE;
		update_cap = SLICE_MODE;
		i_vpr_h(inst,
			"%s: slice unsupported, fps: %u, rc_type: %#x, all_intra %d\n",
			__func__, fps, rc_type, all_intra);
		goto exit;
	}

	output_fmt = &inst->fmts[OUTPUT_PORT];
	output_width = output_fmt->fmt.pix_mp.width;
	output_height = output_fmt->fmt.pix_mp.height;

	max_width = (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) ?
		MAX_MB_SLICE_WIDTH : MAX_BYTES_SLICE_WIDTH;
	max_height = (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) ?
		MAX_MB_SLICE_HEIGHT : MAX_BYTES_SLICE_HEIGHT;
	min_width = (inst->codec == MSM_VIDC_HEVC) ?
		MIN_HEVC_SLICE_WIDTH : MIN_AVC_SLICE_WIDTH;
	min_height = MIN_SLICE_HEIGHT;

	/*
	 * For V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB:
	 * 	- width >= 384 and height >= 128
	 * 	- width and height <= 4096
	 * For V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES:
	 * 	- width >= 192 and height >= 128
	 * 	- width and height <= 1920
	 */
	if (output_width < min_width || output_height < min_height ||
		output_width > max_width || output_height > max_width) {
		adjusted_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE;
		update_cap = SLICE_MODE;
		i_vpr_h(inst,
			"%s: slice unsupported, codec: %#x wxh: [%dx%d]\n",
			__func__, inst->codec, output_width, output_height);
		goto exit;
	}

	mbpf = NUM_MBS_PER_FRAME(output_height, output_width);
	mbps = NUM_MBS_PER_SEC(output_height, output_width, fps);
	max_mbpf = NUM_MBS_PER_FRAME(max_height, max_width);
	max_mbps = NUM_MBS_PER_SEC(max_height, max_width, MAX_SLICES_FRAME_RATE);

	if (mbpf > max_mbpf || mbps > max_mbps) {
		adjusted_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE;
		update_cap = SLICE_MODE;
		i_vpr_h(inst,
			"%s: Unsupported, mbpf[%u] > max[%u], mbps[%u] > max[%u]\n",
			__func__, mbpf, max_mbpf, mbps, max_mbps);
		goto exit;
	}

	if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) {
		update_cap = SLICE_MAX_MB;
		slice_val = capability->cap[SLICE_MAX_MB].value;
		slice_val = max(slice_val, mbpf / MAX_SLICES_PER_FRAME);
	} else {
		slice_val = capability->cap[SLICE_MAX_BYTES].value;
		update_cap = SLICE_MAX_BYTES;
		if (rc_type != HFI_RC_OFF) {
			max_avg_slicesize = ((bitrate / fps) / 8) /
				MAX_SLICES_PER_FRAME;
			slice_val = max(slice_val, max_avg_slicesize);
		}
	}
	adjusted_value = slice_val;

exit:
	msm_vidc_update_cap_value(inst, update_cap,
		adjusted_value, __func__);

	return 0;
}

static int msm_vidc_adjust_static_layer_count_and_type(struct msm_vidc_inst *inst,
	s32 layer_count)
{
	bool hb_requested = false;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!layer_count) {
		i_vpr_h(inst, "client not enabled layer encoding\n");
		goto exit;
	}

	if (inst->hfi_rc_type == HFI_RC_CQ) {
		i_vpr_h(inst, "rc type is CQ, disabling layer encoding\n");
		layer_count = 0;
		goto exit;
	}

	if (inst->codec == MSM_VIDC_H264) {
		if (!inst->capabilities->cap[LAYER_ENABLE].value) {
			layer_count = 0;
			goto exit;
		}

		hb_requested = (inst->capabilities->cap[LAYER_TYPE].value ==
				V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_B) ?
				true : false;
	} else if (inst->codec == MSM_VIDC_HEVC) {
		hb_requested = (inst->capabilities->cap[LAYER_TYPE].value ==
				V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B) ?
				true : false;
	}

	if (hb_requested && inst->hfi_rc_type != HFI_RC_VBR_CFR) {
		i_vpr_h(inst,
			"%s: HB layer encoding is supported for VBR rc only\n",
			__func__);
		layer_count = 0;
		goto exit;
	}

	if (!is_meta_tx_inp_enabled(inst, META_EVA_STATS) &&
		hb_requested && (layer_count > 1)) {
		layer_count = 1;
		i_vpr_h(inst,
			"%s: cvp disable supports only one enh layer HB\n",
			__func__);
	}

	/* decide hfi layer type */
	if (hb_requested) {
		inst->hfi_layer_type = HFI_HIER_B;
	} else {
		/* HP requested */
		inst->hfi_layer_type = HFI_HIER_P_SLIDING_WINDOW;
		if (inst->codec == MSM_VIDC_H264 &&
			inst->hfi_rc_type == HFI_RC_VBR_CFR)
			inst->hfi_layer_type = HFI_HIER_P_HYBRID_LTR;
	}

	/* sanitize layer count based on layer type and codec, and rc type */
	if (inst->hfi_layer_type == HFI_HIER_B) {
		if (layer_count > MAX_ENH_LAYER_HB)
			layer_count = MAX_ENH_LAYER_HB;
	} else if (inst->hfi_layer_type == HFI_HIER_P_HYBRID_LTR) {
		if (layer_count > MAX_AVC_ENH_LAYER_HYBRID_HP)
			layer_count = MAX_AVC_ENH_LAYER_HYBRID_HP;
	} else if (inst->hfi_layer_type == HFI_HIER_P_SLIDING_WINDOW) {
		if (inst->codec == MSM_VIDC_H264) {
			if (layer_count > MAX_AVC_ENH_LAYER_SLIDING_WINDOW)
				layer_count = MAX_AVC_ENH_LAYER_SLIDING_WINDOW;
		} else if (inst->codec == MSM_VIDC_HEVC) {
			if (inst->hfi_rc_type == HFI_RC_VBR_CFR) {
				if (layer_count > MAX_HEVC_VBR_ENH_LAYER_SLIDING_WINDOW)
					layer_count = MAX_HEVC_VBR_ENH_LAYER_SLIDING_WINDOW;
			} else {
				if (layer_count > MAX_HEVC_NON_VBR_ENH_LAYER_SLIDING_WINDOW)
					layer_count = MAX_HEVC_NON_VBR_ENH_LAYER_SLIDING_WINDOW;
			}
		}
	}

exit:
	msm_vidc_update_cap_value(inst, ENH_LAYER_COUNT,
		layer_count, __func__);
	inst->capabilities->cap[ENH_LAYER_COUNT].max = layer_count;
	return 0;
}

int msm_vidc_adjust_layer_count(void *instance, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst_capability *capability;
	s32 client_layer_count;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	client_layer_count = ctrl ? ctrl->val :
		capability->cap[ENH_LAYER_COUNT].value;

	if (!is_parent_available(inst, ENH_LAYER_COUNT,
		BITRATE_MODE, __func__) ||
		!is_parent_available(inst, ENH_LAYER_COUNT,
		META_EVA_STATS, __func__))
		return -EINVAL;

	if (!inst->bufq[OUTPUT_PORT].vb2q->streaming) {
		rc = msm_vidc_adjust_static_layer_count_and_type(inst,
			client_layer_count);
		if (rc)
			goto exit;
	} else {
		if (inst->hfi_layer_type == HFI_HIER_P_HYBRID_LTR ||
			inst->hfi_layer_type == HFI_HIER_P_SLIDING_WINDOW) {
			/* dynamic layer count change is only supported for HP */
			if (client_layer_count >
				inst->capabilities->cap[ENH_LAYER_COUNT].max)
				client_layer_count =
					inst->capabilities->cap[ENH_LAYER_COUNT].max;

			msm_vidc_update_cap_value(inst, ENH_LAYER_COUNT,
				client_layer_count, __func__);
		}
	}

exit:
	return rc;
}

int msm_vidc_adjust_gop_size(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	s32 adjusted_value, enh_layer_count = -1;
	u32 min_gop_size, num_subgops;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[GOP_SIZE].value;

	if (msm_vidc_get_parent_value(inst, GOP_SIZE,
		ENH_LAYER_COUNT, &enh_layer_count, __func__))
		return -EINVAL;

	if (!enh_layer_count)
		goto exit;

	/*
	 * Layer encoding needs GOP size to be multiple of subgop size
	 * And subgop size is 2 ^ number of enhancement layers.
	 */

	/* v4l2 layer count is the number of enhancement layers */
	min_gop_size = 1 << enh_layer_count;
	num_subgops = (adjusted_value + (min_gop_size >> 1)) /
			min_gop_size;
	if (num_subgops)
		adjusted_value = num_subgops * min_gop_size;
	else
		adjusted_value = min_gop_size;

exit:
	msm_vidc_update_cap_value(inst, GOP_SIZE, adjusted_value, __func__);
	return 0;
}

int msm_vidc_adjust_b_frame(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	s32 adjusted_value, enh_layer_count = -1;
	const u32 max_bframe_size = 7;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[B_FRAME].value;

	if (msm_vidc_get_parent_value(inst, B_FRAME,
		ENH_LAYER_COUNT, &enh_layer_count, __func__))
		return -EINVAL;

	if (!enh_layer_count || inst->hfi_layer_type != HFI_HIER_B) {
		adjusted_value = 0;
		goto exit;
	}

	adjusted_value = (1 << enh_layer_count) - 1;
	/* Allowed Bframe values are 0, 1, 3, 7 */
	if (adjusted_value > max_bframe_size)
		adjusted_value = max_bframe_size;

exit:
	msm_vidc_update_cap_value(inst, B_FRAME, adjusted_value, __func__);
	return 0;
}

int msm_vidc_adjust_bitrate(void *instance, struct v4l2_ctrl *ctrl)
{
	int i, rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value, enh_layer_count;
	u32 cumulative_bitrate = 0, cap_id = 0, cap_value = 0;
	u32 layer_br_caps[6] = {L0_BR, L1_BR, L2_BR, L3_BR, L4_BR, L5_BR};
	u32 max_bitrate = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	/* ignore layer bitrate when total bitrate is set */
	if (capability->cap[BIT_RATE].flags & CAP_FLAG_CLIENT_SET) {
		/*
		 * For static case, ctrl is null.
		 * For dynamic case, only BIT_RATE cap uses this adjust function.
		 * Hence, no need to check for ctrl id to be BIT_RATE control, and not
		 * any of layer bitrate controls.
		 */
		adjusted_value = ctrl ? ctrl->val : capability->cap[BIT_RATE].value;
		msm_vidc_update_cap_value(inst, BIT_RATE, adjusted_value, __func__);

		return 0;
	}

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	if (msm_vidc_get_parent_value(inst, BIT_RATE,
		ENH_LAYER_COUNT, &enh_layer_count, __func__))
		return -EINVAL;

	/* get max bit rate for current session config*/
	max_bitrate = msm_vidc_get_max_bitrate(inst);
	if (inst->capabilities->cap[BIT_RATE].value > max_bitrate)
		msm_vidc_update_cap_value(inst, BIT_RATE, max_bitrate, __func__);

	/*
	 * ENH_LAYER_COUNT cap max is positive only if
	 * layer encoding is enabled during streamon.
	 */
	if (capability->cap[ENH_LAYER_COUNT].max) {
		if (!msm_vidc_check_all_layer_bitrate_set(inst)) {
			i_vpr_h(inst,
				"%s: client did not set all layer bitrates\n",
				__func__);
			return 0;
		}

		cumulative_bitrate = msm_vidc_get_cumulative_bitrate(inst);

		/* cap layer bitrates to max supported bitrate */
		if (cumulative_bitrate > max_bitrate) {
			u32 decrement_in_value = 0;
			u32 decrement_in_percent = ((cumulative_bitrate - max_bitrate) * 100) /
				max_bitrate;

			cumulative_bitrate = 0;
			for (i = 0; i <= enh_layer_count; i++) {
				if (i >= ARRAY_SIZE(layer_br_caps))
					break;
				cap_id = layer_br_caps[i];
				cap_value = inst->capabilities->cap[cap_id].value;

				decrement_in_value = (cap_value *
					decrement_in_percent) / 100;
				cumulative_bitrate += (cap_value - decrement_in_value);

				/*
				 * cap value for the L*_BR is changed. Hence, update cap,
				 * and add to FW_LIST to set new values to firmware.
				 */
				msm_vidc_update_cap_value(inst, cap_id,
					(cap_value - decrement_in_value), __func__);
			}
		}

		i_vpr_h(inst,
			"%s: update BIT_RATE with cumulative bitrate\n",
			__func__);
		msm_vidc_update_cap_value(inst, BIT_RATE,
			cumulative_bitrate, __func__);
	}

	return rc;
}

int msm_vidc_adjust_dynamic_layer_bitrate(void *instance, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	struct msm_vidc_inst_capability *capability;
	u32 cumulative_bitrate = 0;
	u32 client_set_cap_id = INST_CAP_NONE;
	u32 old_br = 0, new_br = 0, exceeded_br = 0;
	s32 max_bitrate;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (!ctrl)
		return 0;

	/* ignore layer bitrate when total bitrate is set */
	if (capability->cap[BIT_RATE].flags & CAP_FLAG_CLIENT_SET)
		return 0;

	if (!inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	/*
	 * ENH_LAYER_COUNT cap max is positive only if
	 * layer encoding is enabled during streamon.
	 */
	if (!capability->cap[ENH_LAYER_COUNT].max) {
		i_vpr_e(inst, "%s: layers not enabled\n", __func__);
		return -EINVAL;
	}

	if (!msm_vidc_check_all_layer_bitrate_set(inst)) {
		i_vpr_h(inst,
			"%s: client did not set all layer bitrates\n",
			__func__);
		return 0;
	}

	client_set_cap_id = msm_vidc_get_cap_id(inst, ctrl->id);
	if (client_set_cap_id == INST_CAP_NONE) {
		i_vpr_e(inst, "%s: could not find cap_id for ctrl %s\n",
			__func__, ctrl->name);
		return -EINVAL;
	}

	cumulative_bitrate = msm_vidc_get_cumulative_bitrate(inst);
	max_bitrate = inst->capabilities->cap[BIT_RATE].max;
	old_br = capability->cap[client_set_cap_id].value;
	new_br = ctrl->val;

	/*
	 * new bitrate is not supposed to cause cumulative bitrate to
	 * exceed max supported bitrate
	 */

	if ((cumulative_bitrate - old_br + new_br) > max_bitrate) {
		/* adjust new bitrate */
		exceeded_br = (cumulative_bitrate - old_br + new_br) - max_bitrate;
		new_br = ctrl->val - exceeded_br;
	}
	msm_vidc_update_cap_value(inst, client_set_cap_id, new_br, __func__);

	/* adjust totol bitrate cap */
	i_vpr_h(inst,
		"%s: update BIT_RATE with cumulative bitrate\n",
		__func__);
	msm_vidc_update_cap_value(inst, BIT_RATE,
		msm_vidc_get_cumulative_bitrate(inst), __func__);

	return rc;
}

int msm_vidc_adjust_peak_bitrate(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1, bitrate = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[PEAK_BITRATE].value;

	if (msm_vidc_get_parent_value(inst, PEAK_BITRATE,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	if (rc_type != HFI_RC_CBR_CFR &&
		rc_type != HFI_RC_CBR_VFR)
		return 0;

	if (msm_vidc_get_parent_value(inst, PEAK_BITRATE,
		BIT_RATE, &bitrate, __func__))
		return -EINVAL;

	/* Peak Bitrate should be larger than or equal to avg bitrate */
	if (capability->cap[PEAK_BITRATE].flags & CAP_FLAG_CLIENT_SET) {
		if (adjusted_value < bitrate)
			adjusted_value = bitrate;
	} else {
		adjusted_value = capability->cap[BIT_RATE].value;
	}

	msm_vidc_update_cap_value(inst, PEAK_BITRATE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_hevc_min_qp(void *instance, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (ctrl)
		msm_vidc_update_cap_value(inst, MIN_FRAME_QP,
			ctrl->val, __func__);

	rc = msm_vidc_adjust_hevc_qp(inst, MIN_FRAME_QP);

	return rc;
}

int msm_vidc_adjust_hevc_max_qp(void *instance, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (ctrl)
		msm_vidc_update_cap_value(inst, MAX_FRAME_QP,
			ctrl->val, __func__);

	rc = msm_vidc_adjust_hevc_qp(inst, MAX_FRAME_QP);

	return rc;
}

int msm_vidc_adjust_hevc_i_frame_qp(void *instance, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (ctrl)
		msm_vidc_update_cap_value(inst, I_FRAME_QP,
			ctrl->val, __func__);

	rc = msm_vidc_adjust_hevc_qp(inst, I_FRAME_QP);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_adjust_hevc_p_frame_qp(void *instance, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (ctrl)
		msm_vidc_update_cap_value(inst, P_FRAME_QP,
			ctrl->val, __func__);

	rc = msm_vidc_adjust_hevc_qp(inst, P_FRAME_QP);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_adjust_hevc_b_frame_qp(void *instance, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (ctrl)
		msm_vidc_update_cap_value(inst, B_FRAME_QP,
			ctrl->val, __func__);

	rc = msm_vidc_adjust_hevc_qp(inst, B_FRAME_QP);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_adjust_blur_type(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1, roi_enable = -1;
	s32 pix_fmts = -1, min_quality = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[BLUR_TYPES].value;

	if (adjusted_value == VIDC_BLUR_NONE)
		return 0;

	if (msm_vidc_get_parent_value(inst, BLUR_TYPES, BITRATE_MODE,
		&rc_type, __func__) ||
		msm_vidc_get_parent_value(inst, BLUR_TYPES, PIX_FMTS,
		&pix_fmts, __func__) ||
		msm_vidc_get_parent_value(inst, BLUR_TYPES, MIN_QUALITY,
		&min_quality, __func__) ||
		msm_vidc_get_parent_value(inst, BLUR_TYPES, META_ROI_INFO,
		&roi_enable, __func__))
		return -EINVAL;

	if (adjusted_value == VIDC_BLUR_EXTERNAL) {
		if (is_scaling_enabled(inst) || min_quality) {
			adjusted_value = VIDC_BLUR_NONE;
		}
	} else if (adjusted_value == VIDC_BLUR_ADAPTIVE) {
		if (is_scaling_enabled(inst) || min_quality ||
			(rc_type != HFI_RC_VBR_CFR &&
			rc_type != HFI_RC_CBR_CFR &&
			rc_type != HFI_RC_CBR_VFR) ||
			is_10bit_colorformat(pix_fmts) || roi_enable) {
			adjusted_value = VIDC_BLUR_NONE;
		}
	}

	msm_vidc_update_cap_value(inst, BLUR_TYPES,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_all_intra(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 gop_size = -1, bframe = -1;
	u32 width, height, fps, mbps, max_mbps;

	if (!inst || !inst->capabilities || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = capability->cap[ALL_INTRA].value;

	if (msm_vidc_get_parent_value(inst, ALL_INTRA, GOP_SIZE,
		&gop_size, __func__) ||
		msm_vidc_get_parent_value(inst, ALL_INTRA, B_FRAME,
		&bframe, __func__))
		return -EINVAL;

	width = inst->crop.width;
	height = inst->crop.height;
	fps =  msm_vidc_get_fps(inst);
	mbps = NUM_MBS_PER_SEC(height, width, fps);
	core = inst->core;
	max_mbps = core->capabilities[MAX_MBPS_ALL_INTRA].value;

	if (mbps > max_mbps) {
		adjusted_value = 0;
		i_vpr_h(inst, "%s: mbps %d exceeds max supported mbps %d\n",
			__func__, mbps, max_mbps);
		goto exit;
	}

	if (!gop_size && !bframe)
		adjusted_value = 1;

exit:
	msm_vidc_update_cap_value(inst, ALL_INTRA,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_blur_resolution(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 blur_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[BLUR_RESOLUTION].value;

	if (msm_vidc_get_parent_value(inst, BLUR_RESOLUTION, BLUR_TYPES,
		&blur_type, __func__))
		return -EINVAL;

	if (blur_type != VIDC_BLUR_EXTERNAL)
		return 0;

	msm_vidc_update_cap_value(inst, BLUR_RESOLUTION,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_brs(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1, layer_enabled = -1, layer_type = -1;
	bool hp_requested = false;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[CONTENT_ADAPTIVE_CODING].value;

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	if (msm_vidc_get_parent_value(inst, CONTENT_ADAPTIVE_CODING,
		BITRATE_MODE, &rc_type, __func__) ||
		msm_vidc_get_parent_value(inst, CONTENT_ADAPTIVE_CODING,
		LAYER_ENABLE, &layer_enabled, __func__) ||
		msm_vidc_get_parent_value(inst, CONTENT_ADAPTIVE_CODING,
		LAYER_TYPE, &layer_type, __func__))
		return -EINVAL;

	/*
	 * -BRS is supported only for VBR rc type.
	 *  Hence, do not adjust or set to firmware for non VBR rc's
	 * -If HP is enabled then BRS is not allowed.
	 */
	if (rc_type != HFI_RC_VBR_CFR) {
		adjusted_value = 0;
		goto adjust;
	}

	if (inst->codec == MSM_VIDC_H264) {
		layer_type = V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P;
	} else if (inst->codec == MSM_VIDC_HEVC) {
		layer_type = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P;
	}
	hp_requested = (inst->capabilities->cap[LAYER_TYPE].value == layer_type);

	/*
	 * Disable BRS in case of HP encoding
	 * Hence set adjust value to 0.
	 */
	if (layer_enabled == 1 && hp_requested) {
		adjusted_value = 0;
		goto adjust;
	}

adjust:
	msm_vidc_update_cap_value(inst, CONTENT_ADAPTIVE_CODING,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_bitrate_boost(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 min_quality = -1, rc_type = -1;
	u32 max_bitrate = 0, bitrate = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[BITRATE_BOOST].value;

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	if (msm_vidc_get_parent_value(inst, BITRATE_BOOST,
		MIN_QUALITY, &min_quality, __func__) ||
		msm_vidc_get_parent_value(inst, BITRATE_BOOST,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	/*
	 * Bitrate Boost are supported only for VBR rc type.
	 * Hence, do not adjust or set to firmware for non VBR rc's
	 */
	if (rc_type != HFI_RC_VBR_CFR) {
		adjusted_value = 0;
		goto adjust;
	}

	if (min_quality) {
		adjusted_value = MAX_BITRATE_BOOST;
		goto adjust;
	}

	max_bitrate = msm_vidc_get_max_bitrate(inst);
	bitrate = inst->capabilities->cap[BIT_RATE].value;
	if (adjusted_value) {
		if ((bitrate + bitrate / (100 / adjusted_value)) > max_bitrate) {
			i_vpr_h(inst,
				"%s: bitrate %d is beyond max bitrate %d, remove bitrate boost\n",
				__func__, max_bitrate, bitrate);
			adjusted_value = 0;
		}
	}

adjust:
	msm_vidc_update_cap_value(inst, BITRATE_BOOST,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_min_quality(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 roi_enable = -1, rc_type = -1, enh_layer_count = -1, pix_fmts = -1;
	u32 width, height, frame_rate;
	struct v4l2_format *f;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[MIN_QUALITY].value;

	/*
	 * Although MIN_QUALITY is static, one of its parents,
	 * ENH_LAYER_COUNT is dynamic cap. Hence, dynamic call
	 * may be made for MIN_QUALITY via ENH_LAYER_COUNT.
	 * Therefore, below streaming check is required to avoid
	 * runtime modification of MIN_QUALITY.
	 */
	if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	if (msm_vidc_get_parent_value(inst, MIN_QUALITY,
		BITRATE_MODE, &rc_type, __func__) ||
		msm_vidc_get_parent_value(inst, MIN_QUALITY,
		META_ROI_INFO, &roi_enable, __func__) ||
		msm_vidc_get_parent_value(inst, MIN_QUALITY,
		ENH_LAYER_COUNT, &enh_layer_count, __func__))
		return -EINVAL;

	/*
	 * Min Quality is supported only for VBR rc type.
	 * Hence, do not adjust or set to firmware for non VBR rc's
	 */
	if (rc_type != HFI_RC_VBR_CFR) {
		adjusted_value = 0;
		goto update_and_exit;
	}

	frame_rate = inst->capabilities->cap[FRAME_RATE].value >> 16;
	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	/*
	 * VBR Min Quality not supported for:
	 * - HEVC 10bit
	 * - ROI support
	 * - HP encoding
	 * - External Blur
	 * - Resolution beyond 1080P
	 * (It will fall back to CQCAC 25% or 0% (CAC) or CQCAC-OFF)
	 */
	if (inst->codec == MSM_VIDC_HEVC) {
		if (msm_vidc_get_parent_value(inst, MIN_QUALITY,
			PIX_FMTS, &pix_fmts, __func__))
			return -EINVAL;

		if (is_10bit_colorformat(pix_fmts)) {
			i_vpr_h(inst,
				"%s: min quality is supported only for 8 bit\n",
				__func__);
			adjusted_value = 0;
			goto update_and_exit;
		}
	}

	if (res_is_greater_than(width, height, 1920, 1080)) {
		i_vpr_h(inst, "%s: unsupported res, wxh %ux%u\n",
			__func__, width, height);
		adjusted_value = 0;
		goto update_and_exit;
	}

	if (frame_rate > 60) {
		i_vpr_h(inst, "%s: unsupported fps %u\n",
			__func__, frame_rate);
		adjusted_value = 0;
		goto update_and_exit;
	}

	if (is_meta_tx_inp_enabled(inst, META_ROI_INFO)) {
		i_vpr_h(inst,
			"%s: min quality not supported with roi metadata\n",
			__func__);
		adjusted_value = 0;
		goto update_and_exit;
	}

	if (enh_layer_count > 0 && inst->hfi_layer_type != HFI_HIER_B) {
		i_vpr_h(inst,
			"%s: min quality not supported for HP encoding\n",
			__func__);
		adjusted_value = 0;
		goto update_and_exit;
	}

	/* Above conditions are met. Hence enable min quality */
	adjusted_value = MAX_SUPPORTED_MIN_QUALITY;

update_and_exit:
	msm_vidc_update_cap_value(inst, MIN_QUALITY,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_preprocess(void *instance, struct v4l2_ctrl *ctrl)
{
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 brs = -1, eva_status = -1;
	u32 width, height, frame_rate, operating_rate, max_fps;
	struct v4l2_format *f;

	if (!inst || !inst->capabilities || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	adjusted_value = inst->capabilities->cap[REQUEST_PREPROCESS].value;

	if (msm_vidc_get_parent_value(inst, REQUEST_PREPROCESS, CONTENT_ADAPTIVE_CODING,
		&brs, __func__) ||
		msm_vidc_get_parent_value(inst, REQUEST_PREPROCESS, META_EVA_STATS,
                &eva_status, __func__))
		return -EINVAL;

	width = inst->crop.width;
	height = inst->crop.height;
	frame_rate = msm_vidc_get_frame_rate(inst);;
	operating_rate = msm_vidc_get_operating_rate(inst);;

	max_fps = max(frame_rate, operating_rate);
	f= &inst->fmts[OUTPUT_PORT];

	/*
	 * enable preprocess if
	 * client did not enable EVA metadata statistics and
	 * BRS enabled and upto 4k @ 60 fps
	 */
	if (!is_meta_tx_inp_enabled(inst, META_EVA_STATS) &&
		brs == V4L2_MPEG_MSM_VIDC_ENABLE &&
		res_is_less_than_or_equal_to(width, height, 3840, 2160) &&
		max_fps <= 60)
		adjusted_value = 1;
	else
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, REQUEST_PREPROCESS,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_enc_lowlatency_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[LOWLATENCY_MODE].value;

	if (msm_vidc_get_parent_value(inst, LOWLATENCY_MODE, BITRATE_MODE,
		&rc_type, __func__))
		return -EINVAL;

	if (rc_type == HFI_RC_CBR_CFR ||
		rc_type == HFI_RC_CBR_VFR ||
		is_enc_slice_delivery_mode(inst))
		adjusted_value = 1;

	msm_vidc_update_cap_value(inst, LOWLATENCY_MODE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_lowlatency_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 outbuf_fence = V4L2_MPEG_VIDC_META_DISABLE;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[LOWLATENCY_MODE].value;

	if (msm_vidc_get_parent_value(inst, LOWLATENCY_MODE, META_OUTBUF_FENCE,
		&outbuf_fence, __func__))
		return -EINVAL;

	/* enable lowlatency if outbuf fence is enabled */
	if (outbuf_fence & V4L2_MPEG_VIDC_META_ENABLE &&
		outbuf_fence & V4L2_MPEG_VIDC_META_RX_INPUT)
		adjusted_value = 1;

	msm_vidc_update_cap_value(inst, LOWLATENCY_MODE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_session_priority(void *instance, struct v4l2_ctrl *ctrl)
{
	int adjusted_value;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;
	/*
	 * Priority handling
	 * Client will set 0 (realtime), 1+ (non-realtime)
	 * Driver adds NRT_PRIORITY_OFFSET (2) to clients non-realtime priority
	 * and hence PRIORITY values in the driver become 0, 3+.
	 * Driver may move decode realtime sessions to non-realtime by
	 * increasing priority by 1 to RT sessions in HW overloaded cases.
	 * So driver PRIORITY values can be 0, 1, 3+.
	 * When driver setting priority to firmware, driver adds
	 * FIRMWARE_PRIORITY_OFFSET (1) for all sessions except
	 * non-critical sessions. So finally firmware priority values ranges
	 * from 0 (Critical session), 1 (realtime session),
	 * 2+ (non-realtime session)
	 */
	if (ctrl) {
		/* add offset when client sets non-realtime */
		if (ctrl->val)
			adjusted_value = ctrl->val + NRT_PRIORITY_OFFSET;
		else
			adjusted_value = ctrl->val;
	} else {
		adjusted_value = capability->cap[PRIORITY].value;
	}

	msm_vidc_update_cap_value(inst, PRIORITY, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_roi_info(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1, pix_fmt = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[META_ROI_INFO].value;

	if (msm_vidc_get_parent_value(inst, META_ROI_INFO, BITRATE_MODE,
		&rc_type, __func__))
		return -EINVAL;

	if (msm_vidc_get_parent_value(inst, META_ROI_INFO, PIX_FMTS,
		&pix_fmt, __func__))
		return -EINVAL;

	if ((rc_type != HFI_RC_VBR_CFR && rc_type != HFI_RC_CBR_CFR
		&& rc_type != HFI_RC_CBR_VFR) || !is_8bit_colorformat(pix_fmt)
		|| is_scaling_enabled(inst) || is_rotation_90_or_270(inst))
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, META_ROI_INFO,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_frame_rate(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 adjusted_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (is_encode_session(inst)) {
		d_vpr_e("%s: adjust framerate invalid for enc\n", __func__);
		return -EINVAL;
	}

	capability = inst->capabilities;
	adjusted_value = ctrl ? ctrl->val : capability->cap[FRAME_RATE].value;
	msm_vidc_update_cap_value(inst, FRAME_RATE, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_operating_rate(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 adjusted_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (is_encode_session(inst)) {
		d_vpr_e("%s: adjust operating rate invalid for enc\n", __func__);
		return -EINVAL;
	}

	capability = inst->capabilities;
	adjusted_value = ctrl ? ctrl->val : capability->cap[OPERATING_RATE].value;
	msm_vidc_update_cap_value(inst, OPERATING_RATE, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_outbuf_fence(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 adjusted_value = 0;
	s32 picture_order = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[META_OUTBUF_FENCE].value;

	if (msm_vidc_get_parent_value(inst, META_OUTBUF_FENCE, OUTPUT_ORDER,
		&picture_order, __func__))
		return -EINVAL;

	if (picture_order == V4L2_MPEG_MSM_VIDC_DISABLE) {
		/* disable outbuf fence */
		adjusted_value = V4L2_MPEG_VIDC_META_DISABLE |
			V4L2_MPEG_VIDC_META_RX_INPUT;
	}

	msm_vidc_update_cap_value(inst, META_OUTBUF_FENCE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_slice_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 adjusted_value = 0;
	s32 low_latency = -1;
	s32 picture_order = -1;
	s32 outbuf_fence = V4L2_MPEG_VIDC_META_DISABLE;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[SLICE_DECODE].value;

	if (msm_vidc_get_parent_value(inst, SLICE_DECODE, LOWLATENCY_MODE,
		&low_latency, __func__) ||
	    msm_vidc_get_parent_value(inst, SLICE_DECODE, OUTPUT_ORDER,
		&picture_order, __func__) ||
	    msm_vidc_get_parent_value(inst, SLICE_DECODE, META_OUTBUF_FENCE,
		&outbuf_fence, __func__))
		return -EINVAL;

	if (!low_latency || !picture_order ||
	    !is_meta_rx_inp_enabled(inst, META_OUTBUF_FENCE))
		adjusted_value = V4L2_MPEG_MSM_VIDC_DISABLE;

	msm_vidc_update_cap_value(inst, SLICE_DECODE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_early_notify_enable(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 adjusted_value = 0;
	s32 low_latency = -1;
	s32 picture_order = -1;
	s32 outbuf_fence = V4L2_MPEG_VIDC_META_DISABLE;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[EARLY_NOTIFY_ENABLE].value;

	if (msm_vidc_get_parent_value(inst, EARLY_NOTIFY_ENABLE, LOWLATENCY_MODE,
		&low_latency, __func__) ||
	    msm_vidc_get_parent_value(inst, EARLY_NOTIFY_ENABLE, OUTPUT_ORDER,
		&picture_order, __func__) ||
	    msm_vidc_get_parent_value(inst, EARLY_NOTIFY_ENABLE, META_OUTBUF_FENCE,
		&outbuf_fence, __func__))
		return -EINVAL;

	if (!low_latency || !picture_order ||
	    !is_meta_rx_inp_enabled(inst, META_OUTBUF_FENCE))
		adjusted_value = V4L2_MPEG_MSM_VIDC_DISABLE;

	msm_vidc_update_cap_value(inst, EARLY_NOTIFY_ENABLE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_early_notify_line_count(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 early_notify = 0, adjusted_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[EARLY_NOTIFY_LINE_COUNT].value;

	if (msm_vidc_get_parent_value(inst, EARLY_NOTIFY_LINE_COUNT, EARLY_NOTIFY_ENABLE,
		&early_notify, __func__))
		return -EINVAL;

	/* check if early notify feature is enabled */
	if (!early_notify)
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, EARLY_NOTIFY_LINE_COUNT,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_early_notify_fence_count(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 height = 0, line_cnt = 0, adjusted_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[EARLY_NOTIFY_FENCE_COUNT].value;

	if (msm_vidc_get_parent_value(inst, EARLY_NOTIFY_FENCE_COUNT, EARLY_NOTIFY_LINE_COUNT,
		&line_cnt, __func__))
		return -EINVAL;

	if (!is_early_notify_enabled(inst)) {
		adjusted_value = 0;
		goto set_fence_count;
	}

	height = inst->fmts[INPUT_PORT].fmt.pix_mp.height;

	if (!line_cnt)
		adjusted_value = 1;
	else
		adjusted_value = (height % line_cnt == 0) ?
			(height/line_cnt) : (height/line_cnt + 1);

	if (adjusted_value > MAX_FENCE_COUNT) {
		i_vpr_e(inst, "%s: invalid fence count %d, line count %d\n",
			__func__, adjusted_value, line_cnt);
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	}

set_fence_count:
	msm_vidc_update_cap_value(inst, EARLY_NOTIFY_FENCE_COUNT,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_delivery_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	s32 slice_mode = -1;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (is_decode_session(inst))
		return 0;

	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[DELIVERY_MODE].value;

	if (msm_vidc_get_parent_value(inst, DELIVERY_MODE, SLICE_MODE,
		&slice_mode, __func__))
		return -EINVAL;

	/* Slice encode delivery mode is only supported for Max MB slice mode */
	if (slice_mode != V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) {
		if (inst->codec == MSM_VIDC_HEVC)
			adjusted_value = V4L2_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE_FRAME_BASED;
		else if (inst->codec == MSM_VIDC_H264)
			adjusted_value = V4L2_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE_FRAME_BASED;
	}

	msm_vidc_update_cap_value(inst, DELIVERY_MODE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_prepare_dependency_list(struct msm_vidc_inst *inst)
{
	struct list_head root_list, opt_list;
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst_cap *cap, *rcap;
	struct msm_vidc_inst_cap_entry *entry = NULL, *temp = NULL;
	bool root_visited[INST_CAP_MAX];
	bool opt_visited[INST_CAP_MAX];
	int tmp_count_total, tmp_count, num_nodes = 0;
	int i, rc = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (!list_empty(&inst->caps_list)) {
		i_vpr_h(inst, "%s: dependency list already prepared\n", __func__);
		return 0;
	}

	/* init local list and lookup table entries */
	INIT_LIST_HEAD(&root_list);
	INIT_LIST_HEAD(&opt_list);
	memset(&root_visited, 0, sizeof(root_visited));
	memset(&opt_visited, 0, sizeof(opt_visited));

	/* populate root nodes first */
	for (i = 1; i < INST_CAP_MAX; i++) {
		rcap = &capability->cap[i];
		if (!is_valid_cap(rcap))
			continue;

		/* sanitize cap value */
		if (i != rcap->cap_id) {
			i_vpr_e(inst, "%s: cap id mismatch. expected %s, actual %s\n",
				__func__, cap_name(i), cap_name(rcap->cap_id));
			rc = -EINVAL;
			goto error;
		}

		/* add all root nodes */
		if (is_root(rcap)) {
			rc = add_node(&root_list, rcap, root_visited);
			if (rc)
				goto error;
		} else {
			rc = add_node(&opt_list, rcap, opt_visited);
			if (rc)
				goto error;
		}
	}

	/* add all dependent parents */
	list_for_each_entry_safe(entry, temp, &root_list, list) {
		rcap = &capability->cap[entry->cap_id];
		/* skip leaf node */
		if (!has_childrens(rcap))
			continue;

		for (i = 0; i < MAX_CAP_CHILDREN; i++) {
			if (!rcap->children[i])
				break;

			if (!is_valid_cap_id(rcap->children[i]))
				continue;

			cap = &capability->cap[rcap->children[i]];
			if (!is_valid_cap(cap))
				continue;

			/**
			 * if child node is already part of root list
			 * then no need to add it again.
			 */
			if (root_visited[cap->cap_id])
				continue;

			/**
			 * if child node's all parents are already present in root list
			 * then add it to root list else remains in optional list.
			 */
			if (is_all_parents_visited(cap, root_visited)) {
				rc = swap_node(cap,
						&opt_list, opt_visited, &root_list, root_visited);
				if (rc)
					goto error;
			}
		}
	}

	/* find total optional list entries */
	list_for_each_entry(entry, &opt_list, list)
		num_nodes++;

	/* used for loop detection */
	tmp_count_total = num_nodes;
	tmp_count = num_nodes;

	/* sort final outstanding nodes */
	list_for_each_entry_safe(entry, temp, &opt_list, list) {
		/* initially remove entry from opt list */
		list_del_init(&entry->list);
		opt_visited[entry->cap_id] = false;
		tmp_count--;
		cap = &capability->cap[entry->cap_id];

		/**
		 * if all parents are visited then add this entry to
		 * root list else add it to the end of optional list.
		 */
		if (is_all_parents_visited(cap, root_visited)) {
			list_add_tail(&entry->list, &root_list);
			root_visited[entry->cap_id] = true;
			tmp_count_total--;
		} else {
			list_add_tail(&entry->list, &opt_list);
			opt_visited[entry->cap_id] = true;
		}

		/* detect loop */
		if (!tmp_count) {
			if (num_nodes == tmp_count_total) {
				i_vpr_e(inst, "%s: loop detected in subgraph %d\n",
					__func__, num_nodes);
				rc = -EINVAL;
				goto error;
			}
			num_nodes = tmp_count_total;
			tmp_count = tmp_count_total;
		}
	}

	/* expecting opt_list to be empty */
	if (!list_empty(&opt_list)) {
		i_vpr_e(inst, "%s: opt_list is not empty\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	/* move elements to &inst->caps_list from local */
	list_replace_init(&root_list, &inst->caps_list);

	return 0;
error:
	list_for_each_entry_safe(entry, temp, &opt_list, list) {
		i_vpr_e(inst, "%s: opt_list: %s\n", __func__, cap_name(entry->cap_id));
		list_del_init(&entry->list);
		msm_vidc_vmem_free((void **)&entry);
	}
	list_for_each_entry_safe(entry, temp, &root_list, list) {
		i_vpr_e(inst, "%s: root_list: %s\n", __func__, cap_name(entry->cap_id));
		list_del_init(&entry->list);
		msm_vidc_vmem_free((void **)&entry);
	}
	return rc;
}

/*
 * Loop over instance capabilities from caps_list
 * and call adjust and set function
 */
int msm_vidc_adjust_set_v4l2_properties(struct msm_vidc_inst *inst)
{
	struct msm_vidc_inst_cap_entry *entry = NULL, *temp = NULL;
	int rc = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	i_vpr_h(inst, "%s()\n", __func__);

	/* adjust all possible caps from caps_list */
	list_for_each_entry_safe(entry, temp, &inst->caps_list, list) {
		i_vpr_l(inst, "%s: cap: id %3u, name %s\n", __func__,
			entry->cap_id, cap_name(entry->cap_id));

		rc = msm_vidc_adjust_cap(inst, entry->cap_id, NULL, __func__);
		if (rc)
			return rc;
	}

	/* set all caps from caps_list */
	list_for_each_entry_safe(entry, temp, &inst->caps_list, list) {
		rc = msm_vidc_set_cap(inst, entry->cap_id, __func__);
		if (rc)
			return rc;
	}

	return rc;
}

int msm_vidc_set_header_mode(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	int header_mode, prepend_sps_pps;
	u32 hfi_value = 0;
	struct msm_vidc_inst_capability *capability;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	header_mode = capability->cap[cap_id].value;
	prepend_sps_pps = capability->cap[PREPEND_SPSPPS_TO_IDR].value;

	/* prioritize PREPEND_SPSPPS_TO_IDR mode over other header modes */
	if (prepend_sps_pps)
		hfi_value = HFI_SEQ_HEADER_PREFIX_WITH_SYNC_FRAME;
	else if (header_mode == V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME)
		hfi_value = HFI_SEQ_HEADER_JOINED_WITH_1ST_FRAME;
	else
		hfi_value = HFI_SEQ_HEADER_SEPERATE_FRAME;

	if (is_meta_rx_out_enabled(inst, META_SEQ_HDR_NAL))
		hfi_value |= HFI_SEQ_HEADER_METADATA;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_deblock_mode(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 alpha = 0, beta = 0;
	u32 lf_mode, hfi_value = 0, lf_offset = 6;
	struct msm_vidc_inst_capability *capability;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	rc = msm_vidc_v4l2_to_hfi_enum(inst, LF_MODE, &lf_mode);
	if (rc)
		return -EINVAL;

	beta = inst->capabilities->cap[LF_BETA].value + lf_offset;
	alpha = inst->capabilities->cap[LF_ALPHA].value + lf_offset;
	hfi_value = (alpha << 16) | (beta << 8) | lf_mode;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_constant_quality(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;
	s32 rc_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (msm_vidc_get_parent_value(inst, cap_id,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	if (rc_type != HFI_RC_CQ)
		return 0;

	hfi_value = inst->capabilities->cap[cap_id].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_vbr_related_properties(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;
	s32 rc_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (msm_vidc_get_parent_value(inst, cap_id,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	if (rc_type != HFI_RC_VBR_CFR)
		return 0;

	hfi_value = inst->capabilities->cap[cap_id].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_cbr_related_properties(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;
	s32 rc_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (msm_vidc_get_parent_value(inst, cap_id,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	if (rc_type != HFI_RC_CBR_VFR &&
		rc_type != HFI_RC_CBR_CFR)
		return 0;

	hfi_value = inst->capabilities->cap[cap_id].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_use_and_mark_ltr(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!inst->capabilities->cap[LTR_COUNT].value ||
		(inst->capabilities->cap[cap_id].value ==
			INVALID_DEFAULT_MARK_OR_USE_LTR)) {
		i_vpr_h(inst,
			"%s: LTR_COUNT: %d %s: %d, cap %s is not set\n",
			__func__, inst->capabilities->cap[LTR_COUNT].value,
			cap_name(cap_id),
			inst->capabilities->cap[cap_id].value,
			cap_name(cap_id));
		return 0;
	}

	hfi_value = inst->capabilities->cap[cap_id].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_min_qp(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	struct msm_vidc_inst_capability *capability;
	s32 i_frame_qp = 0, p_frame_qp = 0, b_frame_qp = 0, min_qp_enable = 0;
	u32 i_qp_enable = 0, p_qp_enable = 0, b_qp_enable = 0;
	u32 client_qp_enable = 0, hfi_value = 0, offset = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (capability->cap[MIN_FRAME_QP].flags & CAP_FLAG_CLIENT_SET)
		min_qp_enable = 1;

	if (min_qp_enable ||
		(capability->cap[I_FRAME_MIN_QP].flags & CAP_FLAG_CLIENT_SET))
		i_qp_enable = 1;
	if (min_qp_enable ||
		(capability->cap[P_FRAME_MIN_QP].flags & CAP_FLAG_CLIENT_SET))
		p_qp_enable = 1;
	if (min_qp_enable ||
		(capability->cap[B_FRAME_MIN_QP].flags & CAP_FLAG_CLIENT_SET))
		b_qp_enable = 1;

	client_qp_enable = i_qp_enable | p_qp_enable << 1 | b_qp_enable << 2;
	if (!client_qp_enable) {
		i_vpr_h(inst,
			"%s: client did not set min qp, cap %s is not set\n",
			__func__, cap_name(cap_id));
		return 0;
	}

	if (is_10bit_colorformat(capability->cap[PIX_FMTS].value))
		offset = 12;

	/*
	 * I_FRAME_MIN_QP, P_FRAME_MIN_QP, B_FRAME_MIN_QP,
	 * MIN_FRAME_QP caps have default value as MIN_QP_10BIT values.
	 * Hence, if client sets either one among MIN_FRAME_QP
	 * and (I_FRAME_MIN_QP or P_FRAME_MIN_QP or B_FRAME_MIN_QP),
	 * max of both caps will result into client set value.
	 */
	i_frame_qp = max(capability->cap[I_FRAME_MIN_QP].value,
			capability->cap[MIN_FRAME_QP].value) + offset;
	p_frame_qp = max(capability->cap[P_FRAME_MIN_QP].value,
			capability->cap[MIN_FRAME_QP].value) + offset;
	b_frame_qp = max(capability->cap[B_FRAME_MIN_QP].value,
			capability->cap[MIN_FRAME_QP].value) + offset;

	hfi_value = i_frame_qp | p_frame_qp << 8 | b_frame_qp << 16 |
		client_qp_enable << 24;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_max_qp(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	struct msm_vidc_inst_capability *capability;
	s32 i_frame_qp = 0, p_frame_qp = 0, b_frame_qp = 0, max_qp_enable = 0;
	u32 i_qp_enable = 0, p_qp_enable = 0, b_qp_enable = 0;
	u32 client_qp_enable = 0, hfi_value = 0, offset = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	if (capability->cap[MAX_FRAME_QP].flags & CAP_FLAG_CLIENT_SET)
		max_qp_enable = 1;

	if (max_qp_enable ||
		(capability->cap[I_FRAME_MAX_QP].flags & CAP_FLAG_CLIENT_SET))
		i_qp_enable = 1;
	if (max_qp_enable ||
		(capability->cap[P_FRAME_MAX_QP].flags & CAP_FLAG_CLIENT_SET))
		p_qp_enable = 1;
	if (max_qp_enable ||
		(capability->cap[B_FRAME_MAX_QP].flags & CAP_FLAG_CLIENT_SET))
		b_qp_enable = 1;

	client_qp_enable = i_qp_enable | p_qp_enable << 1 | b_qp_enable << 2;
	if (!client_qp_enable) {
		i_vpr_h(inst,
			"%s: client did not set max qp, cap %s is not set\n",
			__func__, cap_name(cap_id));
		return 0;
	}

	if (is_10bit_colorformat(capability->cap[PIX_FMTS].value))
		offset = 12;

	/*
	 * I_FRAME_MAX_QP, P_FRAME_MAX_QP, B_FRAME_MAX_QP,
	 * MAX_FRAME_QP caps have default value as MAX_QP values.
	 * Hence, if client sets either one among MAX_FRAME_QP
	 * and (I_FRAME_MAX_QP or P_FRAME_MAX_QP or B_FRAME_MAX_QP),
	 * min of both caps will result into client set value.
	 */
	i_frame_qp = min(capability->cap[I_FRAME_MAX_QP].value,
			capability->cap[MAX_FRAME_QP].value) + offset;
	p_frame_qp = min(capability->cap[P_FRAME_MAX_QP].value,
			capability->cap[MAX_FRAME_QP].value) + offset;
	b_frame_qp = min(capability->cap[B_FRAME_MAX_QP].value,
			capability->cap[MAX_FRAME_QP].value) + offset;

	hfi_value = i_frame_qp | p_frame_qp << 8 | b_frame_qp << 16 |
		client_qp_enable << 24;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_frame_qp(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	struct msm_vidc_inst_capability *capab;
	s32 i_frame_qp = 0, p_frame_qp = 0, b_frame_qp = 0;
	u32 i_qp_enable = 0, p_qp_enable = 0, b_qp_enable = 0;
	u32 client_qp_enable = 0, hfi_value = 0, offset = 0;
	s32 rc_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capab = inst->capabilities;

	if (msm_vidc_get_parent_value(inst, cap_id,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming) {
		if (rc_type != HFI_RC_OFF) {
			i_vpr_h(inst,
				"%s: dynamic qp not allowed for rc type %d\n",
				__func__, rc_type);
			return 0;
		}
	}

	if (rc_type == HFI_RC_OFF) {
		/* Mandatorily set for rc off case */
		i_qp_enable = p_qp_enable = b_qp_enable = 1;
	} else {
		/* Set only if client has set for NON rc off case */
		if (capab->cap[I_FRAME_QP].flags & CAP_FLAG_CLIENT_SET)
			i_qp_enable = 1;
		if (capab->cap[P_FRAME_QP].flags & CAP_FLAG_CLIENT_SET)
			p_qp_enable = 1;
		if (capab->cap[B_FRAME_QP].flags & CAP_FLAG_CLIENT_SET)
			b_qp_enable = 1;
	}

	client_qp_enable = i_qp_enable | p_qp_enable << 1 | b_qp_enable << 2;
	if (!client_qp_enable) {
		i_vpr_h(inst,
			"%s: client did not set frame qp, cap %s is not set\n",
			__func__, cap_name(cap_id));
		return 0;
	}

	if (is_10bit_colorformat(capab->cap[PIX_FMTS].value))
		offset = 12;

	i_frame_qp = capab->cap[I_FRAME_QP].value + offset;
	p_frame_qp = capab->cap[P_FRAME_QP].value + offset;
	b_frame_qp = capab->cap[B_FRAME_QP].value + offset;

	hfi_value = i_frame_qp | p_frame_qp << 8 | b_frame_qp << 16 |
		client_qp_enable << 24;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_req_sync_frame(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	s32 prepend_spspps;
	u32 hfi_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	prepend_spspps = inst->capabilities->cap[PREPEND_SPSPPS_TO_IDR].value;
	if (prepend_spspps)
		hfi_value = HFI_SYNC_FRAME_REQUEST_WITH_PREFIX_SEQ_HDR;
	else
		hfi_value = HFI_SYNC_FRAME_REQUEST_WITHOUT_SEQ_HDR;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_chroma_qp_index_offset(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0, chroma_qp_offset_mode = 0, chroma_qp = 0;
	u32 offset = 12;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->capabilities->cap[cap_id].flags & CAP_FLAG_CLIENT_SET)
		chroma_qp_offset_mode = HFI_FIXED_CHROMAQP_OFFSET;
	else
		chroma_qp_offset_mode = HFI_ADAPTIVE_CHROMAQP_OFFSET;

	chroma_qp = inst->capabilities->cap[cap_id].value + offset;
	hfi_value = chroma_qp_offset_mode | chroma_qp << 8 | chroma_qp << 16 ;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_slice_count(void* instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst* inst = (struct msm_vidc_inst*)instance;
	s32 slice_mode = -1;
	u32 hfi_value = 0, set_cap_id = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	slice_mode = inst->capabilities->cap[SLICE_MODE].value;

	if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE) {
		i_vpr_h(inst, "%s: slice mode is: %u, ignore setting to fw\n",
			__func__, slice_mode);
		return 0;
	}
	if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) {
		hfi_value = (inst->codec == MSM_VIDC_HEVC) ?
			((inst->capabilities->cap[SLICE_MAX_MB].value + 3) / 4) :
			inst->capabilities->cap[SLICE_MAX_MB].value;
		set_cap_id = SLICE_MAX_MB;
	} else if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES) {
		hfi_value = inst->capabilities->cap[SLICE_MAX_BYTES].value;
		set_cap_id = SLICE_MAX_BYTES;
	}

	rc = msm_vidc_packetize_control(inst, set_cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_nal_length(void* instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = HFI_NAL_LENGTH_STARTCODES;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!inst->capabilities->cap[WITHOUT_STARTCODE].value) {
		hfi_value = HFI_NAL_LENGTH_STARTCODES;
	} else {
		rc = msm_vidc_v4l2_to_hfi_enum(inst, NAL_LENGTH_FIELD, &hfi_value);
		if (rc)
			return -EINVAL;
	}

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_layer_count_and_type(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_layer_count, hfi_layer_type = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!inst->bufq[OUTPUT_PORT].vb2q->streaming) {
		/* set layer type */
		hfi_layer_type = inst->hfi_layer_type;
		cap_id = LAYER_TYPE;

		rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
			&hfi_layer_type, sizeof(u32), __func__);
		if (rc)
			goto exit;
	} else {
		if (inst->hfi_layer_type == HFI_HIER_B) {
			i_vpr_l(inst,
				"%s: HB dyn layers change is not supported\n",
				__func__);
			return 0;
		}
	}

	/* set layer count */
	cap_id = ENH_LAYER_COUNT;
	/* hfi baselayer starts from 1 */
	hfi_layer_count = inst->capabilities->cap[ENH_LAYER_COUNT].value + 1;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_layer_count, sizeof(u32), __func__);
	if (rc)
		goto exit;

exit:
	return rc;
}

int msm_vidc_set_gop_size(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming) {
		if (inst->hfi_layer_type == HFI_HIER_B) {
			i_vpr_l(inst,
				"%s: HB dyn GOP setting is not supported\n",
				__func__);
			return 0;
		}
	}

	hfi_value = inst->capabilities->cap[GOP_SIZE].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_bitrate(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0, i;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;
	s32 rc_type = -1, enh_layer_count = -1;
	u32 layer_br_caps[6] = {L0_BR, L1_BR, L2_BR, L3_BR, L4_BR, L5_BR};

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* set Total Bitrate */
	if (inst->capabilities->cap[BIT_RATE].flags & CAP_FLAG_CLIENT_SET)
		goto set_total_bitrate;

	/*
	 * During runtime, if BIT_RATE cap CLIENT_SET flag is not set,
	 * then this function will be called due to change in ENH_LAYER_COUNT.
	 * In this case, client did not change bitrate, hence, no need to set
	 * to fw.
	 */
	if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	if (msm_vidc_get_parent_value(inst, BIT_RATE,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	if (rc_type != HFI_RC_CBR_CFR && rc_type != HFI_RC_CBR_VFR) {
		i_vpr_h(inst, "%s: set total bitrate for non CBR rc type\n",
			__func__);
		goto set_total_bitrate;
	}

	if (msm_vidc_get_parent_value(inst, BIT_RATE,
		ENH_LAYER_COUNT, &enh_layer_count, __func__))
		return -EINVAL;

	/*
	 * ENH_LAYER_COUNT cap max is positive only if
	 *    layer encoding is enabled during streamon.
	 */
	if (inst->capabilities->cap[ENH_LAYER_COUNT].max) {
		if (!msm_vidc_check_all_layer_bitrate_set(inst))
			goto set_total_bitrate;

		/* set Layer Bitrate */
		for (i = 0; i <= enh_layer_count; i++) {
			if (i >= ARRAY_SIZE(layer_br_caps))
				break;
			cap_id = layer_br_caps[i];
			hfi_value = inst->capabilities->cap[cap_id].value;
			rc = msm_vidc_packetize_control(inst, cap_id,
				HFI_PAYLOAD_U32, &hfi_value,
				sizeof(u32), __func__);
			if (rc)
				return rc;
		}
		goto exit;
	}

set_total_bitrate:
	hfi_value = inst->capabilities->cap[BIT_RATE].value;
	rc = msm_vidc_packetize_control(inst, BIT_RATE, HFI_PAYLOAD_U32,
			&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;
exit:
	return rc;
}

int msm_vidc_set_dynamic_layer_bitrate(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;
	s32 rc_type = -1;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	/* set Total Bitrate */
	if (inst->capabilities->cap[BIT_RATE].flags & CAP_FLAG_CLIENT_SET) {
		i_vpr_h(inst,
			"%s: Total bitrate is set, ignore layer bitrate\n",
			__func__);
		return 0;
	}

	/*
	 * ENH_LAYER_COUNT cap max is positive only if
	 *    layer encoding is enabled during streamon.
	 */
	if (!inst->capabilities->cap[ENH_LAYER_COUNT].max ||
		!msm_vidc_check_all_layer_bitrate_set(inst)) {
		i_vpr_h(inst,
			"%s: invalid layer bitrate, ignore setting to fw\n",
			__func__);
		return 0;
	}

	if (inst->hfi_rc_type == HFI_RC_CBR_CFR ||
		rc_type == HFI_RC_CBR_VFR) {
		/* set layer bitrate for the client set layer */
		hfi_value = inst->capabilities->cap[cap_id].value;
		rc = msm_vidc_packetize_control(inst, cap_id,
			HFI_PAYLOAD_U32, &hfi_value,
			sizeof(u32), __func__);
		if (rc)
			return rc;
	} else {
		/*
		 * All layer bitartes set for unsupported rc type.
		 * Hence accept layer bitrates, but set total bitrate prop
		 * with cumulative bitrate.
		 */
		hfi_value = inst->capabilities->cap[BIT_RATE].value;
		rc = msm_vidc_packetize_control(inst, BIT_RATE, HFI_PAYLOAD_U32,
				&hfi_value, sizeof(u32), __func__);
		if (rc)
			return rc;
	}

	return rc;
}

int msm_vidc_set_session_priority(void *instance,
		enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	u32 hfi_value = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hfi_value = inst->capabilities->cap[cap_id].value;
	if (!is_critical_priority_session(inst))
		hfi_value = inst->capabilities->cap[cap_id].value +
			inst->capabilities->cap[FIRMWARE_PRIORITY_OFFSET].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_flip(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	u32 hflip, vflip, hfi_value = HFI_DISABLE_FLIP;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hflip = inst->capabilities->cap[HFLIP].value;
	vflip = inst->capabilities->cap[VFLIP].value;

	if (hflip)
		hfi_value |= HFI_HORIZONTAL_FLIP;

	if (vflip)
		hfi_value |= HFI_VERTICAL_FLIP;

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming) {
		if (hfi_value != HFI_DISABLE_FLIP) {
			rc = msm_vidc_set_req_sync_frame(inst,
				REQUEST_I_FRAME);
			if (rc)
				return rc;
		}
	}

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_preprocess(void *instance,
        enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_rotation(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_v4l2_to_hfi_enum(inst, cap_id, &hfi_value);
	if (rc)
		return -EINVAL;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_blur_resolution(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	s32 blur_type = -1;
	u32 hfi_value, blur_width, blur_height;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (msm_vidc_get_parent_value(inst, cap_id,
		BLUR_TYPES, &blur_type, __func__))
		return -EINVAL;

	if (blur_type != VIDC_BLUR_EXTERNAL)
		return 0;

	hfi_value = inst->capabilities->cap[cap_id].value;

	blur_width = (hfi_value & 0xFFFF0000) >> 16;
	blur_height = hfi_value & 0xFFFF;

	if (blur_width > inst->crop.width ||
		blur_height > inst->crop.height) {
		i_vpr_e(inst,
			"%s: blur wxh: %dx%d exceeds crop wxh: %dx%d\n",
			__func__, blur_width, blur_height,
			inst->crop.width, inst->crop.height);
		hfi_value = 0;
	}

	if (blur_width == inst->crop.width &&
		blur_height == inst->crop.height) {
		i_vpr_e(inst,
			"%s: blur wxh: %dx%d is equal to crop wxh: %dx%d\n",
			__func__, blur_width, blur_height,
			inst->crop.width, inst->crop.height);
		hfi_value = 0;
	}

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

static int msm_venc_set_csc_coeff(struct msm_vidc_inst *inst,
	const char *prop_name, u32 hfi_id, void *payload,
	u32 payload_size, u32 row_count, u32 column_count)
{
	int rc = 0;

	i_vpr_h(inst,
		"set cap: name: %24s, hard coded %dx%d matrix array\n",
		prop_name, row_count, column_count);
	rc = venus_hfi_session_property(inst,
		hfi_id,
		HFI_HOST_FLAGS_NONE,
		HFI_PORT_BITSTREAM,
		HFI_PAYLOAD_S32_ARRAY,
		payload,
		payload_size);
	if (rc) {
		i_vpr_e(inst,
			"%s: failed to set %s to fw\n",
			__func__, prop_name);
	}

	return rc;
}
int msm_vidc_set_csc_custom_matrix(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	int i;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	struct msm_vidc_core *core;
	struct msm_vidc_csc_coeff *csc_coeff;
	s32 matrix_payload[MAX_MATRIX_COEFFS + 2];
	s32 csc_bias_payload[MAX_BIAS_COEFFS + 2];
	s32 csc_limit_payload[MAX_LIMIT_COEFFS + 2];

	if (!inst || !inst->capabilities || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	if (!core->platform) {
		d_vpr_e("%s: invalid core platform\n", __func__);
		return -EINVAL;
	}
	csc_coeff = &core->platform->data.csc_data;

	if (!inst->capabilities->cap[cap_id].value ||
		!inst->capabilities->cap[CSC].value) {
		i_vpr_h(inst,
			"%s: ignored as custom martix %u, csc %u\n",
			__func__, inst->capabilities->cap[cap_id].value,
			inst->capabilities->cap[CSC].value);
		return 0;
	}

	/*
	 * first 2 u32's of payload in each case are for
	 * row and column count, next remaining u32's are
	 * for the actual payload values.
	 */

	/* set custom matrix */
	matrix_payload[0] = 3;
	matrix_payload[1] = 3;

	for(i = 0; i < MAX_MATRIX_COEFFS; i++) {
		if ((i + 2) >= ARRAY_SIZE(matrix_payload))
			break;
		matrix_payload[i + 2] =
			csc_coeff->vpe_csc_custom_matrix_coeff[i];
	}

	rc = msm_venc_set_csc_coeff(inst, "CSC_CUSTOM_MATRIX",
		HFI_PROP_CSC_MATRIX, &matrix_payload[0],
		ARRAY_SIZE(matrix_payload) * sizeof(s32),
		matrix_payload[0], matrix_payload[1]);
	if (rc)
		return rc;

	/* set csc bias */
	csc_bias_payload[0] = 1;
	csc_bias_payload[1] = 3;

	for(i = 0; i < MAX_BIAS_COEFFS; i++) {
		if ((i + 2) >= ARRAY_SIZE(csc_bias_payload))
			break;
		csc_bias_payload[i + 2] =
			csc_coeff->vpe_csc_custom_bias_coeff[i];
	}

	rc = msm_venc_set_csc_coeff(inst, "CSC_BIAS",
		HFI_PROP_CSC_BIAS, &csc_bias_payload[0],
		ARRAY_SIZE(csc_bias_payload) * sizeof(s32),
		csc_bias_payload[0], csc_bias_payload[1]);
	if (rc)
		return rc;

	/* set csc limit */
	csc_limit_payload[0] = 1;
	csc_limit_payload[1] = 6;

	for(i = 0; i < MAX_LIMIT_COEFFS; i++) {
		if ((i + 2) >= ARRAY_SIZE(csc_limit_payload))
			break;
		csc_limit_payload[i + 2] =
			csc_coeff->vpe_csc_custom_limit_coeff[i];
	}

	rc = msm_venc_set_csc_coeff(inst, "CSC_LIMIT",
		HFI_PROP_CSC_LIMIT, &csc_limit_payload[0],
		ARRAY_SIZE(csc_limit_payload) * sizeof(s32),
		csc_limit_payload[0], csc_limit_payload[1]);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_reserve_duration(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	u32 hfi_value = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* reserve hardware only when input port is streaming*/
	if (!inst->bufq[INPUT_PORT].vb2q->streaming)
		return 0;

	if (!(inst->capabilities->cap[cap_id].flags & CAP_FLAG_CLIENT_SET))
		return 0;

	inst->capabilities->cap[cap_id].flags &= (~CAP_FLAG_CLIENT_SET);

	if (!is_critical_priority_session(inst)) {
		i_vpr_h(inst, "%s: reserve duration allowed only for critical session\n", __func__);
		return 0;
	}

	hfi_value = inst->capabilities->cap[cap_id].value;

	rc = venus_hfi_reserve_hardware(inst, hfi_value);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_level(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hfi_value = inst->capabilities->cap[cap_id].value;
	if (!(inst->capabilities->cap[cap_id].flags & CAP_FLAG_CLIENT_SET))
		hfi_value = HFI_LEVEL_NONE;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_ir_period(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 ir_type = 0;
	struct msm_vidc_core *core;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;

	if (inst->capabilities->cap[IR_TYPE].value ==
	    V4L2_MPEG_VIDEO_VIDC_INTRA_REFRESH_RANDOM) {
		if (inst->bufq[OUTPUT_PORT].vb2q->streaming) {
			i_vpr_h(inst, "%s: dynamic random intra refresh not allowed\n",
				__func__);
			return 0;
		}
		ir_type = HFI_PROP_IR_RANDOM_PERIOD;
	} else if (inst->capabilities->cap[IR_TYPE].value ==
		   V4L2_MPEG_VIDEO_VIDC_INTRA_REFRESH_CYCLIC) {
		ir_type = HFI_PROP_IR_CYCLIC_PERIOD;
	} else {
		i_vpr_e(inst, "%s: invalid ir_type %d\n",
			__func__, inst->capabilities->cap[IR_TYPE]);
		return -EINVAL;
	}

	rc = venus_hfi_set_ir_period(inst, ir_type, cap_id);
	if (rc) {
		i_vpr_e(inst, "%s: failed to set ir period %d\n",
			__func__, inst->capabilities->cap[IR_PERIOD].value);
		return rc;
	}

	return rc;
}

int msm_vidc_set_q16(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hfi_value = inst->capabilities->cap[cap_id].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_Q16,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_u32(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->capabilities->cap[cap_id].flags & CAP_FLAG_MENU) {
		rc = msm_vidc_v4l2_menu_to_hfi(inst, cap_id, &hfi_value);
		if (rc)
			return -EINVAL;
	} else {
		hfi_value = inst->capabilities->cap[cap_id].value;
	}

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_u32_packed(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->capabilities->cap[cap_id].flags & CAP_FLAG_MENU) {
		rc = msm_vidc_v4l2_menu_to_hfi(inst, cap_id, &hfi_value);
		if (rc)
			return -EINVAL;
	} else {
		hfi_value = inst->capabilities->cap[cap_id].value;
	}

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_u32_enum(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_v4l2_to_hfi_enum(inst, cap_id, &hfi_value);
	if (rc)
		return -EINVAL;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32_ENUM,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_s32(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	s32 hfi_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hfi_value = inst->capabilities->cap[cap_id].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_S32,
		&hfi_value, sizeof(s32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_v4l2_menu_to_hfi(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id, u32 *value)
{
	struct msm_vidc_inst_capability *capability = inst->capabilities;

	switch (capability->cap[cap_id].v4l2_id) {
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		switch (capability->cap[cap_id].value) {
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC:
			*value = 1;
			break;
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC:
			*value = 0;
			break;
		default:
			*value = 1;
			goto set_default;
		}
		return 0;
	case V4L2_CID_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE:
		switch (capability->cap[cap_id].value) {
		case V4L2_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE_FRAME_BASED:
			*value = 0;
			break;
		case V4L2_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE_SLICE_BASED:
			*value = 1;
			break;
		default:
			*value = 0;
			goto set_default;
		}
		return 0;
	case V4L2_CID_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE:
		switch (capability->cap[cap_id].value) {
		case V4L2_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE_FRAME_BASED:
			*value = 0;
			break;
		case V4L2_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE_SLICE_BASED:
			*value = 1;
			break;
		default:
			*value = 0;
			goto set_default;
		}
		return 0;
	default:
		i_vpr_e(inst,
			"%s: mapping not specified for ctrl_id: %#x\n",
			__func__, capability->cap[cap_id].v4l2_id);
		return -EINVAL;
	}

set_default:
	i_vpr_e(inst,
		"%s: invalid value %d for ctrl id: %#x. Set default: %u\n",
		__func__, capability->cap[cap_id].value,
		capability->cap[cap_id].v4l2_id, *value);
	return 0;
}

int msm_vidc_v4l2_to_hfi_enum(struct msm_vidc_inst *inst,
	enum msm_vidc_inst_capability_type cap_id, u32 *value)
{
	struct msm_vidc_inst_capability *capability = inst->capabilities;

	switch (capability->cap[cap_id].v4l2_id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
		*value = inst->hfi_rc_type;
		return 0;
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
	case V4L2_CID_MPEG_VIDEO_AV1_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_VP9_LEVEL:
	case V4L2_CID_MPEG_VIDEO_AV1_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
	case V4L2_CID_MPEG_VIDEO_AV1_TIER:
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_TYPES:
		*value = capability->cap[cap_id].value;
		return 0;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE:
		switch (capability->cap[cap_id].value) {
		case V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B:
			*value = HFI_HIER_B;
			break;
		case V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P:
			//TODO (AS): check if this is right mapping
			*value = HFI_HIER_P_SLIDING_WINDOW;
			break;
		default:
			*value = HFI_HIER_P_SLIDING_WINDOW;
			goto set_default;
		}
		return 0;
	case V4L2_CID_ROTATE:
		switch (capability->cap[cap_id].value) {
		case 0:
			*value = HFI_ROTATION_NONE;
			break;
		case 90:
			*value = HFI_ROTATION_90;
			break;
		case 180:
			*value = HFI_ROTATION_180;
			break;
		case 270:
			*value = HFI_ROTATION_270;
			break;
		default:
			*value = HFI_ROTATION_NONE;
			goto set_default;
		}
		return 0;
	case V4L2_CID_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE:
		switch (capability->cap[cap_id].value) {
		case V4L2_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE_ENABLED:
			*value = HFI_DEBLOCK_ALL_BOUNDARY;
			break;
		case V4L2_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE_DISABLED:
			*value = HFI_DEBLOCK_DISABLE;
			break;
		case DB_HEVC_DISABLE_SLICE_BOUNDARY:
			*value = HFI_DEBLOCK_DISABLE_AT_SLICE_BOUNDARY;
			break;
		default:
			*value = HFI_DEBLOCK_ALL_BOUNDARY;
			goto set_default;
		}
		return 0;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		switch (capability->cap[cap_id].value) {
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED:
			*value = HFI_DEBLOCK_ALL_BOUNDARY;
			break;
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED:
			*value = HFI_DEBLOCK_DISABLE;
			break;
		case DB_H264_DISABLE_SLICE_BOUNDARY:
			*value = HFI_DEBLOCK_DISABLE_AT_SLICE_BOUNDARY;
			break;
		default:
			*value = HFI_DEBLOCK_ALL_BOUNDARY;
			goto set_default;
		}
		return 0;
	case V4L2_CID_MPEG_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD:
		switch (capability->cap[cap_id].value) {
		case V4L2_MPEG_VIDEO_HEVC_SIZE_4:
			*value = HFI_NAL_LENGTH_SIZE_4;
			break;
		default:
			*value = HFI_NAL_LENGTH_STARTCODES;
			goto set_default;
		}
		return 0;
	default:
		i_vpr_e(inst,
			"%s: mapping not specified for ctrl_id: %#x\n",
			__func__, capability->cap[cap_id].v4l2_id);
		return -EINVAL;
	}

set_default:
	i_vpr_e(inst,
		"%s: invalid value %d for ctrl id: %#x. Set default: %u\n",
		__func__, capability->cap[cap_id].value,
		capability->cap[cap_id].v4l2_id, *value);
	return 0;
}

int msm_vidc_set_stage(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	u32 stage = 0;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (!inst || !inst->capabilities || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;

	rc = call_session_op(core, decide_work_mode, inst);
	if (rc) {
		i_vpr_e(inst, "%s: decide_work_mode failed\n", __func__);
		return -EINVAL;
	}

	stage = inst->capabilities->cap[STAGE].value;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&stage, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_pipe(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	u32 pipe;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (!inst || !inst->capabilities || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;

	rc = call_session_op(core, decide_work_route, inst);
	if (rc) {
		i_vpr_e(inst, "%s: decide_work_route failed\n",
			__func__);
		return -EINVAL;
	}

	pipe = inst->capabilities->cap[PIPE].value;
	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
			&pipe, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_set_vui_timing_info(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 hfi_value;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/*
	 * hfi is HFI_PROP_DISABLE_VUI_TIMING_INFO and v4l2 cap is
	 * V4L2_CID_MPEG_VIDC_VUI_TIMING_INFO and hence reverse
	 * the hfi_value from cap_id value.
	 */
	if (inst->capabilities->cap[cap_id].value == V4L2_MPEG_MSM_VIDC_ENABLE)
		hfi_value = 0;
	else
		hfi_value = 1;

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_U32,
		&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}
