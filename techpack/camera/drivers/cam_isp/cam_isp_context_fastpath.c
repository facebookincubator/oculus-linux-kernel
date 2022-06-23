// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

/* TODO clean the defines after the implementation is done */
#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ratelimit.h>
#include <linux/wait.h>
#include <linux/timekeeping.h>
#include <linux/bitops.h>
#include <uapi/media/cam_fastpath_uapi.h>

#include "cam_mem_mgr.h"
#include "cam_trace.h"
#include "cam_debug_util.h"
#include "cam_packet_util.h"
#include "cam_context_utils.h"
#include "cam_cdm_util.h"
#include "cam_isp_context_fastpath.h"
#include "cam_common_util.h"
#include "cam_fastpath_queue.h"

#define INVAL_HANDLE (-1)
static uint32_t ife_port_res_lut[] = {
	0x3000, // IFEOutputFull            = 0;
	0x3001, // IFEOutputDS4             = 1;
	0x3002, // IFEOutputDS16            = 2;
	0x3003, // IFEOutputCAMIFRaw        = 3;
	0x3003, // IFEOutputLSCRaw          = 4;
	0x3003, // IFEOutputGTMRaw          = 5;
	0x3004, // IFEOutputFD              = 6;
	0x3005, // IFEOutputPDAF            = 7;
	0x3006, // IFEOutputRDI0            = 8;
	0x3007, // IFEOutputRDI1            = 9;
	0x3008, // IFEOutputRDI2            = 10;
	0x3009, // IFEOutputRDI3            = 11;
	0x3010, // IFEOutputStatsRS         = 12;
	0x3011, // IFEOutputStatsCS         = 13;
	0x0000, // Empty                    = 14;
	0x3012, // IFEOutputStatsIHIST      = 15;
	0x300F, // IFEOutputStatsBHIST      = 16;
	0x300A, // IFEOutputStatsHDRBE      = 17;
	0x300B, // IFEOutputStatsHDRBHIST   = 18;
	0x300C, // IFEOutputStatsTLBG       = 19;
	0x300D, // IFEOutputStatsBF         = 20;
	0x300E, // IFEOutputStatsAWBBG      = 21;
	0x3013, // IFEOutputDisplayFull     = 22;
	0x3014, // IFEOutputDisplayDS4      = 23;
	0x3015, // IFEOutputDisplayDS16     = 24;
	0x3016, // IFEOutputStatsDualPD     = 25;
	0x3017, // IFEOutputRDIRD           = 26;
	0x3018, // IFEOutputLCR             = 27;
};

static int cam_isp_fpc_clear_queues(struct cam_isp_fastpath_context *ctx)
{
	struct cam_isp_fastpath_ctx_req *req_isp;
	struct cam_isp_fastpath_packet *fp_packet;
	struct list_head *pos, *next;

	/* Mask off all the incoming hardware events */
	mutex_lock(&ctx->mutex_list);

	list_for_each_safe(pos, next, &ctx->pending_packet_list) {
		fp_packet = list_entry(pos, struct cam_isp_fastpath_packet,
				       list);
		list_del(&fp_packet->list);
		kmem_cache_free(ctx->packet_cache, fp_packet);
	}

	list_for_each_safe(pos, next, &ctx->active_packet_list) {
		fp_packet = list_entry(pos, struct cam_isp_fastpath_packet,
				       list);
		list_del(&fp_packet->list);
		kmem_cache_free(ctx->packet_cache, fp_packet);
	}

	list_for_each_safe(pos, next, &ctx->pending_req_list) {
		req_isp = list_entry(pos, struct cam_isp_fastpath_ctx_req,
				     list);
		list_del(&req_isp->list);
		kmem_cache_free(ctx->request_cache, req_isp);
	}

	list_for_each_safe(pos, next, &ctx->active_req_list) {
		req_isp = list_entry(pos, struct cam_isp_fastpath_ctx_req,
				     list);
		list_del(&req_isp->list);
		kmem_cache_free(ctx->request_cache, req_isp);
	}
	ctx->num_in_active = 0;
	list_for_each_safe(pos, next, &ctx->process_req_list) {
		req_isp = list_entry(pos, struct cam_isp_fastpath_ctx_req,
				     list);
		list_del(&req_isp->list);
		kmem_cache_free(ctx->request_cache, req_isp);
	}
	ctx->num_in_processing = 0;

	mutex_unlock(&ctx->mutex_list);
	return 0;
}

/* Camera isp fastpath static functions */
static int cam_isp_fpc_start_dev(struct cam_isp_fastpath_context *ctx,
				 struct cam_start_stop_dev_cmd *cmd)
{
	struct cam_isp_fastpath_ctx_req *req_isp;
	struct cam_isp_start_args  start_isp;
	int rc = 0;

	if (!ctx->hw_ctx) {
		CAM_ERR(CAM_ISP, "Wrong hw context pointer.");
		rc = -EFAULT;
		goto end;
	}
	ctx->sof_cnt = 0;
	mutex_lock(&ctx->mutex_list);

	if (list_empty(&ctx->pending_req_list)) {
		/* should never happen */
		CAM_ERR(CAM_ISP, "Start device with empty configuration");
		rc = -EFAULT;
		goto end;
	}

	req_isp = list_first_entry(&ctx->pending_req_list,
		struct cam_isp_fastpath_ctx_req, list);

	memset(&start_isp, 0, sizeof(start_isp));
	start_isp.hw_config.ctxt_to_hw_map = ctx->hw_ctx;
	start_isp.hw_config.request_id = req_isp->request_id;
	start_isp.hw_config.hw_update_entries = req_isp->cfg;
	start_isp.hw_config.num_hw_update_entries = req_isp->num_cfg;
	start_isp.hw_config.priv  = &req_isp->hw_update_data;
	start_isp.hw_config.init_packet = 1;
	start_isp.hw_config.reapply = 0;

	/* TODO add handle for flush */
	start_isp.start_only = false;

	/*
	 * In case of CSID TPG we might receive SOF and RUP IRQs
	 * before hw_intf.hw_start has returned. So move
	 * req out of pending list before hw_start and add it
	 * back to pending list if hw_start fails.
	 */
	CAM_DBG(CAM_ISP, "start device success ctx %u", ctx->ctx_id);
	req_isp->state = CAM_ISP_HW_EVENT_SOF;
	list_del(&req_isp->list);
	list_add_tail(&req_isp->list, &ctx->active_req_list);
	ctx->num_in_active++;
	CAM_DBG(CAM_REQ,
		"Move pending req: %lld to active list(cnt: %d) ctx %u",
		req_isp->request_id, ctx->ctx_id);

