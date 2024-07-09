// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <media/cam_defs.h>
#include <media/cam_icp.h>

#include "cam_mem_mgr.h"
#include "cam_trace.h"
#include "cam_debug_util.h"
#include "cam_packet_util.h"
#include "cam_icp_context_fastpath.h"

/* Packet process timeout */
#define CAM_ICP_FPC_PROCESS_TIMEOUT_MS 30

static void cam_icp_fpc_packet_done(struct cam_icp_fastpath_context *ctx,
				    u64 request_id)
{

	struct cam_icp_fastpath_packet *fp_pck;
	struct list_head *pos;

	mutex_lock(&ctx->packet_lock);

	/* Try first to get latest updated patch in pending queue */
	list_for_each_prev(pos, &ctx->packet_pending_queue) {
		fp_pck = list_entry(pos, struct cam_icp_fastpath_packet, list);
		if (fp_pck->packet->header.request_id == request_id) {
			complete_all(&fp_pck->done);
			CAM_DBG(CAM_ICP, "Complete packet %llu", request_id);
			break;
		}
	}

	mutex_unlock(&ctx->packet_lock);
}

/* Hw manager callback handling  */
static int
cam_icp_fpc_handle_buf_done(void *context, uint32_t evt_id, void *evt_data)
{
	struct cam_icp_fastpath_context *ctx =
		(struct cam_icp_fastpath_context *)context;
	struct cam_hw_done_event_data *done =
		(struct cam_hw_done_event_data *)evt_data;

	/*
	 * First complete the packet. If packet is reused the next
	 * processing should start as soon as posible.
	 */
	cam_icp_fpc_packet_done(ctx, done->request_id);

	cam_fp_queue_buffer_set_done(&ctx->fp_queue,
				done->request_id, 0,
				CAM_FP_BUFFER_STATUS_SUCCESS, 0);

	CAM_DBG(CAM_ICP, "Buffer done request id %llu evt id %d",
		done->request_id, evt_id);

	return 0;
}

static int cam_icp_fpc_config_stream(struct cam_icp_fastpath_context *ctx,
				     struct cam_packet *packet)
{
	struct cam_hw_stream_setttings cfg;
	int rc;

	if (!ctx->hw_intf.hw_config_stream_settings) {
		CAM_ERR(CAM_ICP, "HW interface is not ready");
		return -EFAULT;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.packet = packet;
	cfg.ctxt_to_hw_map = ctx->hw_ctx;
	cfg.priv = NULL;
	CAM_DBG(CAM_CTXT, "Processing config settings");
	rc = ctx->hw_intf.hw_config_stream_settings(
		ctx->hw_intf.hw_mgr_priv, &cfg);

	CAM_DBG(CAM_ICP, "Processing config settings %d", rc);
	return rc;
}

static int cam_icp_fpc_update_io_cfg_bufs(struct cam_buf_io_cfg *io_cfg,
					  unsigned int num_io_cfgs,
					  struct cam_fp_buffer *bufs,
					  unsigned int num_bufs,
					  u64 buf_mask,
					  unsigned int direction)
{
	uint32_t i, bit, res_num;
	unsigned long *addr;

	addr = (unsigned long *)&buf_mask;
	for_each_set_bit(bit, addr, num_bufs) {
		for (res_num = 0; res_num < num_io_cfgs; res_num++) {
			if (io_cfg[res_num].resource_type == bit)
				break;
		}
		if (res_num == num_io_cfgs) {
			CAM_ERR(CAM_ISP, "Invalid buffer configuration %d %d",
				res_num, num_io_cfgs);
			return -EINVAL;
		}

		if (io_cfg[res_num].direction != direction) {
			CAM_ERR(CAM_ISP, "Direction mismatch req %d actual %d",
				direction, io_cfg[res_num].direction);
			return -EINVAL;
		}

		for (i = 0; i < bufs[bit].num_planes; i++) {
			io_cfg[res_num].mem_handle[i] =
				bufs[bit].plane[i].handle;
			io_cfg[res_num].offsets[i] =
				bufs[bit].plane[i].offset;
		}
		if (i < CAM_PACKET_MAX_PLANES)
			io_cfg[res_num].mem_handle[i] = 0;
	}

	return 0;
}

static void
cam_icp_fpc_print_patch_iocfg(struct cam_buf_io_cfg *io_cfg,
			      unsigned int num_io_cfgs,
			      struct cam_patch_desc *patch_desc,
			      unsigned int num_patches)
{
	unsigned int patch_idx;
	unsigned int io_idx;

	CAM_ERR(CAM_ICP, "Print patch icfg start!");

	for (patch_idx = 0; patch_idx < num_patches; patch_idx++)
		CAM_ERR(CAM_ICP, "Patch index %d handle %x", patch_idx,
			patch_desc[patch_idx].src_buf_hdl);

	for (io_idx = 0; io_idx < num_io_cfgs; io_idx++)
		CAM_ERR(CAM_ICP, "Iocfg index %d handle %x",
			io_idx, io_cfg[io_idx].mem_handle[0]);

	CAM_ERR(CAM_ICP, "Print patch icfg end!");
}

static int
cam_icp_fpc_update_patch_map(struct cam_buf_io_cfg *io_cfg,
			     unsigned int num_io_cfgs,
			     struct cam_patch_desc *patch_desc,
			     unsigned int num_patches,
			     struct cam_icp_fastpath_io_patch_map *patch_map)
{
	unsigned int num_maps = 0;
	unsigned int io_idx;
	unsigned int p_idx;

