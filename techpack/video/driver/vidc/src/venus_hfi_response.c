// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/devcoredump.h>
#include <linux/of_address.h>
#include "hfi_packet.h"
#include "venus_hfi.h"
#include "venus_hfi_response.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_driver.h"
#include "msm_vdec.h"
#include "msm_vidc_control.h"
#include "msm_vidc_memory.h"
#include "msm_vidc_fence.h"

#define in_range(range, val) (((range.begin) < (val)) && ((range.end) > (val)))

extern struct msm_vidc_core *g_core;
struct msm_vidc_core_hfi_range {
	u32 begin;
	u32 end;
	int (*handle)(struct msm_vidc_core *core, struct hfi_packet *pkt);
};

struct msm_vidc_inst_hfi_range {
	u32 begin;
	u32 end;
	int (*handle)(struct msm_vidc_inst *inst, struct hfi_packet *pkt);
};

struct msm_vidc_hfi_buffer_handle {
	enum hfi_buffer_type type;
	int (*handle)(struct msm_vidc_inst *inst, struct hfi_buffer *buffer);
};

struct msm_vidc_hfi_packet_handle {
	enum hfi_buffer_type type;
	int (*handle)(struct msm_vidc_inst *inst, struct hfi_packet *pkt);
};

void print_psc_properties(const char *str, struct msm_vidc_inst *inst,
	struct msm_vidc_subscription_params subsc_params)
{
	if (!inst || !str)
		return;

	i_vpr_h(inst,
		"%s: width %d, height %d, crop offsets[0] %#x, crop offsets[1] %#x, bit depth %#x, coded frames %d "
		"fw min count %d, poc %d, color info %d, profile %d, level %d, tier %d, fg present %d, sb enabled %d\n",
		str, (subsc_params.bitstream_resolution & HFI_BITMASK_BITSTREAM_WIDTH) >> 16,
		(subsc_params.bitstream_resolution & HFI_BITMASK_BITSTREAM_HEIGHT),
		subsc_params.crop_offsets[0], subsc_params.crop_offsets[1],
		subsc_params.bit_depth, subsc_params.coded_frames,
		subsc_params.fw_min_count, subsc_params.pic_order_cnt,
		subsc_params.color_info, subsc_params.profile, subsc_params.level,
		subsc_params.tier, subsc_params.av1_film_grain_present,
		subsc_params.av1_super_block_enabled);
}

static void print_sfr_message(struct msm_vidc_core *core)
{
	struct msm_vidc_sfr *vsfr = NULL;
	u32 vsfr_size = 0;
	void *p = NULL;

	vsfr = (struct msm_vidc_sfr *)core->sfr.align_virtual_addr;
	if (vsfr) {
		if (vsfr->bufSize != core->sfr.mem_size) {
			d_vpr_e("Invalid SFR buf size %d actual %d\n",
				vsfr->bufSize, core->sfr.mem_size);
			return;
		}
		vsfr_size = vsfr->bufSize - sizeof(u32);
		p = memchr(vsfr->rg_data, '\0', vsfr_size);
		/* SFR isn't guaranteed to be NULL terminated */
		if (p == NULL)
			vsfr->rg_data[vsfr_size - 1] = '\0';

		d_vpr_e(FMT_STRING_MSG_SFR, vsfr->rg_data);
	}
}

u32 vidc_port_from_hfi(struct msm_vidc_inst *inst,
	enum hfi_packet_port_type hfi_port)
{
	enum msm_vidc_port_type port = MAX_PORT;

	if (is_decode_session(inst)) {
		switch (hfi_port) {
		case HFI_PORT_BITSTREAM:
			port = INPUT_PORT;
			break;
		case HFI_PORT_RAW:
			port = OUTPUT_PORT;
			break;
		case HFI_PORT_NONE:
			port = PORT_NONE;
			break;
		default:
			i_vpr_e(inst, "%s: invalid hfi port type %d\n",
				__func__, hfi_port);
			break;
		}
	} else if (is_encode_session(inst)) {
		switch (hfi_port) {
		case HFI_PORT_RAW:
			port = INPUT_PORT;
			break;
		case HFI_PORT_BITSTREAM:
			port = OUTPUT_PORT;
			break;
		case HFI_PORT_NONE:
			port = PORT_NONE;
			break;
		default:
			i_vpr_e(inst, "%s: invalid hfi port type %d\n",
				__func__, hfi_port);
			break;
		}
	} else {
		i_vpr_e(inst, "%s: invalid domain %#x\n",
			__func__, inst->domain);
	}

	return port;
}

bool is_valid_hfi_port(struct msm_vidc_inst *inst, u32 port,
	u32 buffer_type, const char *func)
{
	if (!inst) {
		i_vpr_e(inst, "%s: invalid params\n", func);
		return false;
	}

	if (port == HFI_PORT_NONE &&
		buffer_type != HFI_BUFFER_ARP &&
		buffer_type != HFI_BUFFER_PERSIST)
		goto invalid;

	if (port != HFI_PORT_BITSTREAM && port != HFI_PORT_RAW)
		goto invalid;

	return true;

invalid:
	i_vpr_e(inst, "%s: invalid port %#x buffer_type %u\n",
			func, port, buffer_type);
	return false;
}

bool is_valid_hfi_buffer_type(struct msm_vidc_inst *inst,
	u32 buffer_type, const char *func)
{
	if (!inst) {
		i_vpr_e(inst, "%s: invalid params\n", func);
		return false;
	}

	if (buffer_type != HFI_BUFFER_BITSTREAM &&
	    buffer_type != HFI_BUFFER_RAW &&
	    buffer_type != HFI_BUFFER_METADATA &&
	    buffer_type != HFI_BUFFER_BIN &&
	    buffer_type != HFI_BUFFER_ARP &&
	    buffer_type != HFI_BUFFER_COMV &&
	    buffer_type != HFI_BUFFER_NON_COMV &&
	    buffer_type != HFI_BUFFER_LINE &&
	    buffer_type != HFI_BUFFER_DPB &&
	    buffer_type != HFI_BUFFER_PERSIST &&
	    buffer_type != HFI_BUFFER_VPSS &&
	    buffer_type != HFI_BUFFER_PARTIAL_DATA) {
		i_vpr_e(inst, "%s: invalid buffer type %#x\n",
			func, buffer_type);
		return false;
	}
	return true;
}

int validate_packet(u8 *response_pkt, u8 *core_resp_pkt,
	u32 core_resp_pkt_size, const char *func)
{
	u8 *response_limit;
	u32 response_pkt_size = 0;

	if (!response_pkt || !core_resp_pkt || !core_resp_pkt_size) {
		d_vpr_e("%s: invalid params\n", func);
		return -EINVAL;
	}

	response_limit = core_resp_pkt + core_resp_pkt_size;

	if (response_pkt < core_resp_pkt || response_pkt > response_limit) {
		d_vpr_e("%s: invalid packet address\n", func);
		return -EINVAL;
	}

	response_pkt_size = *(u32 *)response_pkt;
	if (!response_pkt_size) {
		d_vpr_e("%s: response packet size cannot be zero\n", func);
		return -EINVAL;
	}

	if (response_pkt_size < sizeof(struct hfi_packet)) {
		d_vpr_e("%s: invalid packet size %d\n",
			func, response_pkt_size);
		return -EINVAL;
	}

	if (response_pkt + response_pkt_size > response_limit) {
		d_vpr_e("%s: invalid packet size %d\n",
			func, response_pkt_size);
		return -EINVAL;
	}
	return 0;
}

static int validate_hdr_packet(struct msm_vidc_core *core,
	struct hfi_header *hdr, const char *function)
{
	struct hfi_packet *packet;
	u8 *pkt;
	int i, rc = 0;

	if (!core || !hdr || !function) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (hdr->size < sizeof(struct hfi_header) + sizeof(struct hfi_packet)) {
		d_vpr_e("%s: invalid header size %d\n", __func__, hdr->size);
		return -EINVAL;
	}

	pkt = (u8 *)((u8 *)hdr + sizeof(struct hfi_header));

	/* validate all packets */
	for (i = 0; i < hdr->num_packets; i++) {
		packet = (struct hfi_packet *)pkt;
		rc = validate_packet(pkt, core->response_packet, core->packet_size, function);
		if (rc)
			return rc;

		pkt += packet->size;
	}

	return 0;
}