	/*
	 * Only place to change state before calling the hw due to
	 * hardware tasklet has higher priority that can cause the
	 * irq handling comes early
	 */
	rc = ctx->hw_intf.hw_start(ctx->hw_intf.hw_mgr_priv,
		&start_isp);
	if (rc) {
		/* HW failure. user need to clean up the resource */
		CAM_ERR(CAM_ISP, "Start HW failed");
		list_del(&req_isp->list);
		list_add(&req_isp->list, &ctx->pending_req_list);
		goto end;
	}
	CAM_DBG(CAM_ISP, "start device success ctx %u",
		ctx->ctx_id);

end:
	if (req_isp && list_empty(&req_isp->list))
		kmem_cache_free(ctx->request_cache, req_isp);

	mutex_unlock(&ctx->mutex_list);

	return rc;
}

static int cam_isp_fpc_stop_dev(struct cam_isp_fastpath_context *ctx,
				struct cam_start_stop_dev_cmd *stop_cmd)
{
	/* stop hw first */
	if (ctx->hw_ctx) {
		struct cam_hw_stop_args stop;
		struct cam_isp_stop_args stop_isp;

		memset(&stop, 0, sizeof(stop));
		memset(&stop_isp, 0, sizeof(stop_isp));

		stop.ctxt_to_hw_map = ctx->hw_ctx;

		if (stop_cmd)
			stop_isp.hw_stop_cmd =
				CAM_ISP_HW_STOP_AT_FRAME_BOUNDARY;
		else
			stop_isp.hw_stop_cmd = CAM_ISP_HW_STOP_IMMEDIATELY;

		stop_isp.stop_only = false;
		stop.args = (void *) &stop_isp;
		ctx->hw_intf.hw_stop(ctx->hw_intf.hw_mgr_priv,
			&stop);
	}

	cam_isp_fpc_clear_queues(ctx);
	return 0;
}

static int cam_isp_fpc_enqueue_request(struct cam_isp_fastpath_context *ctx,
				       struct cam_isp_fastpath_ctx_req *req)
{
	mutex_lock(&ctx->mutex_list);
	list_add_tail(&req->list, &ctx->pending_req_list);
	mutex_unlock(&ctx->mutex_list);
	return 0;
}

static int
cam_isp_fpc_enqueue_init_request(struct cam_isp_fastpath_context *ctx,
				 struct cam_isp_fastpath_ctx_req *req)
{
	struct cam_isp_prepare_hw_update_data *hw_update_data;
	struct cam_isp_prepare_hw_update_data *req_update_old;
	struct cam_isp_prepare_hw_update_data *req_update_new;
	struct cam_isp_fastpath_ctx_req *req_old;
	int rc = 0;

	mutex_lock(&ctx->mutex_list);
	if (list_empty(&ctx->pending_req_list)) {
		list_add_tail(&req->list, &ctx->pending_req_list);
		CAM_DBG(CAM_ISP, "INIT packet added req id= %d",
			req->request_id);
		goto done_unlock;
	}

	req_old = list_first_entry(&ctx->pending_req_list,
		struct cam_isp_fastpath_ctx_req, list);
	if (req_old->hw_update_data.packet_opcode_type ==
		CAM_ISP_PACKET_INIT_DEV) {
		if ((req_old->num_cfg + req->num_cfg) >= CAM_ISP_FP_CFG_MAX) {
			CAM_WARN(CAM_ISP, "Can not merge INIT pkt");
			rc = -ENOMEM;
		}

		if (req_old->num_fence_map_out != 0 ||
			req_old->num_fence_map_in != 0) {
			CAM_WARN(CAM_ISP, "Invalid INIT pkt sequence");
			rc = -EINVAL;
		}

		if (!rc) {
			memcpy(req_old->fence_map_out,
				req->fence_map_out,
				sizeof(req->fence_map_out[0])*
				req->num_fence_map_out);
			req_old->num_fence_map_out =
				req->num_fence_map_out;

			memcpy(req_old->fence_map_in,
				req->fence_map_in,
				sizeof(req->fence_map_in[0])*
				req->num_fence_map_in);
			req_old->num_fence_map_in =
				req->num_fence_map_in;

			memcpy(&req_old->cfg[req_old->num_cfg],
				req->cfg,
				sizeof(req->cfg[0])*
				req->num_cfg);
			req_old->num_cfg += req->num_cfg;

			/* Update frame header params for EPCR */
			hw_update_data = &req->hw_update_data;
			req_old->hw_update_data.frame_header_res_id =
				req->hw_update_data.frame_header_res_id;
			req_old->hw_update_data.frame_header_cpu_addr =
				hw_update_data->frame_header_cpu_addr;

			if (req->hw_update_data.num_reg_dump_buf) {
				req_update_new = &req->hw_update_data;
				req_update_old = &req_old->hw_update_data;
				memcpy(&req_update_old->reg_dump_buf_desc,
					&req_update_new->reg_dump_buf_desc,
					sizeof(struct cam_cmd_buf_desc) *
					req_update_new->num_reg_dump_buf);
				req_update_old->num_reg_dump_buf =
					req_update_new->num_reg_dump_buf;
			}

			req_old->request_id = req->request_id;
		}
	} else {
		CAM_WARN(CAM_ISP,
			"Received Update pkt before INIT pkt. req_id= %lld",
			req->request_id);
		rc = -EINVAL;
	}

done_unlock:
	mutex_unlock(&ctx->mutex_list);
	return rc;
}


static int cam_isp_fpc_update_packet_buffers(struct cam_fp_buffer_set *buf_set,
					     struct cam_packet *packet)
{
	struct cam_buf_io_cfg *io_cfg;
	uint32_t i, bit, res;