	for (p_idx = 0; p_idx < num_patches; p_idx++) {
		for (io_idx = 0; io_idx < num_io_cfgs; io_idx++) {
			/* Store only matching handlers */
			if (patch_desc[p_idx].src_buf_hdl !=
			    io_cfg[io_idx].mem_handle[0])
				continue;

			patch_map->map[num_maps].io_cfg_idx = io_idx;
			patch_map->map[num_maps].patch_idx = p_idx;

			CAM_DBG(CAM_ICP, "Update patch %d iocfg %d handle %x",
				patch_map->map[num_maps].patch_idx,
				patch_map->map[num_maps].io_cfg_idx,
				io_cfg[io_idx].mem_handle[0]);

			num_maps++;
			if (num_maps >= CAM_ICP_MAX_IO_PATCH_MAPS) {
				CAM_ERR(CAM_ICP, "Patch map out of memory!");
				return -ENOMEM;

			}
		}
	}

	if (!num_maps) {
		CAM_ERR(CAM_ICP, "No handles found in patch descriptor!");
		cam_icp_fpc_print_patch_iocfg(io_cfg, num_io_cfgs,
			patch_desc, num_patches);
		return -EINVAL;
	}

	patch_map->num_maps = num_maps;

	return 0;
}

static int
cam_icp_fpc_apply_patch_map(struct cam_buf_io_cfg *io_cfg,
			    unsigned int num_io_cfgs,
			    struct cam_patch_desc *patch_desc,
			    unsigned int num_patches,
			    struct cam_icp_fastpath_io_patch_map *patch_map)
{
	unsigned int i;

	if (!patch_map->num_maps) {
		CAM_ERR(CAM_ICP, "No maps in patch map !");
		return -EINVAL;
	}

	for (i = 0; i < patch_map->num_maps; i++) {
		if (WARN_ON(patch_map->map[i].io_cfg_idx > num_io_cfgs)) {
			CAM_ERR(CAM_ICP, "io_cfg_idx out of the range!");
			return -EINVAL;
		}

		if (WARN_ON(patch_map->map[i].patch_idx > num_patches)) {
			CAM_ERR(CAM_ICP, "patch_idx out of the range!");
			return -EINVAL;
		}

		patch_desc[patch_map->map[i].patch_idx].src_buf_hdl =
			io_cfg[patch_map->map[i].io_cfg_idx].mem_handle[0];

		CAM_DBG(CAM_ICP, "Apply patch index %d handle %x",
			patch_map->map[i].patch_idx,
			patch_desc[patch_map->map[i].patch_idx].src_buf_hdl);
	}