static bool check_for_packet_payload(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt, const char *func)
{
	u32 payload_size = 0;

	if (!inst || !pkt) {
		d_vpr_e("%s: invalid params\n", __func__);
		return false;
	}

	if (pkt->payload_info == HFI_PAYLOAD_NONE) {
		i_vpr_h(inst, "%s: no playload available for packet %#x\n",
			func, pkt->type);
		return false;
	}

	switch (pkt->payload_info) {
	case HFI_PAYLOAD_U32:
	case HFI_PAYLOAD_S32:
	case HFI_PAYLOAD_Q16:
	case HFI_PAYLOAD_U32_ENUM:
	case HFI_PAYLOAD_32_PACKED:
		payload_size = 4;
		break;
	case HFI_PAYLOAD_U64:
	case HFI_PAYLOAD_S64:
	case HFI_PAYLOAD_64_PACKED:
		payload_size = 8;
		break;
	case HFI_PAYLOAD_STRUCTURE:
		if (pkt->type == HFI_CMD_BUFFER)
			payload_size = sizeof(struct hfi_buffer);
		break;
	default:
		payload_size = 0;
		break;
	}

	if (pkt->size < sizeof(struct hfi_packet) + payload_size) {
		i_vpr_e(inst,
			"%s: invalid payload size %u payload type %#x for packet %#x\n",
			func, pkt->size, pkt->payload_info, pkt->type);
		return false;
	}

	return true;
}

static int handle_session_last_flag_info(struct msm_vidc_inst *inst,
		struct hfi_packet *pkt)
{
	int rc = 0;

	if (pkt->type == HFI_INFO_HFI_FLAG_PSC_LAST) {
		if (msm_vidc_allow_psc_last_flag(inst))
			rc = msm_vidc_process_psc_last_flag(inst);
		else
			rc = -EINVAL;
	} else if (pkt->type == HFI_INFO_HFI_FLAG_DRAIN_LAST) {
		if (msm_vidc_allow_drain_last_flag(inst))
			rc = msm_vidc_process_drain_last_flag(inst);
		else
			rc = -EINVAL;
	} else {
		i_vpr_e(inst, "%s: invalid packet type %#x\n", __func__,
			pkt->type);
	}

	if (rc)
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);

	return rc;
}

static int handle_session_info(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int rc = 0;
	char *info;

	switch (pkt->type) {
	case HFI_INFO_UNSUPPORTED:
		info = "unsupported";
		break;
	case HFI_INFO_DATA_CORRUPT:
		info = "data corrupt";
		inst->hfi_frame_info.data_corrupt = 1;
		break;
	case HFI_INFO_BUFFER_OVERFLOW:
		info = "buffer overflow";
		inst->hfi_frame_info.overflow = 1;
		break;
	case HFI_INFO_HFI_FLAG_DRAIN_LAST:
		info = "drain last flag";
		rc = handle_session_last_flag_info(inst, pkt);
		break;
	case HFI_INFO_HFI_FLAG_PSC_LAST:
		info = "drc last flag";
		rc = handle_session_last_flag_info(inst, pkt);
		break;
	default:
		info = "unknown";
		break;
	}

	i_vpr_h(inst, "session info (%#x): %s\n", pkt->type, info);

	return rc;
}

static int handle_session_error(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int rc = 0;
	char *error;

	switch (pkt->type) {
	case HFI_ERROR_MAX_SESSIONS:
		error = "exceeded max sessions";
		break;
	case HFI_ERROR_UNKNOWN_SESSION:
		error = "unknown session id";
		break;
	case HFI_ERROR_INVALID_STATE:
		error = "invalid operation for current state";
		break;
	case HFI_ERROR_INSUFFICIENT_RESOURCES:
		error = "insufficient resources";
		break;
	case HFI_ERROR_BUFFER_NOT_SET:
		error = "internal buffers not set";
		break;
	case HFI_ERROR_FATAL:
		error = "fatal error";
		break;
	case HFI_ERROR_STREAM_UNSUPPORTED:
		error = "stream unsupported";
		break;
	default:
		error = "unknown";
		break;
	}

	i_vpr_e(inst, "%s: session error received %#x: %s\n",
		__func__, pkt->type, error);

	rc = msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	return rc;
}

void fw_coredump(struct msm_vidc_core *core)
{
	int rc = 0;
	struct platform_device *pdev;
	struct device_node *node = NULL;
	struct resource res = {0};
	phys_addr_t mem_phys = 0;
	size_t res_size = 0;
	void *mem_va = NULL;
	char *data = NULL, *dump = NULL;
	u64 total_size;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}
	pdev = core->pdev;

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node) {
		d_vpr_e("%s: DT error getting \"memory-region\" property\n",
			__func__);
		return;
	}

	rc = of_address_to_resource(node, 0, &res);
	if (rc) {
		d_vpr_e("%s: error %d while getting \"memory-region\" resource\n",
			__func__, rc);
		return;
	}

	mem_phys = res.start;
	res_size = (size_t)resource_size(&res);

	mem_va = memremap(mem_phys, res_size, MEMREMAP_WC);
	if (!mem_va) {
		d_vpr_e("%s: unable to remap firmware memory\n", __func__);
		return;
	}
	total_size = res_size + TOTAL_QSIZE + ALIGNED_SFR_SIZE;

	data = vmalloc(total_size);
	if (!data) {
		memunmap(mem_va);
		return;
	}
	dump = data;

	/* copy firmware dump */
	memcpy(data, mem_va, res_size);
	memunmap(mem_va);

	/* copy queues(cmd, msg, dbg) dump(along with headers) */
	data += res_size;
	memcpy(data, (char *)core->iface_q_table.align_virtual_addr, TOTAL_QSIZE);

	/* copy sfr dump */
	data += TOTAL_QSIZE;
	memcpy(data, (char *)core->sfr.align_virtual_addr, ALIGNED_SFR_SIZE);

	dev_coredumpv(&pdev->dev, dump, total_size, GFP_KERNEL);
}

int handle_system_error(struct msm_vidc_core *core,
	struct hfi_packet *pkt)
{
	bool bug_on = false;

	d_vpr_e("%s: system error received\n", __func__);
	print_sfr_message(core);
	venus_hfi_noc_error_info(core);

	if (pkt) {
		/* enable force bugon for requested type */
		if (pkt->type == HFI_SYS_ERROR_FATAL)
			bug_on = !!(msm_vidc_enable_bugon & MSM_VIDC_BUG_ON_FATAL);
		else if (pkt->type == HFI_SYS_ERROR_NOC)
			bug_on = !!(msm_vidc_enable_bugon & MSM_VIDC_BUG_ON_NOC);
		else if (pkt->type == HFI_SYS_ERROR_WD_TIMEOUT)
			bug_on = !!(msm_vidc_enable_bugon & MSM_VIDC_BUG_ON_WD_TIMEOUT);

		if (bug_on) {
			d_vpr_e("%s: force bugon for type %#x\n", __func__, pkt->type);
			MSM_VIDC_FATAL(true);
		}
	}

	msm_vidc_core_deinit(core, true);

	return 0;
}

static int handle_system_init(struct msm_vidc_core *core,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS) {
		core_lock(core, __func__);
		if (core->state == MSM_VIDC_CORE_INIT_WAIT &&
				pkt->packet_id == core->sys_init_id) {
			msm_vidc_change_core_state(core, MSM_VIDC_CORE_INIT, __func__);
			d_vpr_h("%s: successful\n", __func__);
		} else if (core->state != MSM_VIDC_CORE_INIT_WAIT) {
			d_vpr_e("%s: invalid core state %s\n", __func__,
				core_state_name(core->state));
		} else if (pkt->packet_id != core->sys_init_id) {
			d_vpr_e("%s: invalid pkt id %u, expected %u\n", __func__,
				pkt->packet_id, core->sys_init_id);
		}
		core_unlock(core, __func__);
	} else {
		d_vpr_h("%s: unhandled. flags=%d\n", __func__, pkt->flags);
	}

	return 0;
}

static int handle_session_open(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);

	return 0;
}

static int handle_session_close(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);

	signal_session_msg_receipt(inst, SIGNAL_CMD_CLOSE);
	return 0;
}

static int handle_session_start(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful for port %d\n",
			__func__, pkt->port);
	return 0;
}

static int handle_session_stop(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int rc = 0;
	enum signal_session_response signal_type = -1;

	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful for port %d\n",
			__func__, pkt->port);

	if (is_encode_session(inst)) {
		if (pkt->port == HFI_PORT_RAW) {
			signal_type = SIGNAL_CMD_STOP_INPUT;
		} else if (pkt->port == HFI_PORT_BITSTREAM) {
			signal_type = SIGNAL_CMD_STOP_OUTPUT;
		} else {
			i_vpr_e(inst, "%s: invalid port: %d\n",
				__func__, pkt->port);
			return -EINVAL;
		}
	} else if (is_decode_session(inst)) {
		if (pkt->port == HFI_PORT_RAW) {
			signal_type = SIGNAL_CMD_STOP_OUTPUT;
		} else if (pkt->port == HFI_PORT_BITSTREAM) {
			signal_type = SIGNAL_CMD_STOP_INPUT;
		} else {
			i_vpr_e(inst, "%s: invalid port: %d\n",
				__func__, pkt->port);
			return -EINVAL;
		}
	} else {
		i_vpr_e(inst, "%s: invalid session\n", __func__);
		return -EINVAL;
	}

	if (signal_type != -1) {
		rc = msm_vidc_process_stop_done(inst, signal_type);
		if (rc)
			return rc;
	}

	return 0;
}