	packet->header.request_id = buf_set->request_id;
	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&packet->payload +
			packet->io_configs_offset);
	for_each_set_bit(bit, (unsigned long *)&buf_set->out_buffer_set_mask,
			 CAM_FP_MAX_BUFS) {
		for (res = 0; res < packet->num_io_configs; res++) {
			if (io_cfg[res].resource_type == ife_port_res_lut[bit])
				break;
		}
		if (res == packet->num_io_configs) {
			CAM_ERR(CAM_ISP, "Invalid buffer configuration %d %d",
				res, packet->num_io_configs);
			return -EINVAL;
		}
		io_cfg[res].direction = CAM_BUF_OUTPUT;
		for (i = 0; i < buf_set->out_bufs[bit].num_planes; i++) {
			io_cfg[res].mem_handle[i] =
				buf_set->out_bufs[bit].plane[i].handle;
			io_cfg[res].offsets[i] =
				buf_set->out_bufs[bit].plane[i].offset;
		}
		if (i < CAM_PACKET_MAX_PLANES)
			io_cfg[res].mem_handle[i] = 0;
	}
	return 0;
}

static int cam_isp_fpc_get_packet(struct cam_isp_fastpath_context *ctx,
				  int32_t packet_handle,
				  size_t offset,
				  struct cam_packet **packet,
				  size_t *remain_len)
{
	struct cam_isp_fastpath_packet *fp_packet;
	uintptr_t packet_addr;
	size_t len = 0;
	int rc = 0;

	rc = cam_mem_get_cpu_buf(packet_handle,
		&packet_addr, &len);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Can not get packet address");
		rc = -EINVAL;
		return rc;
	}

	*remain_len = len;
	if ((len < sizeof(struct cam_packet)) ||
		(offset >= len - sizeof(struct cam_packet))) {
		CAM_ERR(CAM_ISP, "invalid buff length: %zu or offset",
			len);
		rc = -EINVAL;
		return rc;
	}

	*remain_len -= offset;
	*packet = (struct cam_packet *)(packet_addr + (uint32_t) offset);
	if ((*packet)->header.request_id > 0) {
		fp_packet = kmem_cache_zalloc(ctx->packet_cache, GFP_KERNEL);

		fp_packet->packet = (*packet);
		fp_packet->remain_len = *remain_len;

		mutex_lock(&ctx->mutex_list);
		if (list_empty(&ctx->active_packet_list))
			list_add_tail(&fp_packet->list,
				      &ctx->active_packet_list);
		else
			list_add_tail(&fp_packet->list,
				      &ctx->pending_packet_list);
		mutex_unlock(&ctx->mutex_list);
	}

	CAM_DBG(CAM_ISP, "pack_handle %llx", packet_handle);
	CAM_DBG(CAM_ISP, "packet address is 0x%zx", packet_addr);
	CAM_DBG(CAM_ISP, "packet with length %zu, offset 0x%llx",
		len, offset);
	CAM_DBG(CAM_ISP, "Packet request id %lld",
		(*packet)->header.request_id);
	CAM_DBG(CAM_ISP, "Packet size 0x%x", (*packet)->header.size);
	CAM_DBG(CAM_ISP, "packet op %d", (*packet)->header.op_code);

	return rc;
}

static int
cam_isp_fpc_prepare_hw_config(struct cam_isp_fastpath_context *ctx,
			      struct cam_packet *packet,
			      size_t *remain_len,
			      struct cam_hw_prepare_update_args *cfg,
			      struct cam_isp_fastpath_ctx_req *req_isp)
{
	struct cam_fp_buffer_set *buffer_set = NULL;
	struct cam_isp_hw_cmd_args isp_hw_cmd_args;
	struct cam_isp_fastpath_packet *fp_packet;
	struct cam_hw_cmd_args hw_cmd_args;
	bool reused_packet = false;
	uint32_t packet_opcode = 0;
	int rc = 0;

	if (!packet || packet->header.request_id == 1) {
		buffer_set = cam_fp_queue_get_buffer_set(&ctx->fp_queue);
		if (!buffer_set) {
			CAM_ERR(CAM_ISP, "Null Ptr! packet %p requestId %lld",
				 packet, (packet == NULL) ?
				 -1 : packet->header.request_id);
			return -EINVAL;
		}
		req_isp->out_mask = buffer_set->out_buffer_set_mask;

		mutex_lock(&ctx->mutex_list);
		if (list_empty(&ctx->active_packet_list)) {
			CAM_ERR(CAM_ISP, "Active packet list empty!");
			rc = -EINVAL;
			goto error_unlock_and_release_buffer;
		}
		fp_packet = list_first_entry(&ctx->active_packet_list,
				struct cam_isp_fastpath_packet, list);
		if (!fp_packet) {
			CAM_ERR(CAM_ISP, "Null Ptr!");
			rc = -EINVAL;
			goto error_unlock_and_release_buffer;
		}
		packet = fp_packet->packet;
		*remain_len = fp_packet->remain_len;
		if ((buffer_set->request_id >
		    fp_packet->packet->header.request_id) &&
		    !list_empty(&ctx->pending_packet_list)) {
			list_del_init(&fp_packet->list);
			kmem_cache_free(ctx->packet_cache, fp_packet);

			fp_packet = list_first_entry(&ctx->pending_packet_list,
					struct cam_isp_fastpath_packet, list);
			list_del(&fp_packet->list);
			list_add_tail(&fp_packet->list,
					&ctx->active_packet_list);
			packet = fp_packet->packet;
		}
		if (!packet) {
			CAM_ERR(CAM_ISP, "Null Ptr!");
			rc = -EINVAL;
			goto error_unlock_and_release_buffer;
		}

		fp_packet->use_cnt++;
		if (fp_packet->use_cnt > 1) {
			reused_packet = true;
			CAM_DBG(CAM_ISP, "Packet is reused %d!",
				fp_packet->use_cnt);
		}

		cam_isp_fpc_update_packet_buffers(buffer_set, packet);
		mutex_unlock(&ctx->mutex_list);
	}

	/* Query the packet opcode */
	memset(&hw_cmd_args, 0, sizeof(hw_cmd_args));
	hw_cmd_args.ctxt_to_hw_map = ctx->hw_ctx;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;

	memset(&isp_hw_cmd_args, 0, sizeof(isp_hw_cmd_args));
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_GET_PACKET_OPCODE;
	isp_hw_cmd_args.cmd_data = (void *)packet;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;
	rc = ctx->hw_intf.hw_cmd(ctx->hw_intf.hw_mgr_priv,
		&hw_cmd_args);
	if (rc) {
		CAM_ERR(CAM_ISP, "HW command failed");
		goto error_release_buffer;
	}