	return 0;
}

static struct cam_icp_fastpath_io_patch_map *
cam_icp_fpc_get_any_patch_map(struct cam_icp_fastpath_context *ctx)
{
	struct cam_icp_fastpath_io_patch_map *patch_map = NULL;
	struct cam_icp_fastpath_packet *fp_pck;
	struct list_head *pos;

	mutex_lock(&ctx->packet_lock);

	/* Try first to get latest updated patch in pending queue */
	list_for_each_prev(pos, &ctx->packet_pending_queue) {
		fp_pck = list_entry(pos, struct cam_icp_fastpath_packet, list);
		if (fp_pck->io_patch_map.num_maps) {
			patch_map = &fp_pck->io_patch_map;
			break;
		}
	}

	/* If not found fallback and search in to the free queue */
	list_for_each_prev(pos, &ctx->packet_free_queue) {
		fp_pck = list_entry(pos, struct cam_icp_fastpath_packet, list);
		if (fp_pck->io_patch_map.num_maps) {
			patch_map = &fp_pck->io_patch_map;
			break;
		}
	}

	mutex_unlock(&ctx->packet_lock);

	return patch_map;
}

static int
cam_icp_fpc_update_packet_bufs(struct cam_icp_fastpath_context *ctx,
			       struct cam_fp_buffer_set *buffer_set,
			       struct cam_icp_fastpath_packet *fp_packet)
{
	struct cam_buf_io_cfg *io_cfg;
	struct cam_packet *packet = fp_packet->packet;
	struct cam_patch_desc *patch_desc;
	int rc;

	lockdep_assert_held(&ctx->ctx_lock);

	/* Get request id from buffer set */
	packet->header.request_id = buffer_set->request_id;

	io_cfg = (struct cam_buf_io_cfg *)
		 ((uint8_t *) &packet->payload + packet->io_configs_offset);

	patch_desc = (struct cam_patch_desc *)
			((uint8_t *) &packet->payload +
			packet->patch_offset);

	rc = cam_icp_fpc_update_io_cfg_bufs(io_cfg, packet->num_io_configs,
					    buffer_set->in_bufs,
					    ARRAY_SIZE(buffer_set->in_bufs),
					    buffer_set->in_buffer_set_mask,
					    CAM_BUF_INPUT);
	if (rc < 0)
		return rc;

	rc = cam_icp_fpc_update_io_cfg_bufs(io_cfg, packet->num_io_configs,
					    buffer_set->out_bufs,
					    ARRAY_SIZE(buffer_set->out_bufs),
					    buffer_set->out_buffer_set_mask,
					    CAM_BUF_OUTPUT);
	if (rc < 0)
		return rc;