static int handle_session_drain(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int rc = 0;

	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);

	rc = msm_vidc_process_drain_done(inst);
	if (rc)
		return rc;

	return rc;
}

static int get_driver_buffer_flags(struct msm_vidc_inst *inst, u32 hfi_flags)
{
	u32 driver_flags = 0;

	if (inst->hfi_frame_info.picture_type & HFI_PICTURE_IDR) {
		driver_flags |= MSM_VIDC_BUF_FLAG_KEYFRAME;
	} else if (inst->hfi_frame_info.picture_type & HFI_PICTURE_P) {
		driver_flags |= MSM_VIDC_BUF_FLAG_PFRAME;
	} else if (inst->hfi_frame_info.picture_type & HFI_PICTURE_B) {
		driver_flags |= MSM_VIDC_BUF_FLAG_BFRAME;
	} else if (inst->hfi_frame_info.picture_type & HFI_PICTURE_I) {
		driver_flags |= MSM_VIDC_BUF_FLAG_KEYFRAME;
	} else if (inst->hfi_frame_info.picture_type & HFI_PICTURE_CRA) {
		driver_flags |= MSM_VIDC_BUF_FLAG_KEYFRAME;
	} else if (inst->hfi_frame_info.picture_type & HFI_PICTURE_BLA) {
		driver_flags |= MSM_VIDC_BUF_FLAG_KEYFRAME;
	}

	if (inst->hfi_frame_info.data_corrupt)
		driver_flags |= MSM_VIDC_BUF_FLAG_ERROR;

	if (inst->hfi_frame_info.overflow)
		driver_flags |= MSM_VIDC_BUF_FLAG_ERROR;

	if (inst->hfi_frame_info.no_output) {
		if (inst->capabilities->cap[META_BUF_TAG].value &&
			!(hfi_flags & HFI_BUF_FW_FLAG_CODEC_CONFIG))
			driver_flags |= MSM_VIDC_BUF_FLAG_ERROR;
	}

	if (inst->hfi_frame_info.subframe_input)
		if (inst->capabilities->cap[META_BUF_TAG].value)
			driver_flags |= MSM_VIDC_BUF_FLAG_ERROR;

	if (hfi_flags & HFI_BUF_FW_FLAG_CODEC_CONFIG)
		driver_flags |= MSM_VIDC_BUF_FLAG_CODECCONFIG;

	/*
	 * attach last flag to the buffer for encode session.
	 * For decode session attach only if control(LAST_FLAG_EVENT_ENABLE)
	 * is not set by client. If this control is enabled, last flag
	 * info will be sent via event(V4L2_EVENT_EOS) to client.
	 */
	if ((is_encode_session(inst) &&
		(hfi_flags & HFI_BUF_FW_FLAG_LAST)) ||
		(is_decode_session(inst) &&
		!inst->capabilities->cap[LAST_FLAG_EVENT_ENABLE].value &&
		((hfi_flags & HFI_BUF_FW_FLAG_LAST) ||
		(hfi_flags & HFI_BUF_FW_FLAG_PSC_LAST))))
		driver_flags |= MSM_VIDC_BUF_FLAG_LAST;

	return driver_flags;
}

static int handle_read_only_buffer(struct msm_vidc_inst *inst,
	struct msm_vidc_buffer *buf)
{
	struct msm_vidc_buffer *ro_buf;
	struct msm_vidc_buffers *ro_buffers;
	bool found = false;

	if (!inst || !buf) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!is_decode_session(inst) || !is_output_buffer(buf->type))
		return 0;

	if (!(buf->attr & MSM_VIDC_ATTR_READ_ONLY))
		return 0;

	ro_buffers = msm_vidc_get_buffers(inst, MSM_VIDC_BUF_READ_ONLY, __func__);
	if (!ro_buffers)
		return -EINVAL;

	list_for_each_entry(ro_buf, &ro_buffers->list, list) {
		if (ro_buf->device_addr == buf->device_addr) {
			found = true;
			break;
		}
	}
	/*
	 * RO flag: add to read_only list if buffer is not present
	 *          if present, do nothing
	 */
	if (!found) {
		ro_buf = msm_memory_pool_alloc(inst, MSM_MEM_POOL_BUFFER);
		if (!ro_buf) {
			i_vpr_e(inst, "%s: buffer alloc failed\n", __func__);
			return -ENOMEM;
		}
		memcpy(ro_buf, buf, sizeof(struct msm_vidc_buffer));
		INIT_LIST_HEAD(&ro_buf->list);
		list_add_tail(&ro_buf->list, &ro_buffers->list);
		print_vidc_buffer(VIDC_LOW, "low ", "ro buf added", inst, ro_buf);
	}
	ro_buf->attr |= MSM_VIDC_ATTR_READ_ONLY;

	return 0;
}

static int handle_non_read_only_buffer(struct msm_vidc_inst *inst,
	struct hfi_buffer *buffer)
{
	struct msm_vidc_buffer *ro_buf;
	struct msm_vidc_buffers *ro_buffers;
	bool found = false;

	if (!inst || !buffer) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!is_decode_session(inst) || buffer->type != HFI_BUFFER_RAW)
		return 0;

	if (buffer->flags & HFI_BUF_FW_FLAG_READONLY)
		return 0;

	ro_buffers = msm_vidc_get_buffers(inst, MSM_VIDC_BUF_READ_ONLY, __func__);
	if (!ro_buffers)
		return -EINVAL;

	list_for_each_entry(ro_buf, &ro_buffers->list, list) {
		if (ro_buf->device_addr == buffer->base_address) {
			found = true;
			break;
		}
	}

	/*
	 * Without RO flag: remove buffer from read_only list if present
	 *          if not present, do not error out
	 */
	if (found) {
		print_vidc_buffer(VIDC_LOW, "low ", "ro buf deleted", inst, ro_buf);
		list_del(&ro_buf->list);
		msm_memory_pool_free(inst, ro_buf);
	}

	return 0;
}

static int handle_psc_last_flag_buffer(struct msm_vidc_inst *inst,
		struct hfi_buffer *buffer)
{
	int rc = 0;

	if (!(buffer->flags & HFI_BUF_FW_FLAG_PSC_LAST))
		return 0;

	if (!msm_vidc_allow_psc_last_flag(inst))
		return -EINVAL;

	rc = msm_vidc_process_psc_last_flag(inst);
	if (rc)
		return rc;

	return rc;
}

static int handle_drain_last_flag_buffer(struct msm_vidc_inst *inst,
		struct hfi_buffer *buffer)
{
	int rc = 0;

	if (!(buffer->flags & HFI_BUF_FW_FLAG_LAST))
		return 0;

	if (!msm_vidc_allow_drain_last_flag(inst))
		return -EINVAL;

	if (is_decode_session(inst)) {
		rc = msm_vidc_process_drain_last_flag(inst);
		if (rc)
			return rc;
	} else if (is_encode_session(inst)) {
		rc = msm_vidc_state_change_drain_last_flag(inst);
		if (rc)
			return rc;
	}

	return rc;
}

static int handle_input_buffer(struct msm_vidc_inst *inst,
	struct hfi_buffer *buffer)
{
	int rc = 0;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buf;
	struct msm_vidc_core *core;
	u32 frame_size, batch_size;
	bool found;

