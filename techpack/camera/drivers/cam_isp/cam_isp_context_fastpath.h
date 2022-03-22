/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ISP_CONTEXT_FASTPATH_H_
#define _CAM_ISP_CONTEXT_FASTPATH_H_


#include <linux/spinlock.h>
#include <media/cam_isp.h>
#include <media/cam_defs.h>
#include <linux/workqueue.h>

#include "cam_isp_hw_mgr_intf.h"
#include "cam_fastpath_queue.h"

/*
 * Maximum hw resource - This number is based on the maximum
 * output port resource. The current maximum resource number
 * is 24.
 */
#define CAM_ISP_FP_RES_MAX 24

/*
 * Maximum configuration entry size  - This is based on the
 * worst case DUAL IFE use case plus some margin.
 */
#define CAM_ISP_FP_CFG_MAX 22

struct cam_isp_fastpath_packet {
	struct list_head  list;
	struct cam_packet *packet;
	uint32_t          remain_len;
	uint64_t          use_cnt;
};

struct cam_isp_fastpath_ctx_req {
	struct list_head                      list;
	uint64_t                              request_id;
	uint64_t                              out_mask;
	struct cam_hw_update_entry            cfg[CAM_ISP_FP_CFG_MAX];
	uint32_t                              num_cfg;
	struct cam_hw_fence_map_entry         fence_map_out[CAM_ISP_FP_RES_MAX];
	uint32_t                              num_fence_map_out;
	struct cam_hw_fence_map_entry         fence_map_in[CAM_ISP_FP_RES_MAX];
	uint32_t                              num_fence_map_in;
	uint32_t                              num_acked;
	struct cam_isp_prepare_hw_update_data hw_update_data;
	uint64_t                              timestamp;
	bool                                  reused_packet;
	uint32_t                              sof_index;
	uint32_t                              state;
};

struct cam_isp_fastpath_work_payload {
	struct work_struct work;
	struct cam_isp_fastpath_context *ctx;
	uint32_t evt_id;
	union {
		struct cam_isp_hw_sof_event_data sof;
		struct cam_isp_hw_done_event_data done;
	} data;
};

struct cam_isp_fastpath_context {
	struct cam_hw_mgr_intf                hw_intf;
	uint32_t                              ctx_id;
	int32_t                               dev_hdl;
	void                                 *hw_ctx;
	bool                                  hw_acquired;
	struct workqueue_struct              *work_queue;

	struct kmem_cache                     *payload_cache;
	struct kmem_cache                     *request_cache;
	struct kmem_cache                     *packet_cache;

	struct list_head                      active_packet_list;
	struct list_head                      pending_packet_list;

	struct list_head                      active_req_list;
	unsigned int                          num_in_active;
	struct list_head                      pending_req_list;
	struct list_head                      process_req_list;
	unsigned int                          num_in_processing;

	struct mutex                          mutex_list;

	struct cam_fp_queue                   fp_queue;

	uint32_t                              sof_cnt;

};

int cam_isp_fastpath_query_cap(void *hnd, struct cam_query_cap_cmd *query);
int cam_isp_fastpath_acquire_hw(void *hnd, void *args);
int cam_isp_fastpath_release_hw(void *hnd, void *args);
int cam_isp_fastpath_flush_dev(void *hnd, struct cam_flush_dev_cmd *cmd);
int cam_isp_fastpath_config_dev(void *hnd, struct cam_config_dev_cmd *cmd);
int cam_isp_fastpath_start_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd);
int cam_isp_fastpath_stop_dev(void *hnd, struct cam_start_stop_dev_cmd *cmd);
int cam_isp_fastpath_acquire_dev(void *hnd, struct cam_acquire_dev_cmd *cmd);
int cam_isp_fastpath_release_dev(void *hnd, struct cam_release_dev_cmd *cmd);
int cam_isp_fastpath_set_stream_mode(void *hnd,
				     struct cam_set_stream_mode *cmd);
int cam_isp_fastpath_stream_mode_cmd(void *hnd,
				     struct cam_stream_mode_cmd *cmd);
void *cam_isp_fastpath_context_create(struct cam_hw_mgr_intf *hw_intf);
void cam_isp_fastpath_context_destroy(void *hnd);

#endif  /* _CAM_ISP_CONTEXT_FASTPATH_H_ */