	/*
	 * ICP io buffer handlers are also present in patch descriptor. Map of
	 * those locations (iocfg to patch_desc) need to be created when
	 * packet and buffer set are matching. And latter when packet is reused
	 * handlers in patch descriptor will be changed with (apply patch map).
	 */
	if (buffer_set->request_id == fp_packet->request_id) {
		/* Update patch map only once */
		if (!fp_packet->io_patch_map.num_maps) {
			rc = cam_icp_fpc_update_patch_map(io_cfg,
				packet->num_io_configs,
				patch_desc,
				packet->num_patches,
				&fp_packet->io_patch_map);
			if (rc < 0)
				return rc;
		}
	} else {
		struct cam_icp_fastpath_io_patch_map *patch_map =
			&fp_packet->io_patch_map;

		if (!patch_map->num_maps) {
			CAM_ERR(CAM_CORE, "No patch maps try to get older!");
			patch_map = cam_icp_fpc_get_any_patch_map(ctx);
		}
		if (!patch_map) {
			CAM_ERR(CAM_CORE, "Can not find valid patch map!");
			return -EINVAL;
		}

		rc = cam_icp_fpc_apply_patch_map(io_cfg,
			packet->num_io_configs,
			patch_desc,
			packet->num_patches,
			patch_map);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int cam_icp_fpc_alloc_packet_queue(struct cam_icp_fastpath_context *ctx,
					  unsigned int num_packets)
{
	int i;

	if (ctx->packets_mem)
		return -ENOMEM;

	ctx->packets_mem = vmalloc(sizeof(*ctx->packets_mem) * num_packets);
	if (!ctx->packets_mem)
		return -ENOMEM;

	mutex_lock(&ctx->packet_lock);

	INIT_LIST_HEAD(&ctx->packet_free_queue);
	INIT_LIST_HEAD(&ctx->packet_pending_queue);

	/* Fill empty buffers queue */
	for (i = 0; i < num_packets; i++) {
		INIT_LIST_HEAD(&ctx->packets_mem[i].list);
		init_completion(&ctx->packets_mem[i].done);

		list_add_tail(&ctx->packets_mem[i].list,
			      &ctx->packet_free_queue);
	}

	mutex_unlock(&ctx->packet_lock);

	return 0;
}

static int cam_icp_fpc_free_packet_queue(struct cam_icp_fastpath_context *ctx)
{
	mutex_lock(&ctx->packet_lock);

	INIT_LIST_HEAD(&ctx->packet_free_queue);
	INIT_LIST_HEAD(&ctx->packet_pending_queue);

	if (ctx->packets_mem) {
		vfree(ctx->packets_mem);
		ctx->packets_mem = NULL;
	}

	mutex_unlock(&ctx->packet_lock);

	return 0;
}

static int cam_icp_fpc_flush_packet_queue(struct cam_icp_fastpath_context *ctx)
{
	struct cam_icp_fastpath_packet *fp_pck;
	struct list_head *next;
	struct list_head *pos;

	mutex_lock(&ctx->ctx_lock);
	mutex_lock(&ctx->packet_lock);

	/* Add list for each from pending to free queue */
	list_for_each_safe(pos, next, &ctx->packet_pending_queue) {
		fp_pck = list_entry(pos, struct cam_icp_fastpath_packet, list);

		list_del(&fp_pck->list);
		list_add_tail(&fp_pck->list, &ctx->packet_free_queue);
		complete_all(&fp_pck->done);
	}

	mutex_unlock(&ctx->packet_lock);
	mutex_unlock(&ctx->ctx_lock);

	return 0;
}

static int cam_icp_fpc_enqueue_packet(struct cam_icp_fastpath_context *ctx,
				    struct cam_packet *packet,
				    size_t remain_len)
{
	struct cam_icp_fastpath_packet *fp_pck;

	mutex_lock(&ctx->packet_lock);

	/* If there is no available packet in empty list delete oldest packet */
	if (list_empty(&ctx->packet_free_queue)) {
		fp_pck = list_first_entry(&ctx->packet_pending_queue,
				struct cam_icp_fastpath_packet, list);
		CAM_ERR(CAM_CORE, "Empty free_queue! Get packet from pending");
	} else {
		fp_pck = list_first_entry(&ctx->packet_free_queue,
				struct cam_icp_fastpath_packet, list);
	}

	list_del(&fp_pck->list);

	/*
	 * Store original request id. The request id from header will be
	 * updated with the one from the processed buffer set.
	 */
	fp_pck->request_id = packet->header.request_id;
	fp_pck->packet = packet;
	fp_pck->remain_len = remain_len;
	fp_pck->io_patch_map.num_maps = 0;
	list_add_tail(&fp_pck->list, &ctx->packet_pending_queue);

	/* New enqueued packets should be completed */
	complete_all(&fp_pck->done);

	mutex_unlock(&ctx->packet_lock);

	return 0;
}

static inline bool
cam_icp_fpc_is_packet_available(struct cam_icp_fastpath_context *ctx)
{
	bool available;

	mutex_lock(&ctx->packet_lock);

	available = !list_empty(&ctx->packet_pending_queue);

	mutex_unlock(&ctx->packet_lock);

	return available;
}

static struct cam_icp_fastpath_packet *
cam_icp_fpc_get_packet_and_discard_oldest(struct cam_icp_fastpath_context *ctx, u64 request_id)
{
	struct cam_icp_fastpath_packet *fp_pck = NULL;
	struct cam_icp_fastpath_packet *fp_pck_next;
	struct list_head *pos, *next;

	mutex_lock(&ctx->packet_lock);

	list_for_each_safe(pos, next, &ctx->packet_pending_queue) {
		fp_pck = list_entry(pos, struct cam_icp_fastpath_packet, list);
		if (list_is_last(&fp_pck->list, &ctx->packet_pending_queue))
			break;

		fp_pck_next = list_next_entry(fp_pck, list);
		if (fp_pck_next->request_id > request_id)
			break;

		/* Move old packets from pending to free queue */
		list_del(&fp_pck->list);
		list_add_tail(&fp_pck->list, &ctx->packet_free_queue);
		CAM_DBG(CAM_ICP, "Free packet %llu for request id %llu!",
			fp_pck->request_id, request_id);
	}

	CAM_DBG(CAM_ICP, "Found packet %llu for request id %llu!",
		fp_pck->request_id, request_id);

	mutex_unlock(&ctx->packet_lock);

	return fp_pck;
}

static int cam_icp_fpc_try_to_process(struct cam_icp_fastpath_context *ctx)
{
	struct cam_icp_fastpath_packet *fp_packet;
	struct cam_hw_prepare_update_args prep;
	struct cam_fp_buffer_set *buffer_set = NULL;
	struct cam_hw_config_args config;
	int rc = 0;

	lockdep_assert_held(&ctx->ctx_lock);

	if (!cam_icp_fpc_is_packet_available(ctx)) {
		CAM_DBG(CAM_ICP, "Packet not Received!");
		return -EAGAIN;
	}

	buffer_set = cam_fp_queue_get_buffer_set(&ctx->fp_queue);
	if (!buffer_set) {
		CAM_DBG(CAM_ICP, "Buffer not available!");
		return -EAGAIN;
	}
	if (buffer_set->status != CAM_FP_BUFFER_STATUS_SUCCESS) {
		CAM_WARN(CAM_ICP, "Ouch! buffer_set %llu with error!", buffer_set->request_id);
		rc = -EINVAL;
		goto error_release_buffer_set;
	}
	fp_packet = cam_icp_fpc_get_packet_and_discard_oldest(ctx, buffer_set->request_id);
	if (WARN_ON(!fp_packet)) {
		CAM_ERR(CAM_ICP, "Ouch! Packet received but not available!");
		rc = -EINVAL;
		goto error_release_buffer_set;
	}

	/* If packet is reused wait to finish first */
	if (buffer_set->request_id != fp_packet->request_id) {
		long wait_rc = wait_for_completion_timeout(&fp_packet->done,
			msecs_to_jiffies(CAM_ICP_FPC_PROCESS_TIMEOUT_MS));
		if (wait_rc < 1) {
			CAM_ERR(CAM_ICP, "Wait for completion timeout %d", rc);
			return wait_rc ? wait_rc : -ETIMEDOUT;
		}
		CAM_DBG(CAM_ICP, "Packet process wait done %d request id %llu",
			wait_rc, buffer_set->request_id);
	}

	rc = cam_icp_fpc_update_packet_bufs(ctx, buffer_set, fp_packet);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "Fail to update packet bufs");
		goto error_release_buffer_set;
	}