	if (!inst || !buffer || !inst->capabilities || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	buffers = msm_vidc_get_buffers(inst, MSM_VIDC_BUF_INPUT, __func__);
	if (!buffers)
		return -EINVAL;

	found = false;
	list_for_each_entry(buf, &buffers->list, list) {
		if (buf->index == buffer->index) {
			found = true;
			break;
		}
	}
	if (!found) {
		i_vpr_e(inst, "%s: invalid buffer idx %d addr %#x data_offset %d\n",
			__func__, buffer->index, buffer->base_address,
			buffer->data_offset);
		return -EINVAL;
	}

	/* attach dequeued flag for, only last frame in the batch */
	if (msm_vidc_is_super_buffer(inst)) {
		frame_size = call_session_op(core, buffer_size, inst, MSM_VIDC_BUF_INPUT);
		batch_size = inst->capabilities->cap[SUPER_FRAME].value;
		if (!frame_size || !batch_size) {
			i_vpr_e(inst, "%s: invalid size: frame %u, batch %u\n",
				__func__, frame_size, batch_size);
			return -EINVAL;
		}
		if (buffer->addr_offset / frame_size < batch_size - 1) {
			i_vpr_l(inst, "%s: superframe last buffer not reached: %u, %u, %u\n",
				__func__, buffer->addr_offset, frame_size, batch_size);
			return 0;
		}
	}

	if (!(buf->attr & MSM_VIDC_ATTR_QUEUED)) {
		print_vidc_buffer(VIDC_ERR, "err ", "not queued", inst, buf);
		return 0;
	}

	buf->data_size = buffer->data_size;
	buf->attr &= ~MSM_VIDC_ATTR_QUEUED;
	buf->attr |= MSM_VIDC_ATTR_DEQUEUED;

	buf->flags = 0;
	buf->flags = get_driver_buffer_flags(inst, buffer->flags);

	/* handle ts_reorder for no_output prop attached input buffer */
	if (is_ts_reorder_allowed(inst) && inst->hfi_frame_info.no_output) {
		i_vpr_h(inst, "%s: received no_output buffer. remove timestamp %lld\n",
			__func__, buf->timestamp);
		msm_vidc_ts_reorder_remove_timestamp(inst, buf->timestamp);
	}

	print_vidc_buffer(VIDC_HIGH, "high", "dqbuf", inst, buf);
	msm_vidc_update_stats(inst, buf, MSM_VIDC_DEBUGFS_EVENT_EBD);

	/* etd: update end timestamp and flags in stats entry */
	if (!msm_vidc_is_super_buffer(inst))
		msm_vidc_remove_buffer_stats(inst, buf);

	return rc;
}

static int handle_output_buffer(struct msm_vidc_inst *inst,
	struct hfi_buffer *buffer)
{
	int cnt, rc = 0;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buf;
	bool found, fatal = false;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	/* handle drain last flag buffer */
	if (buffer->flags & HFI_BUF_FW_FLAG_LAST) {
		rc = handle_drain_last_flag_buffer(inst, buffer);
		if (rc)
			msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	}

	if (is_decode_session(inst)) {
		/* handle psc last flag buffer */
		if (buffer->flags & HFI_BUF_FW_FLAG_PSC_LAST) {
			rc = handle_psc_last_flag_buffer(inst, buffer);
			if (rc)
				msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		}
		/* handle non-read only buffer */
		if (!(buffer->flags & HFI_BUF_FW_FLAG_READONLY)) {
			rc = handle_non_read_only_buffer(inst, buffer);
			if (rc)
				msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		}
	}

	buffers = msm_vidc_get_buffers(inst, MSM_VIDC_BUF_OUTPUT, __func__);
	if (!buffers)
		return -EINVAL;

	found = false;
	list_for_each_entry(buf, &buffers->list, list) {
		if (is_decode_session(inst))
			found = (buf->device_addr == buffer->base_address &&
				buf->data_offset == buffer->data_offset);
		else
			found = (buf->index == buffer->index);

		if (found)
			break;
	}
	if (!found)
		return 0;

	if (!(buf->attr & MSM_VIDC_ATTR_QUEUED)) {
		print_vidc_buffer(VIDC_ERR, "err ", "not queued", inst, buf);
		return 0;
	}

	buf->data_offset = buffer->data_offset;
	buf->data_size = buffer->data_size;
	buf->timestamp = buffer->timestamp;

	buf->attr &= ~MSM_VIDC_ATTR_QUEUED;
	buf->attr |= MSM_VIDC_ATTR_DEQUEUED;

	if (is_encode_session(inst)) {
		/* encoder output is not expected to be corrupted */
		if (inst->hfi_frame_info.data_corrupt) {
			i_vpr_e(inst, "%s: encode output is corrupted\n", __func__);
			fatal = true;
		}
		if (inst->hfi_frame_info.overflow) {
			/* overflow not expected for image session */
			if (is_image_session(inst)) {
				i_vpr_e(inst, "%s: overflow detected for an image session\n",
					__func__);
				fatal = true;
			}

			/* overflow not expected for cbr_cfr session */
			if (!buffer->data_size && inst->hfi_rc_type == HFI_RC_CBR_CFR) {
				i_vpr_e(inst, "%s: overflow detected for cbr_cfr session\n",
					__func__);
				fatal = true;
			}
		}
		if (fatal)
			msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	}

	/*
	 * reset data size to zero for last flag buffer.
	 * reset RO flag for last flag buffer.
	 */
	if ((buffer->flags & HFI_BUF_FW_FLAG_LAST) ||
		(buffer->flags & HFI_BUF_FW_FLAG_PSC_LAST)) {
		if (buffer->data_size) {
			i_vpr_e(inst, "%s: reset data size to zero for last flag buffer\n",
				__func__);
			buf->data_size = 0;
		}
		if (buffer->flags & HFI_BUF_FW_FLAG_READONLY) {
			i_vpr_e(inst, "%s: reset RO flag for last flag buffer\n",
				__func__);
			buffer->flags &= ~HFI_BUF_FW_FLAG_READONLY;
		}
	}

	if (is_decode_session(inst)) {
		/* RO flag is not expected for linear colorformat */
		if (is_linear_colorformat(inst->capabilities->cap[PIX_FMTS].value) &&
			(buffer->flags & HFI_BUF_FW_FLAG_READONLY)) {
			buffer->flags &= ~HFI_BUF_FW_FLAG_READONLY;
			print_vidc_buffer(
				VIDC_HIGH, "high", "RO flag in linear colorformat", inst, buf);
		}

		if (buffer->flags & HFI_BUF_FW_FLAG_READONLY) {
			buf->attr |= MSM_VIDC_ATTR_READ_ONLY;
			rc = handle_read_only_buffer(inst, buf);
			if (rc)
				msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		} else {
			buf->attr &= ~MSM_VIDC_ATTR_READ_ONLY;
		}
	}

	buf->flags = 0;
	buf->flags = get_driver_buffer_flags(inst, buffer->flags);

	/* fence signalling */
	/* FW sending O/P with Fence Id will not be guranteed to be in order while stream off
	 * State check is added to receive fence id irrespective of the order
	 */
	for (cnt = 0; cnt < inst->hfi_frame_info.fence_count; cnt++) {
		if (inst->hfi_frame_info.fence_id[cnt] > inst->prev_fence_id ||
		    inst->state == MSM_VIDC_OUTPUT_STREAMING) {
			if (buf->data_size)
				msm_vidc_fence_signal(inst, inst->hfi_frame_info.fence_id[cnt]);
			else
				msm_vidc_fence_destroy(inst, inst->hfi_frame_info.fence_id[cnt]);
			inst->fences_per_output_counter++;
			inst->prev_fence_id = inst->hfi_frame_info.fence_id[cnt];
		} else {
			i_vpr_e(inst, "%s: invalid fence id %u, prev fence id %u\n",
				__func__, inst->hfi_frame_info.fence_id[cnt], inst->prev_fence_id);
			msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		}
	}

	/* validate firmware returned all the fences or not */
	if (inst->fences_per_output_counter >= buf->fence_count) {
		/* reset fence_count value after FBD handling */
		inst->fences_per_output_counter -= buf->fence_count;
	} else {
		i_vpr_e(inst, "%s: fence count mismatch. value %d, min expected %d\n",
			__func__, inst->fences_per_output_counter, buf->fence_count);
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	}

	if (is_decode_session(inst)) {
		inst->power.fw_cr = inst->hfi_frame_info.cr;
		inst->power.fw_cf = inst->hfi_frame_info.cf;
	} else {
		inst->power.fw_cr = inst->hfi_frame_info.cr;
	}

	if (!is_image_session(inst) && is_decode_session(inst) && buf->data_size)
		msm_vidc_update_timestamp_rate(inst, buf->timestamp);

	/* update output buffer timestamp, if ts_reorder is enabled */
	if (is_ts_reorder_allowed(inst) && buf->data_size)
		msm_vidc_ts_reorder_get_first_timestamp(inst, &buf->timestamp);

	print_vidc_buffer(VIDC_HIGH, "high", "dqbuf", inst, buf);
	msm_vidc_update_stats(inst, buf, MSM_VIDC_DEBUGFS_EVENT_FBD);

	/* fbd: print stats and remove entry */
	if (!msm_vidc_is_super_buffer(inst))
		msm_vidc_remove_buffer_stats(inst, buf);

	return rc;
}

static int handle_input_metadata_buffer(struct msm_vidc_inst *inst,
	struct hfi_buffer *buffer)
{
	int rc = 0;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buf;
	struct msm_vidc_core *core;
	u32 frame_size, batch_size;
	bool found;