	packet_opcode = isp_hw_cmd_args.u.packet_op_code;
	CAM_DBG(CAM_ISP, "packet op %d", packet_opcode);

	/* preprocess the configuration */
	cfg->packet = packet;
	cfg->remain_len = *remain_len;
	CAM_DBG(CAM_ISP, "try to prepare config packet......");
	rc = ctx->hw_intf.hw_prepare_update(
		ctx->hw_intf.hw_mgr_priv, cfg);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Prepare config packet failed in HW layer");
		rc = -EFAULT;
		goto error_release_buffer;
	}
	if (req_isp) {
		req_isp->request_id = packet->header.request_id;
		req_isp->reused_packet = reused_packet;
	}

	return 0;

error_unlock_and_release_buffer:
	mutex_unlock(&ctx->mutex_list);
error_release_buffer:
	if (buffer_set) {
		CAM_ERR(CAM_ISP, "HW config fail release buffer set %llu",
			buffer_set->request_id);
		cam_fp_queue_buffer_set_done(&ctx->fp_queue,
					     buffer_set->request_id, 0,
					     CAM_FP_BUFFER_STATUS_ERROR,
					     req_isp->sof_index);
	}
	return rc;
}

static int cam_isp_fpc_config_dev(struct cam_isp_fastpath_context *ctx,
				  struct cam_config_dev_cmd *cmd)
{
	struct cam_isp_fastpath_ctx_req *req_isp;
	struct cam_hw_prepare_update_args cfg;
	struct cam_packet *packet = NULL;
	size_t remain_len = 0;
	int rc = 0;

	CAM_DBG(CAM_ISP, "get free request object......");

	if (cmd->packet_handle != INVAL_HANDLE) {
		cam_isp_fpc_get_packet(ctx, cmd->packet_handle,
			(size_t) cmd->offset, &packet, &remain_len);
		if (packet->header.request_id > 1)
			return 0;
	}

	req_isp = kmem_cache_zalloc(ctx->request_cache, GFP_KERNEL);

	/* preprocess the configuration */
	memset(&cfg, 0, sizeof(cfg));
	cfg.ctxt_to_hw_map = ctx->hw_ctx;
	cfg.max_hw_update_entries = CAM_ISP_FP_CFG_MAX;
	cfg.hw_update_entries = req_isp->cfg;
	cfg.max_out_map_entries = CAM_ISP_FP_CFG_MAX;
	cfg.max_in_map_entries = CAM_ISP_FP_CFG_MAX;
	cfg.out_map_entries = req_isp->fence_map_out;
	cfg.in_map_entries = req_isp->fence_map_in;
	cfg.priv  = &req_isp->hw_update_data;
	rc = cam_isp_fpc_prepare_hw_config(ctx, packet, &remain_len, &cfg,
			req_isp);
	if (rc != 0)
		goto free_req;

	req_isp->num_cfg = cfg.num_hw_update_entries;
	req_isp->num_fence_map_out = cfg.num_out_map_entries;
	req_isp->num_fence_map_in = cfg.num_in_map_entries;
	req_isp->num_acked = 0;

	if (req_isp->hw_update_data.packet_opcode_type ==
	    CAM_ISP_PACKET_INIT_DEV) {
		rc = cam_isp_fpc_enqueue_init_request(ctx, req_isp);
		if (rc)
			CAM_ERR(CAM_ISP, "Enqueue INIT pkt failed");
	} else {
		cam_isp_fpc_enqueue_request(ctx, req_isp);
	}
	if (rc)
		goto free_req;

	CAM_DBG(CAM_REQ,
		"Preprocessing Config req_id %lld successful on ctx %u",
		req_isp->request_id, ctx->ctx_id);

	return rc;

free_req:
	kmem_cache_free(ctx->request_cache, req_isp);

	return rc;
}

static int cam_isp_fpc_apply_req(struct cam_isp_fastpath_context *ctx)
{
	struct cam_isp_fastpath_ctx_req *req_isp;
	struct cam_hw_config_args cfg;
	int rc = 0;

	if (!ctx) {
		CAM_ERR(CAM_ISP, "invalid CTX");
		rc = -EFAULT;
		goto end;
	}

	mutex_lock(&ctx->mutex_list);
	if (list_empty(&ctx->pending_req_list)) {
		CAM_ERR(CAM_ISP, "No available request to apply");
		rc = -EFAULT;
		goto end;
	}

	req_isp = list_first_entry(&ctx->pending_req_list,
		struct cam_isp_fastpath_ctx_req, list);

	if (!req_isp) {
		CAM_ERR(CAM_ISP, "invalid request");
		rc = -EFAULT;
		goto end;
	}

	req_isp->state = CAM_ISP_HW_EVENT_SOF; // next state should be DONE

	CAM_DBG(CAM_REQ, "Apply request %lld num_cfg %d reused packet %d",
		req_isp->request_id, req_isp->num_cfg, req_isp->reused_packet);

	memset(&cfg, 0, sizeof(cfg));
	cfg.ctxt_to_hw_map = ctx->hw_ctx;
	cfg.request_id = req_isp->request_id;
	cfg.hw_update_entries = req_isp->cfg;
	cfg.num_hw_update_entries = req_isp->num_cfg;
	cfg.priv  = &req_isp->hw_update_data;
	cfg.init_packet = 0;
	cfg.reapply = req_isp->reused_packet;

	rc = ctx->hw_intf.hw_config(ctx->hw_intf.hw_mgr_priv, &cfg);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Can not apply the configuration");
	} else {
		list_del(&req_isp->list);
		list_add_tail(&req_isp->list, &ctx->active_req_list);
		ctx->num_in_active++;
	}
end:
	mutex_unlock(&ctx->mutex_list);
	return rc;
}

static int
cam_isp_fpc_release_hw(struct cam_isp_fastpath_context *ctx)
{
	int rc = 0;

	if (ctx->hw_ctx) {
		struct cam_hw_release_args rel_arg;

		memset(&rel_arg, 0, sizeof(rel_arg));
		rel_arg.ctxt_to_hw_map = ctx->hw_ctx;
		ctx->hw_intf.hw_release(ctx->hw_intf.hw_mgr_priv,
			&rel_arg);
		ctx->hw_ctx = NULL;
	} else {
		CAM_ERR(CAM_ISP, "No hw resources acquired for this ctx");
	}

	ctx->hw_acquired = false;

	CAM_DBG(CAM_ISP, "Release device success[%u]",
		ctx->ctx_id);
	return rc;
}