	/* preprocess the configuration */
	memset(&prep, 0, sizeof(prep));

	prep.packet = fp_packet->packet;
	prep.remain_len = fp_packet->remain_len;

	prep.ctxt_to_hw_map = ctx->hw_ctx;

	prep.max_hw_update_entries = CAM_ICP_RES_MAX;
	prep.num_hw_update_entries = 0;
	prep.hw_update_entries = ctx->hw_entry.update;

	prep.max_out_map_entries = CAM_ICP_RES_MAX;
	prep.out_map_entries =  ctx->hw_entry.fence_map_out;

	prep.max_in_map_entries = CAM_ICP_RES_MAX;
	prep.in_map_entries = ctx->hw_entry.fence_map_in;
	rc = ctx->hw_intf.hw_prepare_update(ctx->hw_intf.hw_mgr_priv, &prep);
	if (rc != 0) {
		CAM_ERR(CAM_ICP, "Prepare config packet failed in HW layer");
		rc = -EFAULT;
		goto error_release_buffer_set;
	}

	memset(&config, 0, sizeof(config));
	config.ctxt_to_hw_map = ctx->hw_ctx;
	config.request_id = buffer_set->request_id;
	config.hw_update_entries = prep.hw_update_entries;
	config.num_hw_update_entries = prep.num_hw_update_entries;
	config.priv = prep.priv;
	config.init_packet = 0;

	reinit_completion(&fp_packet->done);
	rc = ctx->hw_intf.hw_config(ctx->hw_intf.hw_mgr_priv, &config);
	if (rc) {
		CAM_ERR(CAM_ICP, "Can not apply the configuration");
		complete_all(&fp_packet->done);
		goto error_release_buffer_set;
	}

	CAM_DBG(CAM_ICP, "Process Buffer! %llu", buffer_set->request_id);

