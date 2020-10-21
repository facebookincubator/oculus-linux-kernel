/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ISP_CONTEXT_H_
#define _CAM_ISP_CONTEXT_H_


#include <linux/spinlock.h>
#include <media/cam_isp.h>
#include <media/cam_defs.h>

#include "cam_context.h"
#include "cam_isp_hw_mgr_intf.h"

/*
 * Maximum hw resource - This number is based on the maximum
 * output port resource. The current maximum resource number
 * is 24.
 */
#define CAM_ISP_CTX_RES_MAX                     24

/*
 * Maximum configuration entry size  - This is based on the
 * worst case DUAL IFE use case plus some margin.
 */
#define CAM_ISP_CTX_CFG_MAX                     22

/*
 * Maximum entries in state monitoring array for error logging
 */
#define CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES   40

/* forward declaration */
struct cam_isp_context;

/* cam isp context irq handling function type */
typedef int (*cam_isp_hw_event_cb_func)(struct cam_isp_context *ctx_isp,
	void *evt_data);

/**
 * enum cam_isp_ctx_activated_substate - sub states for activated
 *
 */
enum cam_isp_ctx_activated_substate {
	CAM_ISP_CTX_ACTIVATED_SOF,
	CAM_ISP_CTX_ACTIVATED_APPLIED,
	CAM_ISP_CTX_ACTIVATED_EPOCH,
	CAM_ISP_CTX_ACTIVATED_BUBBLE,
	CAM_ISP_CTX_ACTIVATED_BUBBLE_APPLIED,
	CAM_ISP_CTX_ACTIVATED_HW_ERROR,
	CAM_ISP_CTX_ACTIVATED_HALT,
	CAM_ISP_CTX_ACTIVATED_MAX,
};

/**
 * enum cam_isp_state_change_trigger - Different types of ISP events
 *
 */
enum cam_isp_state_change_trigger {
	CAM_ISP_STATE_CHANGE_TRIGGER_ERROR,
	CAM_ISP_STATE_CHANGE_TRIGGER_APPLIED,
	CAM_ISP_STATE_CHANGE_TRIGGER_REG_UPDATE,
	CAM_ISP_STATE_CHANGE_TRIGGER_SOF,
	CAM_ISP_STATE_CHANGE_TRIGGER_EPOCH,
	CAM_ISP_STATE_CHANGE_TRIGGER_DONE,
	CAM_ISP_STATE_CHANGE_TRIGGER_EOF,
	CAM_ISP_STATE_CHANGE_TRIGGER_FLUSH,
	CAM_ISP_STATE_CHANGE_TRIGGER_MAX
};

/**
 * struct cam_isp_ctx_debug -  Contains debug parameters
 *
 * @dentry:                    Debugfs entry
 * @enable_state_monitor_dump: Enable isp state monitor dump
 * @enable_stream_mode_sof:    Enable sof events in stream mode
 *
 */
struct cam_isp_ctx_debug {
	struct dentry  *dentry;
	uint32_t        enable_state_monitor_dump;
	uint32_t        enable_stream_mode_sof;
};

/**
 * struct cam_isp_ctx_irq_ops - Function table for handling IRQ callbacks
 *
 * @irq_ops:               Array of handle function pointers.
 *
 */
struct cam_isp_ctx_irq_ops {
	cam_isp_hw_event_cb_func         irq_ops[CAM_ISP_HW_EVENT_MAX];
};

/**
 * struct cam_isp_ctx_req - ISP context request object
 *
 * @base:                  Common request object ponter
 * @cfg:                   ISP hardware configuration array
 * @num_cfg:               Number of ISP hardware configuration entries
 * @fence_map_out:         Output fence mapping array
 * @num_fence_map_out:     Number of the output fence map
 * @fence_map_in:          Input fence mapping array
 * @num_fence_map_in:      Number of input fence map
 * @num_acked:             Count to track acked entried for output.
 *                         If count equals the number of fence out, it means
 *                         the request has been completed.
 * @bubble_report:         Flag to track if bubble report is active on
 *                         current request
 * @hw_update_data:        HW update data for this request
 * @reapply:               True if reapplying after bubble
 *
 */
struct cam_isp_ctx_req {
	struct cam_ctx_request               *base;