static int cam_isp_fpc_release_dev(struct cam_isp_fastpath_context *ctx)
{
	if (ctx->hw_ctx) {
		CAM_ERR(CAM_ISP, "releasing hw");
		cam_isp_fpc_release_hw(ctx);
	}

	if (ctx->hw_ctx) {
		struct cam_hw_release_args rel_arg;

		memset(&rel_arg, 0, sizeof(rel_arg));
		rel_arg.ctxt_to_hw_map = ctx->hw_ctx;
		ctx->hw_intf.hw_release(ctx->hw_intf.hw_mgr_priv,
			&rel_arg);
		ctx->hw_ctx = NULL;
	}

	ctx->hw_acquired = false;

	cam_isp_fpc_clear_queues(ctx);

	cam_destroy_device_hdl(ctx->dev_hdl);

	CAM_DBG(CAM_ISP, "Release device success[%u]",
		ctx->ctx_id);
	return 0;
}

// Assumes ctx->mutex_list is locked
static struct cam_isp_fastpath_ctx_req *cam_isp_fpc_get_latest_processing_request(
			struct cam_isp_fastpath_context *ctx)
{
	struct cam_isp_fastpath_ctx_req *req_isp;

	while (ctx->num_in_processing > 1) {
		CAM_ERR(CAM_ISP, "ctx->num_in_processing %u", ctx->num_in_processing);
		if (list_empty(&ctx->process_req_list)) {
			CAM_ERR(CAM_ISP,
						"CRITICAL ERR: Process list is empty! num_in_processing %d",
						ctx->num_in_processing);
			ctx->num_in_processing = 0;
			return NULL;
		}

		req_isp = list_first_entry(&ctx->process_req_list,
				struct cam_isp_fastpath_ctx_req, list);

		CAM_ERR(CAM_ISP,
				"Recycling req %llu, sof_idx %u ts %llu WAIT %u from %u",
				req_isp->request_id,
				req_isp->sof_index,
				req_isp->timestamp,
				req_isp->num_acked,
				req_isp->num_fence_map_out);

		cam_fp_queue_buffer_set_done(&ctx->fp_queue,
			req_isp->request_id, req_isp->timestamp,
			CAM_FP_BUFFER_STATUS_ERROR,
			req_isp->sof_index);
		list_del_init(&req_isp->list);
		kmem_cache_free(ctx->request_cache, req_isp);
		ctx->num_in_processing--;
	}

	if (list_empty(&ctx->process_req_list)) {
		CAM_ERR(CAM_ISP, "Process list is empty!");
		return NULL;
	}
	req_isp = list_first_entry(&ctx->process_req_list,
			struct cam_isp_fastpath_ctx_req, list);

	return req_isp;
}


// Assumes ctx->mutex_list is locked
static struct cam_isp_fastpath_ctx_req *cam_isp_fpc_get_latest_active_request(
			struct cam_isp_fastpath_context *ctx)
{
	struct cam_isp_fastpath_ctx_req *req_isp;

	while (ctx->num_in_active > 1) {
		CAM_ERR(CAM_ISP, "ctx->active_req_list %u", ctx->active_req_list);
		if (list_empty(&ctx->active_req_list)) {
			CAM_ERR(CAM_ISP,
						"CRITICAL ERR: Process list is empty! num_in_active %d",
						ctx->num_in_active);
			ctx->num_in_active = 0;
			return NULL;
		}
		req_isp = list_first_entry(&ctx->active_req_list,
			struct cam_isp_fastpath_ctx_req, list);

		CAM_ERR(CAM_ISP,
				"Recycling Active req %llu, sof_idx %u ts %llu WAIT %u from %u",
				req_isp->request_id,
				req_isp->sof_index,
				req_isp->timestamp,
				req_isp->num_acked,
				req_isp->num_fence_map_out);
		cam_fp_queue_buffer_set_done(&ctx->fp_queue,
			req_isp->request_id, req_isp->timestamp,
			CAM_FP_BUFFER_STATUS_ERROR,
			req_isp->sof_index);
		list_del_init(&req_isp->list);
		kmem_cache_free(ctx->request_cache, req_isp);
		ctx->num_in_active--;
	}

	if (list_empty(&ctx->active_req_list)) {
		CAM_ERR(CAM_ISP, "Active list is empty!");
		return NULL;
	}
	req_isp = list_first_entry(&ctx->active_req_list,
			struct cam_isp_fastpath_ctx_req, list);

	return req_isp;
}