	return 0;

error_release_buffer_set:
	cam_fp_queue_buffer_set_done(&ctx->fp_queue,
				     buffer_set->request_id, 0,
				     CAM_FP_BUFFER_STATUS_ERROR, 0);
	return rc;
}

static int cam_icp_fpc_queue_buf_notify(void *priv)
{
	struct cam_icp_fastpath_context *ctx = priv;
	int rc;

	if (!ctx) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_lock);

	rc = cam_icp_fpc_try_to_process(ctx);

	mutex_unlock(&ctx->ctx_lock);

	return rc;
}

static int cam_icp_fpc_flush(void *priv)
{
	struct cam_icp_fastpath_context *ctx = priv;

	if (!ctx) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	cam_icp_fpc_flush_packet_queue(ctx);

	return 0;
}

/* Fastpath queue ops */
static const struct cam_fp_queue_ops cam_fastpath_queue_ops = {
	.queue_buf_notify = cam_icp_fpc_queue_buf_notify,
	.flush		  = cam_icp_fpc_flush,
};

int cam_icp_fastpath_set_power(void *hnd, int on)
{
	struct cam_icp_fastpath_context *ctx = hnd;
	int rc;

	if (!ctx) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	if (!ctx->hw_intf.hw_open || !ctx->hw_intf.hw_close) {
		CAM_ERR(CAM_ICP, "Missing hw_open/hw_close!");
		return -EINVAL;
	}

	if (on) {
		rc = ctx->hw_intf.hw_open(ctx->hw_intf.hw_mgr_priv, NULL);
		if (rc < 0) {
			CAM_ERR(CAM_ICP, "FW download failed");
			return rc;
		}
	} else {
		rc = ctx->hw_intf.hw_close(ctx->hw_intf.hw_mgr_priv, NULL);
		if (rc < 0) {
			CAM_ERR(CAM_ICP, "FW download failed");
			return rc;
		}
	}

	CAM_DBG(CAM_ICP, "Set power %d", on);
	return 0;
}

int cam_icp_fastpath_query_cap(void *hnd, struct cam_query_cap_cmd *query)
{
	struct cam_icp_fastpath_context *ctx = hnd;

	if (!ctx || !query) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	if (!ctx->hw_intf.hw_get_caps) {
		CAM_ERR(CAM_ICP, "Missing hw_get_caps!");
		return -EINVAL;
	}

	return ctx->hw_intf.hw_get_caps(ctx->hw_intf.hw_mgr_priv, query);
}

int cam_icp_fastpath_acquire_hw(void *hnd, void *args)
{
	struct cam_icp_fastpath_context *ctx = hnd;

	if (!ctx || !args) {
		CAM_ERR(CAM_ICP, "Invalid input pointer");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "Not Implemented!");
	return 0;
}

int cam_icp_fastpath_release_hw(void *hnd, void *args)
{
	struct cam_icp_fastpath_context *ctx = hnd;

	if (!ctx) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "Not Implemented!");
	return 0;
}

int cam_icp_fastpath_flush_dev(void *hnd, struct cam_flush_dev_cmd *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "Not Implemented!");
	return 0;
}