	struct cam_hw_update_entry            cfg[CAM_ISP_CTX_CFG_MAX];
	uint32_t                              num_cfg;
	struct cam_hw_fence_map_entry         fence_map_out
						[CAM_ISP_CTX_RES_MAX];
	uint32_t                              num_fence_map_out;
	struct cam_hw_fence_map_entry         fence_map_in[CAM_ISP_CTX_RES_MAX];
	uint32_t                              num_fence_map_in;
	uint32_t                              num_acked;
	int32_t                               bubble_report;
	struct cam_isp_prepare_hw_update_data hw_update_data;
	bool                                  bubble_detected;
	bool                                  reapply;
};

/**
 * struct cam_isp_context_state_monitor - ISP context state
 *                                        monitoring for
 *                                        debug purposes
 *
 * @curr_state:          Current sub state that received req
 * @trigger:             Event type of incoming req
 * @req_id:              Request id
 * @frame_id:            Frame id based on SOFs
 * @evt_time_stamp       Current time stamp
 *
 */
struct cam_isp_context_state_monitor {
	enum cam_isp_ctx_activated_substate  curr_state;
	enum cam_isp_state_change_trigger    trigger;
	uint64_t                             req_id;
	int64_t                              frame_id;
	unsigned int                         evt_time_stamp;
};

/**
 * struct cam_isp_context_req_id_info - ISP context request id
 *                     information for bufdone.
 *
 *@last_bufdone_req_id:   Last bufdone request id
 *
 */

struct cam_isp_context_req_id_info {
	int64_t                          last_bufdone_req_id;
};

/**
 * struct cam_isp_stream_image - Frame buffer and command to configure
 *                               the frame buffer on the hardware
 *
 * @list_entry:                  List entry field
 * @image_id:                    Unique ID of this image buffer
 * @mem_handles:                 The memory handle for each plane
 * @num_cfg:                     Number of configs in cfg field
 * @cfg:                         The config to program this frame to hardware
 * @num_fence_map_out:           Number of out fences
 * @fence_map_out:               Out fences to be signaled when frame is done
 * @hw_update_data:              Hardware update to be applied before the req
 * @pf_data:                     Pagefault data
 * @capture_timestamp:           Capture time of the frame
 * @frame_num:                   The frame number
 *
 */
struct cam_isp_stream_image {
	struct list_head                    list_entry;
	uint64_t                            image_id;
	int32_t                             mem_handles[CAM_PACKET_MAX_PLANES];
	uint32_t                            num_cfg;
	struct cam_hw_update_entry          cfg[CAM_ISP_CTX_CFG_MAX];
	uint32_t                            num_fence_map_out;
	struct cam_hw_fence_map_entry       fence_map_out[CAM_ISP_CTX_RES_MAX];
	struct cam_isp_prepare_hw_update_data hw_update_data;
	struct cam_hw_mgr_dump_pf_data      pf_data;
	uint64_t                            capture_timestamp;
	int64_t                             frame_num;
};

/**
 * struct cam_isp_context   -  ISP context object
 *
 * @base:                      Common context object pointer
 * @frame_id:                  Frame id tracking for the isp context
 * @substate_actiavted:        Current substate for the activated state.
 * @process_bubble:            Atomic variable to check if ctx is still
 *                             processing bubble.
 * @bubble_frame_cnt:          Count number of frames since the req is in bubble
 * @substate_machine:          ISP substate machine for external interface
 * @substate_machine_irq:      ISP substate machine for irq handling
 * @req_base:                  Common request object storage
 * @req_isp:                   ISP private request object storage
 * @hw_ctx:                    HW object returned by the acquire device command
 * @sof_timestamp_val:         Captured time stamp value at sof hw event
 * @boot_timestamp:            Boot time stamp for a given req_id
 * @active_req_cnt:            Counter for the active request
 * @reported_req_id:           Last reported request id
 * @subscribe_event:           The irq event mask that CRM subscribes to, IFE
 *                             will invoke CRM cb at those event.
 * @last_applied_req_id:       Last applied request id
 * @state_monitor_head:        Write index to the state monitoring array
 * @req_info                   Request id information about last buf done
 * @cam_isp_ctx_state_monitor: State monitoring array
 * @rdi_only_context:          Get context type information.
 * @skip_req_mgr:              Do not submit requests to request manager instead
 *                             use the context workqueue to submit requests.
 *                             true, if context is rdi only context
 * @hw_acquired:               Indicate whether HW resources are acquired
 * @init_received:             Indicate whether init config packet is received
 * @split_acquire:             Indicate whether a separate acquire is expected
 * @init_timestamp:            Timestamp at which this context is initialized
 *                             mode and dictated by the flag skip_req_mgr.
 * @last_request_accepted:     The last request that has been submitted.
 * @last_request_signaled:     The last successfully signaled request.
 * @bw_workq:                  Workq to handle bandwidth updates.
 * @bw_work:                   Work item handling bw updates.
 * @hw_update_data:            The bw update data.
 * @hw_update_index:           Index which is to be used in hw_update_data
 * @bw_lock:                   Lock to sync between thread applying bw update
 *                             and user thread that sends new bw data.
 * @stream_image_free_list:    List of free image buffers
 * @stream_image_ready_list:   List of image buffers that are ready
 * @stream_image_umd_list:     List of image buffers sent to umd driver
 * @stream_image_active_list:  List of buffers whose buf done is active
 * @num_stream_images:         Total number of valid image buffers
 * @stream_images:             Frame/Image buffers
 * @stream_image_completion:   Stream images are available to send to UMD
 * @stream_image_applied:      Frame applied to ISP
 * @stream_image_wait:         Used to sync multiple waiters
 *
 */