static void cam_isp_fpc_handle_workqueue_event(struct work_struct *work)
{
	struct cam_isp_fastpath_work_payload *payload;
	struct cam_isp_hw_done_event_data *done;
	struct cam_isp_fastpath_ctx_req *req_isp;
	struct cam_isp_fastpath_context *ctx;
	struct cam_config_dev_cmd cmd;
	int i, j;

	payload = container_of(work, struct cam_isp_fastpath_work_payload,
					work);

	ctx = payload->ctx;

	switch (payload->evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
		ctx->sof_cnt++;
		CAM_DBG(CAM_ISP, "FP SOF %d", ctx->sof_cnt);
		mutex_lock(&ctx->mutex_list);
		req_isp = cam_isp_fpc_get_latest_active_request(ctx);
		if (req_isp) {
			req_isp->timestamp = payload->data.sof.boot_time;
			req_isp->sof_index = ctx->sof_cnt;
			if (req_isp->state != CAM_ISP_HW_EVENT_SOF)
				CAM_ERR(CAM_ISP, "invalid state %u in process %u", req_isp->state, ctx->num_in_processing);


			CAM_DBG(CAM_ISP, "SOF %lld act %u process %u",
					req_isp->request_id,
					ctx->num_in_active,
					ctx->num_in_processing);

			req_isp->state = CAM_ISP_HW_EVENT_DONE;
			list_del(&req_isp->list);
			list_add_tail(&req_isp->list, &ctx->process_req_list);
			ctx->num_in_active--;
			ctx->num_in_processing++;
		}
		mutex_unlock(&ctx->mutex_list);
		break;
	case CAM_ISP_HW_EVENT_EPOCH:
		memset(&cmd, 0, sizeof(cmd));
		cmd.packet_handle = INVAL_HANDLE;

		cam_isp_fpc_config_dev(ctx, &cmd);
		cam_isp_fpc_apply_req(ctx);

		break;
	case CAM_ISP_HW_EVENT_DONE:
		done = &payload->data.done;

		mutex_lock(&ctx->mutex_list);

		req_isp = cam_isp_fpc_get_latest_processing_request(ctx);
		if (!req_isp) {
			mutex_unlock(&ctx->mutex_list);
			kmem_cache_free(ctx->payload_cache, payload);
			return;
		}
		if (req_isp->state != CAM_ISP_HW_EVENT_DONE)
			CAM_ERR(CAM_ISP, "invalid state %u in process %u", req_isp->state, ctx->num_in_processing);

		for (i = 0; i < done->num_handles; i++) {
			for (j = 0; j < req_isp->num_fence_map_out; j++) {
				if (done->resource_handle[i] ==
				    req_isp->fence_map_out[j].resource_handle)
					break;
			}

			if (j == req_isp->num_fence_map_out) {
				CAM_ERR(CAM_ISP, "resource not found");
				mutex_unlock(&ctx->mutex_list);
				kmem_cache_free(ctx->request_cache, req_isp);
				kmem_cache_free(ctx->payload_cache, payload);
				return;
			}

			req_isp->num_acked++;
			if (req_isp->num_acked == req_isp->num_fence_map_out) {
				CAM_DBG(CAM_ISP, "Return buffer %d SOF %d!",
					req_isp->request_id, req_isp->sof_index);
				cam_fp_queue_buffer_set_done(&ctx->fp_queue,
					req_isp->request_id, req_isp->timestamp,
					CAM_FP_BUFFER_STATUS_SUCCESS,
					req_isp->sof_index);
				req_isp->state = CAM_ISP_HW_EVENT_MAX; // set invalid state
				list_del_init(&req_isp->list);
				kmem_cache_free(ctx->request_cache, req_isp);
				if (ctx->num_in_processing)
					ctx->num_in_processing--;
				else
					CAM_ERR(CAM_ISP,
						"Detected mismatch in ctx->num_in_processing");
			} else {
				CAM_DBG(CAM_ISP, "Ret %lld WAIT %u from %u",
					req_isp->request_id,
					req_isp->num_acked,
					req_isp->num_fence_map_out);
			}
		}

		mutex_unlock(&ctx->mutex_list);
		break;
	case CAM_ISP_HW_EVENT_EOF:
	case CAM_ISP_HW_EVENT_REG_UPDATE:
	case CAM_ISP_HW_EVENT_ERROR:
	default:
		CAM_ERR(CAM_ISP, "Unhandled event!");
		break;
	}

	kmem_cache_free(ctx->payload_cache, payload);
}

static int cam_isp_fpc_handle_irq(void *context, uint32_t evt_id,
				  void *evt_data)
{
	struct cam_isp_fastpath_context *ctx =
		(struct cam_isp_fastpath_context *)context;
	struct cam_isp_fastpath_work_payload *payload;
	bool work_status;

	/* Dont create work for events which are not handled */
	switch (evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
	case CAM_ISP_HW_EVENT_EPOCH:
	case CAM_ISP_HW_EVENT_DONE:
		/* Those events will be processed */
		break;
	default:
		return 0;
	}

	payload = kmem_cache_zalloc(ctx->payload_cache, GFP_ATOMIC);
	if (!payload) {
		CAM_ERR(CAM_ISP, "Failed to allocate payload for evt %d", evt_id);
		return -ENOMEM;
	}

	payload->evt_id = evt_id;
	payload->ctx = ctx;

	switch (evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
		memcpy(&payload->data.sof, evt_data,
			sizeof(payload->data.sof));
		break;
	case CAM_ISP_HW_EVENT_DONE:
		memcpy(&payload->data.done, evt_data,
			sizeof(payload->data.done));
		break;
	default:
		/* No payload for the rest of the events */
		break;
	}

	INIT_WORK((struct work_struct *)&payload->work,
		  cam_isp_fpc_handle_workqueue_event);

	work_status = queue_work(ctx->work_queue, &payload->work);
	if (work_status == false) {
		CAM_ERR(CAM_ISP, "Failed to queue work for event=0x%x", evt_id);
		kmem_cache_free(ctx->payload_cache, payload);
		return -ENOMEM;
	}

	return 0;
}

static int cam_isp_fpc_acquire_dev(struct cam_isp_fastpath_context *ctx,
				   struct cam_acquire_dev_cmd *cmd)
{
	struct cam_isp_hw_cmd_args isp_hw_cmd_args;
	struct cam_isp_resource *isp_res = NULL;
	struct cam_create_dev_hdl req_hdl_param;
	struct cam_hw_acquire_args param;
	struct cam_hw_release_args release;
	struct cam_hw_cmd_args hw_cmd_args;
	int rc = 0;

	CAM_DBG(CAM_ISP,
		"session_hdl 0x%x, num_resources %d, hdl type %d, res %lld",
		cmd->session_handle, cmd->num_resources,
		cmd->handle_type, cmd->resource_hdl);

	if (cmd->num_resources == CAM_API_COMPAT_CONSTANT) {
		CAM_DBG(CAM_ISP, "Acquire dev handle");
		goto get_dev_handle;
	}

	if (cmd->num_resources > CAM_ISP_FP_RES_MAX) {
		CAM_ERR(CAM_ISP, "Too much resources in the acquire");
		rc = -ENOMEM;
		goto end;
	}

	/* for now we only support user pointer */
	if (cmd->handle_type != 1)  {
		CAM_ERR(CAM_ISP, "Only user pointer is supported");
		rc = -EINVAL;
		goto end;
	}

	isp_res = kzalloc(sizeof(*isp_res) * cmd->num_resources, GFP_KERNEL);
	if (!isp_res) {
		rc = -ENOMEM;
		goto end;
	}

	CAM_DBG(CAM_ISP, "start copy %d resources from user",
		 cmd->num_resources);

	if (copy_from_user(isp_res, u64_to_user_ptr(cmd->resource_hdl),
		sizeof(*isp_res)*cmd->num_resources)) {
		rc = -EFAULT;
		goto free_res;
	}