int cam_icp_fastpath_config_dev(void *hnd, struct cam_config_dev_cmd *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;
	struct cam_packet *packet;
	uintptr_t packet_addr;
	size_t remain_len = 0;
	size_t len;
	int rc;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_ICP, "Invalid arguments! ctx %p cmd %p", ctx, cmd);
		return -EINVAL;
	}

	rc = cam_mem_get_cpu_buf((int32_t) cmd->packet_handle,
		&packet_addr, &len);
	if (rc) {
		CAM_ERR(CAM_ICP, "Can not get packet address");
		return -EINVAL;
	}

	remain_len = len;
	if ((len < sizeof(struct cam_packet)) ||
	    (cmd->offset >= (len - sizeof(struct cam_packet)))) {
		CAM_ERR(CAM_ICP,
			"Invalid offset, len: %zu offset: %llu max_size: %zu",
			len, cmd->offset, sizeof(struct cam_packet));
		cam_mem_put_cpu_buf((int32_t) cmd->packet_handle);
		return -EINVAL;
	}

	remain_len -= (size_t)cmd->offset;
	packet = (struct cam_packet *) ((uint8_t *)packet_addr +
		(uint32_t)cmd->offset);

	rc = cam_packet_util_validate_packet(packet, remain_len);
	if (rc) {
		CAM_ERR(CAM_ICP, "Invalid packet params, remain length: %zu",
			remain_len);
		cam_mem_put_cpu_buf((int32_t) cmd->packet_handle);
		return rc;
	}

	mutex_lock(&ctx->ctx_lock);

	if (((packet->header.op_code & 0xff) ==
	    CAM_ICP_OPCODE_IPE_SETTINGS) ||
	    ((packet->header.op_code & 0xff) ==
	    CAM_ICP_OPCODE_BPS_SETTINGS)) {
		rc = cam_icp_fpc_config_stream(ctx, packet);
		CAM_DBG(CAM_ICP, "Stream settings received!");
	} else {
		/* Store valid packet if opcode is not stream config */
		CAM_DBG(CAM_ICP, "New packet received %llu!",
			packet->header.request_id);

		cam_icp_fpc_enqueue_packet(ctx, packet, remain_len);

		/* To to process new packet if buffers are available */
		rc = cam_icp_fpc_try_to_process(ctx);
		/* Dont propagate eagain to userspace */
		rc = (rc == -EAGAIN) ? 0 : rc;
	}

	mutex_unlock(&ctx->ctx_lock);

	if (rc)
		CAM_ERR(CAM_ICP, "Failed to prepare/config device %x  %d",
			packet->header.op_code, rc);

	cam_mem_put_cpu_buf((int32_t) cmd->packet_handle);
	return rc;
}

int cam_icp_fastpath_start_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;
	struct cam_hw_start_args start;
	int rc;

	if (!ctx) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	if (!ctx->hw_intf.hw_start)
		return 0;

	memset(&start, 0, sizeof(start));
	start.ctxt_to_hw_map = ctx->hw_ctx;
	rc = ctx->hw_intf.hw_start(ctx->hw_intf.hw_mgr_priv, &start);
	if (rc) {
		/* HW failure. user need to clean up the resource */
		CAM_ERR(CAM_ICP, "Start HW failed");
		return rc;
	}

	return 0;
}

int cam_icp_fastpath_stop_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;
	struct cam_hw_stop_args stop;
	int rc;

	if (!ctx) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	if (!ctx->hw_intf.hw_start)
		return 0;

	memset(&stop, 0, sizeof(stop));
	stop.ctxt_to_hw_map = ctx->hw_ctx;
	rc = ctx->hw_intf.hw_stop(ctx->hw_intf.hw_mgr_priv, &stop);
	if (rc) {
		/* HW failure. user need to clean up the resource */
		CAM_ERR(CAM_ICP, "Stop HW failed");
		return rc;
	}

	cam_icp_fpc_flush_packet_queue(ctx);

	return 0;
}

int cam_icp_fastpath_acquire_dev(void *hnd, struct cam_acquire_dev_cmd *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;
	struct cam_create_dev_hdl req_hdl_param;
	struct cam_hw_release_args release;
	struct cam_hw_acquire_args param;
	int rc;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "ses hdl: %x, num_res: %d, type: %d, res: %lld",
		cmd->session_handle, cmd->num_resources, cmd->handle_type,
		cmd->resource_hdl);

	if (cmd->num_resources > CAM_ICP_RES_MAX) {
		CAM_ERR(CAM_ICP, "Resource limit exceed %d",
			cmd->num_resources);
		return -ENOMEM;
	}

	/* for now we only support user pointer */
	if (cmd->handle_type != 1)  {
		CAM_ERR(CAM_ICP, "Only user pointer is supported");
		return -EINVAL;
	}

	/* fill in parameters */
	memset(&param, 0, sizeof(param));
	param.context_data = ctx;
	param.event_cb = cam_icp_fpc_handle_buf_done;
	param.num_acq = cmd->num_resources;
	param.acquire_info = cmd->resource_hdl;
	param.session_hdl = cmd->session_handle;

	/* call HW manager to reserve the resource */
	rc = ctx->hw_intf.hw_acquire(ctx->hw_intf.hw_mgr_priv, &param);
	if (rc != 0) {
		CAM_ERR(CAM_ICP, "Acquire device failed");
		return rc;
	}
	ctx->hw_ctx = param.ctxt_to_hw_map;

	memset(&req_hdl_param, 0, sizeof(req_hdl_param));
	req_hdl_param.session_hdl = cmd->session_handle;
	req_hdl_param.v4l2_sub_dev_flag = 0;
	req_hdl_param.media_entity_flag = 0;

	ctx->dev_hdl = cam_create_device_hdl(&req_hdl_param);
	if (ctx->dev_hdl <= 0) {
		rc = -EFAULT;
		CAM_ERR(CAM_ICP, "Can not create device handle");
		goto error_release_hw;
	}

	// device handle - bits [31:24] must be zero. Otherwise we will modify it!!!
	if (FP_DEV_GET_HDL_IDX(ctx->dev_hdl))
		CAM_ERR(CAM_ISP, "FP DEVICE INDEX IS NOT ZERO 0x%08x", ctx->dev_hdl);

	// insert context index to device handle - bits [31:24]
	FP_INSERT_IDX(ctx);


	cmd->dev_handle = ctx->dev_hdl;

	rc = cam_icp_fpc_alloc_packet_queue(ctx, CAM_ICP_PACKET_QUEUE_SIZE);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "Can not allocate packet queue %d", rc);
		goto error_destroy_hdl;
	}

	return 0;