	if (!inst || !buffer || !inst->capabilities || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	buffers = msm_vidc_get_buffers(inst, MSM_VIDC_BUF_INPUT_META, __func__);
	if (!buffers)
		return -EINVAL;

	found = false;
	list_for_each_entry(buf, &buffers->list, list) {
		if (buf->index == buffer->index) {
			found = true;
			break;
		}
	}
	if (!found) {
		i_vpr_e(inst, "%s: invalid idx %d daddr %#x data_offset %d\n",
			__func__, buffer->index, buffer->base_address,
			buffer->data_offset);
		return -EINVAL;
	}
	/* attach dequeued flag for, only last frame in the batch */
	if (msm_vidc_is_super_buffer(inst)) {
		frame_size = call_session_op(core, buffer_size, inst, MSM_VIDC_BUF_INPUT_META);
		batch_size = inst->capabilities->cap[SUPER_FRAME].value;
		if (!frame_size || !batch_size) {
			i_vpr_e(inst, "%s: invalid size: frame %u, batch %u\n",
				__func__, frame_size, batch_size);
			return -EINVAL;
		}
		if (buffer->addr_offset / frame_size < batch_size - 1) {
			i_vpr_l(inst, "%s: superframe last buffer not reached: %u, %u, %u\n",
				__func__, buffer->addr_offset, frame_size, batch_size);
			return 0;
		}
	}

	if (!(buf->attr & MSM_VIDC_ATTR_QUEUED)) {
		print_vidc_buffer(VIDC_ERR, "err ", "not queued", inst, buf);
		return 0;
	}

	buf->data_size = buffer->data_size;
	buf->attr &= ~MSM_VIDC_ATTR_QUEUED;
	buf->attr |= MSM_VIDC_ATTR_DEQUEUED;
	buf->flags = 0;
	if ((is_encode_session(inst) &&
		(buffer->flags & HFI_BUF_FW_FLAG_LAST)) ||
		(is_decode_session(inst) &&
		!inst->capabilities->cap[LAST_FLAG_EVENT_ENABLE].value &&
		((buffer->flags & HFI_BUF_FW_FLAG_LAST) ||
		(buffer->flags & HFI_BUF_FW_FLAG_PSC_LAST))))
		buf->flags |= MSM_VIDC_BUF_FLAG_LAST;

	print_vidc_buffer(VIDC_LOW, "low ", "dqbuf", inst, buf);
	return rc;
}

static int handle_output_metadata_buffer(struct msm_vidc_inst *inst,
	struct hfi_buffer *buffer)
{
	int rc = 0;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buf;
	bool found;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	buffers = msm_vidc_get_buffers(inst, MSM_VIDC_BUF_OUTPUT_META, __func__);
	if (!buffers)
		return -EINVAL;

	found = false;
	list_for_each_entry(buf, &buffers->list, list) {
		if (buf->index == buffer->index) {
			found = true;
			break;
		}
	}
	if (!found) {
		i_vpr_e(inst, "%s: invalid idx %d daddr %#x data_offset %d\n",
			__func__, buffer->index, buffer->base_address,
			buffer->data_offset);
		return -EINVAL;
	}

	if (!(buf->attr & MSM_VIDC_ATTR_QUEUED)) {
		print_vidc_buffer(VIDC_ERR, "err ", "not queued", inst, buf);
		return 0;
	}

	buf->data_size = buffer->data_size;
	buf->attr &= ~MSM_VIDC_ATTR_QUEUED;
	buf->attr |= MSM_VIDC_ATTR_DEQUEUED;
	buf->flags = 0;
	if ((is_encode_session(inst) &&
		(buffer->flags & HFI_BUF_FW_FLAG_LAST)) ||
		(is_decode_session(inst) &&
		!inst->capabilities->cap[LAST_FLAG_EVENT_ENABLE].value &&
		((buffer->flags & HFI_BUF_FW_FLAG_LAST) ||
		(buffer->flags & HFI_BUF_FW_FLAG_PSC_LAST))))
		buf->flags |= MSM_VIDC_BUF_FLAG_LAST;

	print_vidc_buffer(VIDC_LOW, "low ", "dqbuf", inst, buf);
	return rc;
}

static bool is_metabuffer_dequeued(struct msm_vidc_inst *inst,
	struct msm_vidc_buffer *buf)
{
	bool found = false;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buffer;
	enum msm_vidc_buffer_type buffer_type;

	if (is_input_buffer(buf->type) && is_input_meta_enabled(inst))
		buffer_type = MSM_VIDC_BUF_INPUT_META;
	else if (is_output_buffer(buf->type) && is_output_meta_enabled(inst))
		buffer_type = MSM_VIDC_BUF_OUTPUT_META;
	else
		return true;

	buffers = msm_vidc_get_buffers(inst, buffer_type, __func__);
	if (!buffers)
		return false;

	list_for_each_entry(buffer, &buffers->list, list) {
		if (buffer->index == buf->index &&
			buffer->attr & MSM_VIDC_ATTR_DEQUEUED) {
			found = true;
			break;
		}
	}
	return found;
}

static int msm_vidc_check_meta_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int i;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buf;
	static const enum msm_vidc_buffer_type buffer_type[] = {
		MSM_VIDC_BUF_INPUT,
		MSM_VIDC_BUF_OUTPUT,
	};

	for (i = 0; i < ARRAY_SIZE(buffer_type); i++) {
		/*
		 * skip input meta buffers check as meta buffers were
		 * already delivered if output fence enabled.
		 */
		if (is_meta_rx_inp_enabled(inst, META_OUTBUF_FENCE)) {
			if (buffer_type[i] == MSM_VIDC_BUF_INPUT)
				continue;
		}
		buffers = msm_vidc_get_buffers(inst, buffer_type[i], __func__);
		if (!buffers)
			return -EINVAL;

		list_for_each_entry(buf, &buffers->list, list) {
			if (buf->attr & MSM_VIDC_ATTR_DEQUEUED) {
				if (!is_metabuffer_dequeued(inst, buf)) {
					print_vidc_buffer(VIDC_ERR, "err ",
						"meta not dequeued", inst, buf);
					return -EINVAL;
				}
			}
		}
	}
	return rc;
}

static int handle_dequeue_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int i;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buf;
	struct msm_vidc_buffer *dummy;
	static const enum msm_vidc_buffer_type buffer_type[] = {
		MSM_VIDC_BUF_INPUT_META,
		MSM_VIDC_BUF_INPUT,
		MSM_VIDC_BUF_OUTPUT_META,
		MSM_VIDC_BUF_OUTPUT,
	};

	/* check metabuffers dequeued before sending vb2_buffer_done() */
	rc = msm_vidc_check_meta_buffers(inst);
	if (rc)
		return rc;

	for (i = 0; i < ARRAY_SIZE(buffer_type); i++) {
		buffers = msm_vidc_get_buffers(inst, buffer_type[i], __func__);
		if (!buffers)
			return -EINVAL;

		list_for_each_entry_safe(buf, dummy, &buffers->list, list) {
			if (buf->attr & MSM_VIDC_ATTR_DEQUEUED) {
				buf->attr &= ~MSM_VIDC_ATTR_DEQUEUED;
				/*
				 * do not send vb2_buffer_done when fw returns
				 * same buffer again
				 */
				if (buf->attr & MSM_VIDC_ATTR_BUFFER_DONE) {
					print_vidc_buffer(VIDC_HIGH, "high",
						"vb2 done already", inst, buf);
				} else {
					buf->attr |= MSM_VIDC_ATTR_BUFFER_DONE;
					rc = msm_vidc_vb2_buffer_done(inst, buf);
					if (rc) {
						print_vidc_buffer(VIDC_HIGH, "err ",
							"vb2 done failed", inst, buf);
						/* ignore the error */
						rc = 0;
					}
				}
				msm_vidc_put_driver_buf(inst, buf);
			}
		}
	}

	return rc;
}

static int handle_release_internal_buffer(struct msm_vidc_inst *inst,
	struct hfi_buffer *buffer)
{
	int rc = 0;
	struct msm_vidc_buffers *buffers;
	struct msm_vidc_buffer *buf;
	bool found;

	buffers = msm_vidc_get_buffers(inst, hfi_buf_type_to_driver(inst->domain,
		buffer->type, HFI_PORT_NONE), __func__);
	if (!buffers)
		return -EINVAL;

	found = false;
	list_for_each_entry(buf, &buffers->list, list) {
		if (buf->device_addr == buffer->base_address) {
			found = true;
			break;
		}
	}

	if (!is_internal_buffer(buf->type))
		return 0;

	if (found) {
		rc = msm_vidc_destroy_internal_buffer(inst, buf);
		if (rc)
			return rc;
	} else {
		i_vpr_e(inst, "%s: invalid idx %d daddr %#x\n",
			__func__, buffer->index, buffer->base_address);
		return -EINVAL;
	}
	return rc;
}

static int handle_release_output_buffer(struct msm_vidc_inst *inst,
	struct hfi_buffer *buffer, enum hfi_packet_port_type port_type)
{
	int rc = 0;
	struct msm_vidc_buffer *buf;
	bool found = false;

	list_for_each_entry(buf, &inst->buffers.release.list, list) {
		if (buf->device_addr == buffer->base_address) {
			found = true;
			break;
		}
	}
	if (!found) {
		i_vpr_e(inst, "%s: invalid idx %d daddr %#x\n",
			__func__, buffer->index, buffer->base_address);
		return -EINVAL;
	}