	memset(&param, 0, sizeof(param));
	param.context_data = ctx;
	param.event_cb = cam_isp_fpc_handle_irq;
	param.num_acq = cmd->num_resources;
	param.acquire_info = (uintptr_t) isp_res;

	/* call HW manager to reserve the resource */
	rc = ctx->hw_intf.hw_acquire(ctx->hw_intf.hw_mgr_priv,
		&param);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Acquire device failed");
		goto free_res;
	}

	/* Query the context has rdi only resource */
	memset(&hw_cmd_args, 0, sizeof(hw_cmd_args));
	hw_cmd_args.ctxt_to_hw_map = param.ctxt_to_hw_map;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;

	memset(&isp_hw_cmd_args, 0, sizeof(isp_hw_cmd_args));
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_CMD_CTX_TYPE;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;
	rc = ctx->hw_intf.hw_cmd(ctx->hw_intf.hw_mgr_priv,
				&hw_cmd_args);
	if (rc) {
		CAM_ERR(CAM_ISP, "HW command failed");
		goto free_hw;
	}

	ctx->hw_ctx = param.ctxt_to_hw_map;
	ctx->hw_acquired = true;

	kfree(isp_res);
	isp_res = NULL;

get_dev_handle:

	memset(&req_hdl_param, 0, sizeof(req_hdl_param));
	req_hdl_param.session_hdl = cmd->session_handle;
	/* bridge is not ready for these flags. so false for now */
	req_hdl_param.v4l2_sub_dev_flag = 0;
	req_hdl_param.media_entity_flag = 0;
	req_hdl_param.priv = ctx;

	CAM_DBG(CAM_ISP, "get device handle form bridge");
	ctx->dev_hdl = cam_create_device_hdl(&req_hdl_param);

	if (ctx->dev_hdl <= 0) {
		rc = -EFAULT;
		CAM_ERR(CAM_ISP, "Can not create device handle");
		goto free_hw;
	}

	// insert context index to device handle - bits [31:24]
	// device handle - bits [31:24] must be zero. Otherwise we will modify it!!!
	if (FP_DEV_GET_HDL_IDX(ctx->dev_hdl))
		CAM_ERR(CAM_ISP, "FP DEVICE INDEX IS NOT ZERO 0x%08x", ctx->dev_hdl);

	// insert context index to device handle - bits [31:24]
	FP_INSERT_IDX(ctx);

	cmd->dev_handle = ctx->dev_hdl;

	CAM_DBG(CAM_ISP,
		"Acquire success on session_hdl 0x%x num_rsrces %d ctx %u",
		cmd->session_handle, cmd->num_resources, ctx->ctx_id);

	return rc;

free_hw:
	release.ctxt_to_hw_map = ctx->hw_ctx;
	if (ctx->hw_acquired)
		ctx->hw_intf.hw_release(ctx->hw_intf.hw_mgr_priv,
			&release);
	ctx->hw_ctx = NULL;
	ctx->hw_acquired = false;
free_res:
	kfree(isp_res);
end:
	return rc;
}

static int cam_isp_fpc_acquire_hw_v2(struct cam_isp_fastpath_context *ctx,
				     void *args)
{
	struct cam_hw_acquire_args param;
	struct cam_hw_release_args release;
	struct cam_hw_cmd_args hw_cmd_args;
	struct cam_isp_hw_cmd_args isp_hw_cmd_args;
	struct cam_acquire_hw_cmd_v2 *cmd =
		(struct cam_acquire_hw_cmd_v2 *)args;
	struct cam_isp_acquire_hw_info *acquire_hw_info = NULL;
	int rc = 0, i, j;

	CAM_DBG(CAM_ISP,
		"session_hdl 0x%x, hdl type %d, res %lld",
		cmd->session_handle, cmd->handle_type, cmd->resource_hdl);

	/* for now we only support user pointer */
	if (cmd->handle_type != 1)  {
		CAM_ERR(CAM_ISP, "Only user pointer is supported");
		rc = -EINVAL;
		goto end;
	}

	if (cmd->data_size < sizeof(*acquire_hw_info)) {
		CAM_ERR(CAM_ISP, "data_size is not a valid value");
		goto end;
	}

	acquire_hw_info = kzalloc(cmd->data_size, GFP_KERNEL);
	if (!acquire_hw_info) {
		rc = -ENOMEM;
		goto end;
	}

	CAM_DBG(CAM_ISP, "start copy resources from user");

	if (copy_from_user(acquire_hw_info, (void __user *)cmd->resource_hdl,
		cmd->data_size)) {
		rc = -EFAULT;
		goto free_res;
	}

	memset(&param, 0, sizeof(param));
	param.context_data = ctx;
	param.event_cb = cam_isp_fpc_handle_irq;
	param.num_acq = CAM_API_COMPAT_CONSTANT;
	param.acquire_info_size = cmd->data_size;
	param.acquire_info = (uint64_t) acquire_hw_info;

	/* call HW manager to reserve the resource */
	rc = ctx->hw_intf.hw_acquire(ctx->hw_intf.hw_mgr_priv,
		&param);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Acquire device failed");
		goto free_res;
	}

	memset(&hw_cmd_args, 0, sizeof(hw_cmd_args));
	hw_cmd_args.ctxt_to_hw_map = param.ctxt_to_hw_map;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;

	memset(&isp_hw_cmd_args, 0, sizeof(isp_hw_cmd_args));
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_CMD_CTX_TYPE;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;
	rc = ctx->hw_intf.hw_cmd(ctx->hw_intf.hw_mgr_priv,
				&hw_cmd_args);
	if (rc) {
		CAM_ERR(CAM_ISP, "HW command failed");
		goto free_hw;
	}

	if (param.valid_acquired_hw) {
		for (i = 0; i < CAM_MAX_ACQ_RES; i++)
			cmd->hw_info.acquired_hw_id[i] =
				param.acquired_hw_id[i];

		for (i = 0; i < CAM_MAX_ACQ_RES; i++)
			for (j = 0; j < CAM_MAX_HW_SPLIT; j++)
				cmd->hw_info.acquired_hw_path[i][j] =
					param.acquired_hw_path[i][j];
	}

	cmd->hw_info.valid_acquired_hw = param.valid_acquired_hw;

	ctx->hw_ctx = param.ctxt_to_hw_map;
	ctx->hw_acquired = true;

	kfree(acquire_hw_info);
	return rc;

