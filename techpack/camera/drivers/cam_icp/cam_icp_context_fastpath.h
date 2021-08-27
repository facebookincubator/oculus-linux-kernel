/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ICP_FASTPATH_CONTEXT_H_
#define _CAM_ICP_FASTPATH_CONTEXT_H_

#include "cam_hw_mgr_intf.h"
#include "cam_fastpath_queue.h"

/* Size of packet queue */
#define CAM_ICP_PACKET_QUEUE_SIZE 5

/* Max number of resourcess */
#define CAM_ICP_RES_MAX 20

/* Max number of patch maps */
#define CAM_ICP_MAX_IO_PATCH_MAPS 16

struct cam_icp_fastpath_io_patch_map {
	unsigned int num_maps;
	struct {
		unsigned int io_cfg_idx;
		unsigned int patch_idx;
	} map[CAM_ICP_MAX_IO_PATCH_MAPS];
};

struct cam_icp_fastpath_packet {
	struct list_head list;
	u64 request_id;
	struct cam_packet *packet;
	size_t remain_len;
	struct cam_icp_fastpath_io_patch_map io_patch_map;
};

struct cam_icp_fastpath_context {
	struct cam_hw_mgr_intf   hw_intf;
	int32_t                  dev_hdl;
	void                     *hw_ctx;

	struct {
		struct cam_hw_update_entry    update[CAM_ICP_RES_MAX];
		struct cam_hw_fence_map_entry fence_map_out[CAM_ICP_RES_MAX];
		struct cam_hw_fence_map_entry fence_map_in[CAM_ICP_RES_MAX];
	} hw_entry;

	struct cam_fp_queue fp_queue;

	struct list_head packet_pending_queue;
	struct list_head packet_free_queue;
	struct cam_icp_fastpath_packet *packets_mem;

	struct mutex ctx_lock;
};

int cam_icp_fastpath_set_power(void *hnd, int on);
int cam_icp_fastpath_query_cap(void *hnd, struct cam_query_cap_cmd *query);
int cam_icp_fastpath_acquire_hw(void *hnd, void *args);
int cam_icp_fastpath_release_hw(void *hnd, void *args);
int cam_icp_fastpath_flush_dev(void *hnd, struct cam_flush_dev_cmd *cmd);
int cam_icp_fastpath_config_dev(void *hnd, struct cam_config_dev_cmd *cmd);
int cam_icp_fastpath_start_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd);
int cam_icp_fastpath_stop_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd);
int cam_icp_fastpath_acquire_dev(void *hnd, struct cam_acquire_dev_cmd *cmd);
int cam_icp_fastpath_release_dev(void *hnd, struct cam_release_dev_cmd *cmd);
int cam_icp_fastpath_set_stream_mode(void *hnd,
				     struct cam_set_stream_mode *cmd);
int cam_isp_fastpath_stream_mode_cmd(void *hnd,
				     struct cam_stream_mode_cmd *cmd);
void *cam_icp_fastpath_context_create(struct cam_hw_mgr_intf *hw_intf);
void cam_icp_fastpath_context_destroy(void *hnd);

#endif /* _CAM_ICP_FASTPATH_CONTEXT_H_ */