	rc = msm_vdec_handle_release_buffer(inst, buf);
	if (rc)
		return rc;

	return rc;
}

static int handle_session_buffer(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int i, rc = 0;
	struct hfi_buffer *buffer;
	u32 hfi_handle_size = 0;
	const struct msm_vidc_hfi_buffer_handle *hfi_handle_arr = NULL;
	static const struct msm_vidc_hfi_buffer_handle enc_input_hfi_handle[] = {
		{HFI_BUFFER_METADATA,       handle_input_metadata_buffer      },
		{HFI_BUFFER_RAW,            handle_input_buffer               },
		{HFI_BUFFER_VPSS,           handle_release_internal_buffer    },
	};
	static const struct msm_vidc_hfi_buffer_handle enc_output_hfi_handle[] = {
		{HFI_BUFFER_METADATA,       handle_output_metadata_buffer     },
		{HFI_BUFFER_BITSTREAM,      handle_output_buffer              },
		{HFI_BUFFER_BIN,            handle_release_internal_buffer    },
		{HFI_BUFFER_COMV,           handle_release_internal_buffer    },
		{HFI_BUFFER_NON_COMV,       handle_release_internal_buffer    },
		{HFI_BUFFER_LINE,           handle_release_internal_buffer    },
		{HFI_BUFFER_ARP,            handle_release_internal_buffer    },
		{HFI_BUFFER_DPB,            handle_release_internal_buffer    },
	};
	static const struct msm_vidc_hfi_buffer_handle dec_input_hfi_handle[] = {
		{HFI_BUFFER_METADATA,       handle_input_metadata_buffer      },
		{HFI_BUFFER_BITSTREAM,      handle_input_buffer               },
		{HFI_BUFFER_BIN,            handle_release_internal_buffer    },
		{HFI_BUFFER_COMV,           handle_release_internal_buffer    },
		{HFI_BUFFER_NON_COMV,       handle_release_internal_buffer    },
		{HFI_BUFFER_LINE,           handle_release_internal_buffer    },
		{HFI_BUFFER_PERSIST,        handle_release_internal_buffer    },
		{HFI_BUFFER_PARTIAL_DATA,   handle_release_internal_buffer    },
	};
	static const struct msm_vidc_hfi_buffer_handle dec_output_hfi_handle[] = {
		{HFI_BUFFER_METADATA,       handle_output_metadata_buffer     },
		{HFI_BUFFER_RAW,            handle_output_buffer              },
		{HFI_BUFFER_DPB,            handle_release_internal_buffer    },
	};

	if (pkt->payload_info == HFI_PAYLOAD_NONE) {
		i_vpr_h(inst, "%s: received hfi buffer packet without payload\n",
			__func__);
		return 0;
	}

	if (!check_for_packet_payload(inst, pkt, __func__)) {
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		return 0;
	}

	buffer = (struct hfi_buffer *)((u8 *)pkt + sizeof(struct hfi_packet));
	if (!is_valid_hfi_buffer_type(inst, buffer->type, __func__)) {
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		return 0;
	}

	if (!is_valid_hfi_port(inst, pkt->port, buffer->type, __func__)) {
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		return 0;
	}
	if (is_decode_session(inst) && buffer->type == HFI_BUFFER_RAW &&
		buffer->flags & HFI_BUF_FW_FLAG_RELEASE_DONE)
		return handle_release_output_buffer(inst, buffer, pkt->port);

	if (is_encode_session(inst)) {
		if (pkt->port == HFI_PORT_RAW) {
			hfi_handle_size = ARRAY_SIZE(enc_input_hfi_handle);
			hfi_handle_arr = enc_input_hfi_handle;
		} else if (pkt->port == HFI_PORT_BITSTREAM) {
			hfi_handle_size = ARRAY_SIZE(enc_output_hfi_handle);
			hfi_handle_arr = enc_output_hfi_handle;
		}
	} else if (is_decode_session(inst)) {
		if (pkt->port == HFI_PORT_BITSTREAM) {
			hfi_handle_size = ARRAY_SIZE(dec_input_hfi_handle);
			hfi_handle_arr = dec_input_hfi_handle;
		} else if (pkt->port == HFI_PORT_RAW) {
			hfi_handle_size = ARRAY_SIZE(dec_output_hfi_handle);
			hfi_handle_arr = dec_output_hfi_handle;
		}
	}

	/* handle invalid session */
	if (!hfi_handle_arr || !hfi_handle_size) {
		i_vpr_e(inst, "%s: invalid session %d\n", __func__, inst->domain);
		return -EINVAL;
	}

	/* handle session buffer */
	for (i = 0; i < hfi_handle_size; i++) {
		if (hfi_handle_arr[i].type == buffer->type) {
			rc = hfi_handle_arr[i].handle(inst, buffer);
			if (rc)
				return rc;
			break;
		}
	}

	/* handle unknown buffer type */
	if (i == hfi_handle_size) {
		i_vpr_e(inst, "%s: port %u, unknown buffer type %#x\n", __func__,
			pkt->port, buffer->type);
		return -EINVAL;
	}

	return rc;
}

static int handle_input_port_settings_change(struct msm_vidc_inst *inst)
{
	int rc = 0;
	enum msm_vidc_allow allow = MSM_VIDC_DISALLOW;

	allow = msm_vidc_allow_input_psc(inst);
	if (allow == MSM_VIDC_DISALLOW) {
		return -EINVAL;
	} else if (allow == MSM_VIDC_ALLOW) {
		rc = msm_vidc_state_change_input_psc(inst);
		if (rc)
			return rc;
		print_psc_properties("INPUT_PSC", inst, inst->subcr_params[INPUT_PORT]);
		rc = msm_vdec_input_port_settings_change(inst);
		if (rc)
			return rc;
	}

	return rc;
}

static int handle_output_port_settings_change(struct msm_vidc_inst *inst)
{
	int rc = 0;

	print_psc_properties("OUTPUT_PSC", inst, inst->subcr_params[OUTPUT_PORT]);
	rc = msm_vdec_output_port_settings_change(inst);
	if (rc)
		return rc;

	return rc;
}

static int handle_port_settings_change(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int rc = 0;

	i_vpr_h(inst, "%s: Received port settings change, type %d\n",
		__func__, pkt->port);

	if (pkt->port == HFI_PORT_RAW) {
		rc = handle_output_port_settings_change(inst);
		if (rc)
			goto exit;
	} else if (pkt->port == HFI_PORT_BITSTREAM) {
		rc = handle_input_port_settings_change(inst);
		if (rc)
			goto exit;
	} else {
		i_vpr_e(inst, "%s: invalid port type: %#x\n",
			__func__, pkt->port);
		rc = -EINVAL;
		goto exit;
	}

exit:
	if (rc)
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	return rc;
}

static int handle_session_subscribe_mode(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);
	return 0;
}

static int handle_session_delivery_mode(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);
	return 0;
}

static int handle_session_pause(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);
	return 0;
}

static int handle_session_resume(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);
	return 0;
}

static int handle_session_stability(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	if (pkt->flags & HFI_FW_FLAGS_SUCCESS)
		i_vpr_h(inst, "%s: successful\n", __func__);
	return 0;
}

static int handle_session_early_notify_partial_frame(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	u32 port;
	u64 fence_id = 0;
	int rc = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	port = vidc_port_from_hfi(inst, pkt->port);
	if (port >= MAX_PORT) {
		i_vpr_e(inst,
			"%s: invalid port: %d\n", __func__, pkt->port);
		return -EINVAL;
	}

	if (!check_for_packet_payload(inst, pkt, __func__)) {
		i_vpr_e(inst, "%s: invalid packet\n", __func__);
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		return 0;
	}

	fence_id = *(u64 *)((u8 *)pkt + sizeof(struct hfi_packet));
	if (fence_id < inst->prev_fence_id) {
		i_vpr_e(inst, "%s: invalid fence id %u, prev fence id %u\n",
			__func__, fence_id, inst->prev_fence_id);
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		return -EINVAL;
	}
	rc = msm_vidc_fence_signal(inst, fence_id);
	if (rc) {
		i_vpr_e(inst,
			"%s: fence signal failed. invalid fence_id %llu, count %d\n",
			__func__, fence_id, inst->fences_per_output_counter);
		return rc;
	}
	inst->fences_per_output_counter++;
	inst->prev_fence_id = fence_id;

	i_vpr_l(inst, "%s: received fence id %llu, count %d\n",
		__func__, fence_id, inst->fences_per_output_counter);

	return 0;
}