free_hw:
	memset(&release, 0, sizeof(release));
	release.ctxt_to_hw_map = ctx->hw_ctx;
	ctx->hw_intf.hw_release(ctx->hw_intf.hw_mgr_priv, &release);
	ctx->hw_ctx = NULL;
	ctx->hw_acquired = false;
free_res:
	kfree(acquire_hw_info);
end:
	return rc;
}

/* Isp Fastpath context interface functions */
int cam_isp_fastpath_query_cap(void *hnd, struct cam_query_cap_cmd *query)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !query) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	if (!ctx->hw_intf.hw_get_caps) {
		CAM_ERR(CAM_CORE, "Missing hw_get_caps!");
		return -EINVAL;
	}

	return ctx->hw_intf.hw_get_caps(ctx->hw_intf.hw_mgr_priv, query);
}

int cam_isp_fastpath_acquire_hw(void *hnd, void *args)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !args) {
		CAM_ERR(CAM_ISP, "Invalid input pointer");
		return -EINVAL;
	}

	return cam_isp_fpc_acquire_hw_v2(ctx, args);
}

int cam_isp_fastpath_release_hw(void *hnd, void *args)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	return cam_isp_fpc_release_hw(ctx);
}

int cam_isp_fastpath_flush_dev(void *hnd, struct cam_flush_dev_cmd *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_ERR(CAM_CORE, "Not Implemented!");
	return 0;
}

int cam_isp_fastpath_config_dev(void *hnd, struct cam_config_dev_cmd *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CORE, "Invalid arguments! ctx %p cmd %p", ctx, cmd);
		return -EINVAL;
	}

	return cam_isp_fpc_config_dev(ctx, cmd);
}

int cam_isp_fastpath_start_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	return cam_isp_fpc_start_dev(ctx, cmd);
}

int cam_isp_fastpath_stop_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	return cam_isp_fpc_stop_dev(ctx, cmd);
}

int cam_isp_fastpath_acquire_dev(void *hnd, struct cam_acquire_dev_cmd *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	return cam_isp_fpc_acquire_dev(ctx, cmd);
}

int cam_isp_fastpath_release_dev(void *hnd, struct cam_release_dev_cmd *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;
	int rc;

	if (!ctx) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	rc = cam_isp_fpc_stop_dev(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_ISP, "Stop device failed rc=%d", rc);

	rc = cam_isp_fpc_release_dev(ctx);
	if (rc)
		CAM_ERR(CAM_ISP, "Release device failed rc=%d", rc);

	return rc;
}

int cam_isp_fastpath_set_stream_mode(void *hnd,
				     struct cam_set_stream_mode *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_ERR(CAM_CORE, "Not Implemented!");
	return 0;
}

int cam_isp_fastpath_stream_mode_cmd(void *hnd,
				     struct cam_stream_mode_cmd *cmd)
{
	struct cam_isp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CORE, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_ERR(CAM_CORE, "Not Implemented!");
	return 0;
}

void *cam_isp_fastpath_context_create(
			struct cam_hw_mgr_intf *hw_intf, int ctx_id)

{
	struct cam_isp_fastpath_context *ctx = NULL;
	char fp_q_name[CAM_FP_MAX_NAME_SIZE];
	int rc;

	if (!hw_intf)
		return NULL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;
	ctx->ctx_id = ctx_id;

	/* Initialize the stream mode parameters */
	INIT_LIST_HEAD(&ctx->active_packet_list);
	INIT_LIST_HEAD(&ctx->pending_packet_list);
	INIT_LIST_HEAD(&ctx->active_req_list);
	ctx->num_in_active = 0;
	INIT_LIST_HEAD(&ctx->pending_req_list);
	INIT_LIST_HEAD(&ctx->process_req_list);
	ctx->num_in_processing = 0;

	ctx->payload_cache = KMEM_CACHE(cam_isp_fastpath_work_payload, 0);
	if (!ctx->payload_cache)
		goto error_free_context;

	ctx->request_cache = KMEM_CACHE(cam_isp_fastpath_ctx_req, 0);
	if (!ctx->request_cache)
		goto error_release_payload_cache;

	ctx->packet_cache = KMEM_CACHE(cam_isp_fastpath_packet, 0);
	if (!ctx->packet_cache)
		goto error_release_request_cache;

	snprintf(fp_q_name, sizeof(fp_q_name), "cam_fp_ife%d_q", ctx_id);
	/* Initialize fastpath queue with max of 16 buffers */
	rc = cam_fp_queue_init(&ctx->fp_queue, fp_q_name, 16,
			       NULL, NULL);
	if (rc < 0)
		goto error_release_packet_cache;

	snprintf(fp_q_name, sizeof(fp_q_name), "cam_fp_ife%d_wq", ctx_id);
	ctx->work_queue = alloc_ordered_workqueue(fp_q_name, WQ_HIGHPRI);
	if (!ctx->work_queue) {
		CAM_ERR(CAM_CORE, "Can not create workqueue!");
		goto error_fp_queue_deinit;
	}

	/* HW interface should be stored localy */
	memcpy(&ctx->hw_intf, hw_intf, sizeof(*hw_intf));

	mutex_init(&ctx->mutex_list);

	return ctx;

error_fp_queue_deinit:
	cam_fp_queue_deinit(&ctx->fp_queue);
error_release_packet_cache:
	kmem_cache_destroy(ctx->packet_cache);
error_release_request_cache:
	kmem_cache_destroy(ctx->request_cache);
error_release_payload_cache:
	kmem_cache_destroy(ctx->payload_cache);
error_free_context:
	kfree(ctx);
	return NULL;
}

void cam_isp_fastpath_context_destroy(struct cam_isp_fastpath_context *ctx)
{
	if (!ctx)
		return;

	flush_workqueue(ctx->work_queue);
	destroy_workqueue(ctx->work_queue);

	cam_fp_queue_deinit(&ctx->fp_queue);

	kmem_cache_destroy(ctx->packet_cache);
	kmem_cache_destroy(ctx->request_cache);
	kmem_cache_destroy(ctx->payload_cache);

	mutex_destroy(&ctx->mutex_list);

	kfree(ctx);
}