error_destroy_hdl:
	cam_destroy_device_hdl(ctx->dev_hdl);
	ctx->dev_hdl = -1;
error_release_hw:
	memset(&release, 0, sizeof(release));
	release.ctxt_to_hw_map = ctx->hw_ctx;
	ctx->hw_intf.hw_release(ctx->hw_intf.hw_mgr_priv, &release);
	ctx->hw_ctx = NULL;
	ctx->dev_hdl = -1;
	return rc;
}

int cam_icp_fastpath_release_dev(void *hnd, struct cam_release_dev_cmd *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;
	struct cam_hw_release_args arg;

	if (!ctx) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	if (!ctx->hw_intf.hw_release) {
		CAM_ERR(CAM_ICP, "HW interface is not ready");
		return -EINVAL;
	}

	memset(&arg, 0, sizeof(arg));
	arg.ctxt_to_hw_map = ctx->hw_ctx;
	arg.active_req = false;
	ctx->hw_intf.hw_release(ctx->hw_intf.hw_mgr_priv, &arg);

	cam_destroy_device_hdl(ctx->dev_hdl);
	ctx->dev_hdl = -1;

	cam_icp_fpc_free_packet_queue(ctx);

	ctx->hw_ctx = NULL;

	return 0;
}

int cam_icp_fastpath_set_stream_mode(void *hnd,
				     struct cam_set_stream_mode *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "Not Implemented!");
	return 0;
}

int cam_icp_fastpath_stream_mode_cmd(void *hnd,
				     struct cam_stream_mode_cmd *cmd)
{
	struct cam_icp_fastpath_context *ctx = hnd;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_ICP, "Invalid arguments!");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "Not Implemented!");
	return 0;
}

void *cam_icp_fastpath_context_create(
			struct cam_hw_mgr_intf *hw_intf, int ctx_id)

{
	struct cam_icp_fastpath_context *ctx;
	char fp_q_name[CAM_FP_MAX_NAME_SIZE];
	int rc;

	if (!hw_intf)
		return NULL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ctx->ctx_id = ctx_id;

	snprintf(fp_q_name, sizeof(fp_q_name), "cam_fp_ipe%d_q", ctx_id);

	/* Initialize fastpath queue with max of 16 buffers */
	rc = cam_fp_queue_init(&ctx->fp_queue, fp_q_name, 16,
			       &cam_fastpath_queue_ops, ctx);
	if (rc < 0)
		goto error_free_context;

	/* HW interface should be stored localy */
	memcpy(&ctx->hw_intf, hw_intf, sizeof(*hw_intf));

	mutex_init(&ctx->ctx_lock);
	mutex_init(&ctx->packet_lock);

	return ctx;

error_free_context:
	kfree(ctx);
	return NULL;
}

void cam_icp_fastpath_context_destroy(void *hnd)
{
	struct cam_icp_fastpath_context *ctx = hnd;

	if (!ctx)
		return;

	cam_fp_queue_deinit(&ctx->fp_queue);
	mutex_destroy(&ctx->ctx_lock);
	kfree(ctx);
}