static int handle_session_command(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int i, rc;
	static const struct msm_vidc_hfi_packet_handle hfi_pkt_handle[] = {
		{HFI_CMD_OPEN,              handle_session_open               },
		{HFI_CMD_CLOSE,             handle_session_close              },
		{HFI_CMD_START,             handle_session_start              },
		{HFI_CMD_STOP,              handle_session_stop               },
		{HFI_CMD_DRAIN,             handle_session_drain              },
		{HFI_CMD_BUFFER,            handle_session_buffer             },
		{HFI_CMD_SETTINGS_CHANGE,   handle_port_settings_change       },
		{HFI_CMD_SUBSCRIBE_MODE,    handle_session_subscribe_mode     },
		{HFI_CMD_DELIVERY_MODE,     handle_session_delivery_mode      },
		{HFI_CMD_PAUSE,             handle_session_pause              },
		{HFI_CMD_RESUME,            handle_session_resume             },
		{HFI_CMD_STABILITY,         handle_session_stability          },
		{HFI_CMD_EARLY_NOTIFY_PARTIAL_FRAME,
		    handle_session_early_notify_partial_frame                 },
	};

	/* handle session pkt */
	for (i = 0; i < ARRAY_SIZE(hfi_pkt_handle); i++) {
		if (hfi_pkt_handle[i].type == pkt->type) {
			rc = hfi_pkt_handle[i].handle(inst, pkt);
			if (rc)
				return rc;
			break;
		}
	}

	/* handle unknown buffer type */
	if (i == ARRAY_SIZE(hfi_pkt_handle)) {
		i_vpr_e(inst, "%s: Unsupported command type: %#x\n", __func__, pkt->type);
		return -EINVAL;
	}

	return 0;
}

static int handle_dpb_list_property(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	u32 payload_size, num_words_in_payload;
	u8 *payload_start;
	int i = 0;

	payload_size = pkt->size - sizeof(struct hfi_packet);
	num_words_in_payload = payload_size / 4;
	payload_start = (u8 *)((u8 *)pkt + sizeof(struct hfi_packet));
	memset(inst->dpb_list_payload, 0, MAX_DPB_LIST_ARRAY_SIZE);

	if (payload_size > MAX_DPB_LIST_PAYLOAD_SIZE) {
		i_vpr_e(inst,
			"%s: dpb list payload size %d exceeds expected max size %d\n",
			__func__, payload_size, MAX_DPB_LIST_PAYLOAD_SIZE);
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		return -EINVAL;
	}
	memcpy(inst->dpb_list_payload, payload_start, payload_size);

	for (i = 0; (i + 3) < num_words_in_payload; i = i + 4) {
		i_vpr_l(inst,
			"%s: base addr %#x %#x, addr offset %#x, data offset %#x\n",
			__func__, inst->dpb_list_payload[i], inst->dpb_list_payload[i + 1],
			inst->dpb_list_payload[i + 2], inst->dpb_list_payload[i + 3]);
	}

	return 0;
}