struct cam_isp_context {
	struct cam_context                   *base;

	int64_t                               frame_id;
	enum cam_isp_ctx_activated_substate   substate_activated;
	atomic_t                              process_bubble;
	uint32_t                              bubble_frame_cnt;
	struct cam_ctx_ops                   *substate_machine;
	struct cam_isp_ctx_irq_ops           *substate_machine_irq;

	struct cam_ctx_request                req_base[CAM_CTX_REQ_MAX];
	struct cam_isp_ctx_req                req_isp[CAM_CTX_REQ_MAX];

	void                                 *hw_ctx;
	uint64_t                              sof_timestamp_val;
	uint64_t                              boot_timestamp;
	int32_t                               active_req_cnt;
	int64_t                               reported_req_id;
	uint32_t                              subscribe_event;
	int64_t                               last_applied_req_id;
	atomic64_t                            state_monitor_head;
	struct cam_isp_context_state_monitor  cam_isp_ctx_state_monitor[
		CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES];
	struct cam_isp_context_req_id_info    req_info;
	bool                                  rdi_only_context;
	bool                                  skip_req_mgr;
	bool                                  hw_acquired;
	bool                                  init_received;
	bool                                  split_acquire;
	unsigned int                          init_timestamp;
	uint64_t                              last_request_accepted;
	uint64_t                              last_request_signaled;

	struct workqueue_struct              *bw_workq;
	struct work_struct                    bw_work;
	struct cam_isp_prepare_hw_update_data hw_update_data[2];
	uint32_t                              hw_update_index;
	struct mutex                          bw_lock;

	struct list_head                      stream_image_free_list;
	struct list_head                      stream_image_ready_list;
	struct list_head                      stream_image_umd_list;
	struct list_head                      stream_image_active_list;
	uint32_t                              num_stream_images;
	struct cam_isp_stream_image
		stream_images[CAM_MAX_STREAM_MODE_HANDLES];
	struct completion                     stream_image_completion;
	struct cam_isp_stream_image          *stream_image_applied;
	bool                                  stream_image_wait;
	int32_t                               stream_recovery_num_frames;
};

/**
 * cam_isp_context_init()
 *
 * @brief:              Initialization function for the ISP context
 *
 * @ctx:                ISP context obj to be initialized
 * @bridge_ops:         Bridge call back funciton
 * @hw_intf:            ISP hw manager interface
 * @ctx_id:             ID for this context
 *
 */
int cam_isp_context_init(struct cam_isp_context *ctx,
	struct cam_context *ctx_base,
	struct cam_req_mgr_kmd_ops *bridge_ops,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id);

/**
 * cam_isp_context_deinit()
 *
 * @brief:               Deinitialize function for the ISP context
 *
 * @ctx:                 ISP context obj to be deinitialized
 *
 */
int cam_isp_context_deinit(struct cam_isp_context *ctx);


#endif  /* __CAM_ISP_CONTEXT_H__ */