static int handle_property_fence_array(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	u64 cur_fence_id = 0, prev_fence_id = 0;
	int i = 0, fence_count = 0;
	u32 payload_size;
	u8 *payload_start;

	if (!inst || !pkt || !inst->capabilities) {
		i_vpr_e(inst, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	payload_size = pkt->size - sizeof(struct hfi_packet);
	fence_count = payload_size / sizeof(u64);
	payload_start = (u8 *)((u8 *)pkt + sizeof(struct hfi_packet));

	if (payload_size > sizeof(inst->hfi_frame_info.fence_id)) {
		i_vpr_e(inst,
			"%s: fence list payload size %d exceeds expected max size %d\n",
			__func__, payload_size, sizeof(inst->hfi_frame_info.fence_id));
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		return -EINVAL;
	}

	for (i = 0; i < fence_count; i++) {
		cur_fence_id = *((u64 *)payload_start + i);
		if (prev_fence_id > cur_fence_id) {
			i_vpr_e(inst, "%s: invalid fence id %llu, prev %llu\n",
				__func__, cur_fence_id, prev_fence_id);
			msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
			return -EINVAL;
		}
		inst->hfi_frame_info.fence_id[i] = cur_fence_id;
		inst->hfi_frame_info.fence_count++;
		prev_fence_id = cur_fence_id;
	}
	i_vpr_l(inst, "%s: fence_id[0] %llu, received %d, expected %d\n", __func__,
		inst->hfi_frame_info.fence_id[0], inst->fences_per_output_counter,
		inst->hfi_frame_info.fence_count);

	return 0;
}

static int handle_property_with_payload(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt, u32 port)
{
	int rc = 0;
	u32 *payload_ptr = NULL;

	payload_ptr = (u32 *)((u8 *)pkt + sizeof(struct hfi_packet));
	if (!payload_ptr) {
		i_vpr_e(inst,
			"%s: payload_ptr cannot be null\n", __func__);
		return -EINVAL;
	}

	switch (pkt->type) {
	case HFI_PROP_BITSTREAM_RESOLUTION:
		inst->subcr_params[port].bitstream_resolution = payload_ptr[0];
		break;
	case HFI_PROP_CROP_OFFSETS:
		inst->subcr_params[port].crop_offsets[0] = payload_ptr[0];
		inst->subcr_params[port].crop_offsets[1] = payload_ptr[1];
		break;
	case HFI_PROP_LUMA_CHROMA_BIT_DEPTH:
		inst->subcr_params[port].bit_depth = payload_ptr[0];
		break;
	case HFI_PROP_CODED_FRAMES:
		inst->subcr_params[port].coded_frames = payload_ptr[0];
		break;
	case HFI_PROP_BUFFER_FW_MIN_OUTPUT_COUNT:
		inst->subcr_params[port].fw_min_count = payload_ptr[0];
		break;
	case HFI_PROP_PIC_ORDER_CNT_TYPE:
		inst->subcr_params[port].pic_order_cnt = payload_ptr[0];
		break;
	case HFI_PROP_SIGNAL_COLOR_INFO:
		inst->subcr_params[port].color_info = payload_ptr[0];
		break;
	case HFI_PROP_PROFILE:
		inst->subcr_params[port].profile = payload_ptr[0];
		break;
	case HFI_PROP_LEVEL:
		inst->subcr_params[port].level = payload_ptr[0];
		break;
	case HFI_PROP_TIER:
		inst->subcr_params[port].tier = payload_ptr[0];
		break;
	case HFI_PROP_AV1_FILM_GRAIN_PRESENT:
		inst->subcr_params[port].av1_film_grain_present = payload_ptr[0];
		break;
	case HFI_PROP_AV1_SUPER_BLOCK_ENABLED:
		inst->subcr_params[port].av1_super_block_enabled = payload_ptr[0];
		break;
	case HFI_PROP_PICTURE_TYPE:
		inst->hfi_frame_info.picture_type = payload_ptr[0];
		if (inst->hfi_frame_info.picture_type & HFI_PICTURE_B)
			inst->has_bframe = true;
		if (inst->hfi_frame_info.picture_type & HFI_PICTURE_IDR)
			inst->iframe = true;
		else
			inst->iframe = false;
		break;
	case HFI_PROP_SUBFRAME_INPUT:
		if (port != INPUT_PORT) {
			i_vpr_e(inst,
				"%s: invalid port: %d for property %#x\n",
				__func__, pkt->port, pkt->type);
			break;
		}
		inst->hfi_frame_info.subframe_input = 1;
		break;
	case HFI_PROP_WORST_COMPRESSION_RATIO:
		inst->hfi_frame_info.cr = payload_ptr[0];
		break;
	case HFI_PROP_WORST_COMPLEXITY_FACTOR:
		inst->hfi_frame_info.cf = payload_ptr[0];
		break;
	case HFI_PROP_CABAC_SESSION:
		if (payload_ptr[0] == 1)
			msm_vidc_update_cap_value(inst, ENTROPY_MODE,
				V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
				__func__);
		else
			msm_vidc_update_cap_value(inst, ENTROPY_MODE,
				V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
				__func__);
		break;
	case HFI_PROP_DPB_LIST:
		if (is_decode_session(inst) && port == OUTPUT_PORT &&
			inst->capabilities->cap[DPB_LIST].value) {
			rc = handle_dpb_list_property(inst, pkt);
			if (rc)
				break;
		} else {
			i_vpr_e(inst,
				"%s: invalid property %#x for %s port %d dpb cap value %d\n",
				__func__, pkt->type, is_decode_session(inst) ? "decode" : "encode",
				port, inst->capabilities->cap[DPB_LIST].value);
		}
		break;
	case HFI_PROP_QUALITY_MODE:
		if (inst->capabilities->cap[QUALITY_MODE].value !=  payload_ptr[0])
			i_vpr_e(inst,
				"%s: fw quality mode(%d) not matching the capability value(%d)\n",
				__func__,  payload_ptr[0],
				inst->capabilities->cap[QUALITY_MODE].value);
		break;
	case HFI_PROP_STAGE:
		if (inst->capabilities->cap[STAGE].value !=  payload_ptr[0])
			i_vpr_e(inst,
				"%s: fw stage mode(%d) not matching the capability value(%d)\n",
				__func__,  payload_ptr[0], inst->capabilities->cap[STAGE].value);
		break;
	case HFI_PROP_PIPE:
		if (inst->capabilities->cap[PIPE].value !=  payload_ptr[0])
			i_vpr_e(inst,
				"%s: fw pipe mode(%d) not matching the capability value(%d)\n",
				__func__,  payload_ptr[0], inst->capabilities->cap[PIPE].value);
		break;
	case HFI_PROP_FENCE:
		rc = handle_property_fence_array(inst, pkt);
		if (rc)
			break;
		break;
	default:
		i_vpr_e(inst, "%s: invalid property %#x\n",
			__func__, pkt->type);
		break;
	}

	return rc;
}

static int handle_property_without_payload(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt, u32 port)
{
	switch (pkt->type) {
	case HFI_PROP_NO_OUTPUT:
		if (port != INPUT_PORT) {
			i_vpr_e(inst,
				"%s: invalid port: %d for property %#x\n",
				__func__, pkt->port, pkt->type);
			break;
		}
		i_vpr_h(inst, "received no_output property\n");
		inst->hfi_frame_info.no_output = 1;
		break;
	default:
		i_vpr_e(inst, "%s: invalid property %#x\n",
			__func__, pkt->type);
		break;
	}

	return 0;
}

static int handle_session_property(struct msm_vidc_inst *inst,
	struct hfi_packet *pkt)
{
	int rc = 0;
	u32 port;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	i_vpr_l(inst, "%s: property type %#x\n", __func__, pkt->type);

	port = vidc_port_from_hfi(inst, pkt->port);
	if (port >= MAX_PORT) {
		i_vpr_e(inst,
			"%s: invalid port: %d for property %#x\n",
			__func__, pkt->port, pkt->type);
		return -EINVAL;
	}

	if (pkt->flags & HFI_FW_FLAGS_INFORMATION) {
		i_vpr_h(inst,
			"%s: information flag received for property %#x packet\n",
			__func__, pkt->type);
		return 0;
	}

	if (check_for_packet_payload(inst, pkt, __func__)) {
		rc = handle_property_with_payload(inst, pkt, port);
		if (rc)
			return rc;
	} else {
		rc = handle_property_without_payload(inst, pkt, port);
		if (rc)
			return rc;
	}

	return rc;
}

static int handle_image_version_property(struct msm_vidc_core *core,
	struct hfi_packet *pkt)
{
	u32 i = 0;
	u8 *str_image_version;
	u32 req_bytes;

	req_bytes = pkt->size - sizeof(*pkt);
	if (req_bytes < VENUS_VERSION_LENGTH - 1) {
		d_vpr_e("%s: bad_pkt: %d\n", __func__, req_bytes);
		return -EINVAL;
	}
	str_image_version = (u8 *)pkt + sizeof(struct hfi_packet);
	/*
	 * The version string returned by firmware includes null
	 * characters at the start and in between. Replace the null
	 * characters with space, to print the version info.
	 */
	for (i = 0; i < VENUS_VERSION_LENGTH - 1; i++) {
		if (str_image_version[i] != '\0')
			core->fw_version[i] = str_image_version[i];
		else
			core->fw_version[i] = ' ';
	}
	core->fw_version[i] = '\0';

	d_vpr_h("%s: F/W version: %s\n", __func__, core->fw_version);
	return 0;
}

static int handle_system_property(struct msm_vidc_core *core,
	struct hfi_packet *pkt)
{
	int rc = 0;

	switch (pkt->type) {
	case HFI_PROP_IMAGE_VERSION:
		rc = handle_image_version_property(core, pkt);
		break;
	default:
		d_vpr_h("%s: property type %#x successful\n",
			__func__, pkt->type);
		break;
	}
	return rc;
}

static int handle_system_response(struct msm_vidc_core *core,
	struct hfi_header *hdr)
{
	int rc = 0;
	struct hfi_packet *packet;
	u8 *pkt, *start_pkt;
	int i, j;
	static const struct msm_vidc_core_hfi_range be[] = {
		{HFI_SYSTEM_ERROR_BEGIN,   HFI_SYSTEM_ERROR_END,   handle_system_error     },
		{HFI_PROP_BEGIN,           HFI_PROP_END,           handle_system_property  },
		{HFI_CMD_BEGIN,            HFI_CMD_END,            handle_system_init      },
	};

	start_pkt = (u8 *)((u8 *)hdr + sizeof(struct hfi_header));
	for (i = 0; i < ARRAY_SIZE(be); i++) {
		pkt = start_pkt;
		for (j = 0; j < hdr->num_packets; j++) {
			packet = (struct hfi_packet *)pkt;
			/* handle system error */
			if (packet->flags & HFI_FW_FLAGS_SYSTEM_ERROR) {
				d_vpr_e("%s: received system error %#x\n",
					__func__, packet->type);
				rc = handle_system_error(core, packet);
				if (rc)
					goto exit;
				goto exit;
			}
			if (in_range(be[i], packet->type)) {
				rc = be[i].handle(core, packet);
				if (rc)
					goto exit;

				/* skip processing anymore packets after system error */
				if (!i) {
					d_vpr_e("%s: skip processing anymore packets\n", __func__);
					goto exit;
				}
			}
			pkt += packet->size;
		}
	}

exit:
	return rc;
}

static int __handle_session_response(struct msm_vidc_inst *inst,
	struct hfi_header *hdr)
{
	int rc = 0;
	struct hfi_packet *packet;
	u8 *pkt, *start_pkt;
	bool dequeue = false;
	int i, j;
	static const struct msm_vidc_inst_hfi_range be[] = {
		{HFI_SESSION_ERROR_BEGIN,  HFI_SESSION_ERROR_END,  handle_session_error    },
		{HFI_INFORMATION_BEGIN,    HFI_INFORMATION_END,    handle_session_info     },
		{HFI_PROP_BEGIN,           HFI_PROP_END,           handle_session_property },
		{HFI_CMD_BEGIN,            HFI_CMD_END,            handle_session_command  },
	};

	memset(&inst->hfi_frame_info, 0, sizeof(struct msm_vidc_hfi_frame_info));
	start_pkt = (u8 *)((u8 *)hdr + sizeof(struct hfi_header));
	for (i = 0; i < ARRAY_SIZE(be); i++) {
		pkt = start_pkt;
		for (j = 0; j < hdr->num_packets; j++) {
			packet = (struct hfi_packet *)pkt;
			/* handle session error */
			if (packet->flags & HFI_FW_FLAGS_SESSION_ERROR) {
				i_vpr_e(inst, "%s: received session error %#x\n",
					__func__, packet->type);
				handle_session_error(inst, packet);
			}
			if (in_range(be[i], packet->type)) {
				dequeue |= (packet->type == HFI_CMD_BUFFER);
				rc = be[i].handle(inst, packet);
				if (rc)
					msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
			}
			pkt += packet->size;
		}
	}

	if (dequeue) {
		rc = handle_dequeue_buffers(inst);
		if (rc)
			return rc;
	}
	memset(&inst->hfi_frame_info, 0, sizeof(struct msm_vidc_hfi_frame_info));

	return rc;
}

static int handle_session_response(struct msm_vidc_core *core,
	struct hfi_header *hdr)
{
	struct msm_vidc_inst *inst;
	struct hfi_packet *packet;
	u8 *pkt;
	int i, rc = 0;
	bool found_ipsc = false;

	if (!core || !hdr) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	inst = get_inst(core, hdr->session_id);
	if (!inst) {
		d_vpr_e("%s: Invalid inst\n", __func__);
		return -EINVAL;
	}

	inst_lock(inst, __func__);
	/* search for cmd settings change pkt */
	pkt = (u8 *)((u8 *)hdr + sizeof(struct hfi_header));
	for (i = 0; i < hdr->num_packets; i++) {
		packet = (struct hfi_packet *)pkt;
		if (packet->type == HFI_CMD_SETTINGS_CHANGE) {
			if (packet->port == HFI_PORT_BITSTREAM) {
				found_ipsc = true;
				break;
			}
		}
		pkt += packet->size;
	}

	/* if ipsc packet is found, initialise subsc_params */
	if (found_ipsc)
		msm_vdec_init_input_subcr_params(inst);

	rc = __handle_session_response(inst, hdr);
	if (rc)
		goto exit;

exit:
	inst_unlock(inst, __func__);
	put_inst(inst);
	return rc;
}

int handle_response(struct msm_vidc_core *core, void *response)
{
	struct hfi_header *hdr;
	int rc = 0;

	if (!core || !response) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hdr = (struct hfi_header *)response;
	rc = validate_hdr_packet(core, hdr, __func__);
	if (rc) {
		d_vpr_e("%s: hdr pkt validation failed\n", __func__);
		return handle_system_error(core, NULL);
	}

	if (!hdr->session_id)
		return handle_system_response(core, hdr);
	else
		return handle_session_response(core, hdr);

	return 0;
}
