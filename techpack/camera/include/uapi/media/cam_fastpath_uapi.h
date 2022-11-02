/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_FASTPATH_UAPI_H_
#define CAM_FASTPATH_UAPI_H_
#include <linux/types.h>

#define FP_DEV_HDL_IDX_SIZE        8
#define FP_DEV_HDL_IDX_POS         32

/* see cam_req_mgr hdl info */
#define FP_DEV_HDL_IDX_MASK          ((1 << FP_DEV_HDL_IDX_SIZE) - 1)
#define FP_DEV_HDL_IDX_SHIFT         (FP_DEV_HDL_IDX_POS - FP_DEV_HDL_IDX_SIZE)

#define FP_INSERT_IDX(CTX) \
	do { \
		((CTX)->dev_hdl &= \
			~((FP_DEV_HDL_IDX_MASK) << FP_DEV_HDL_IDX_SHIFT)); \
		((CTX)->dev_hdl |= \
			(((CTX)->ctx_id & FP_DEV_HDL_IDX_MASK) << FP_DEV_HDL_IDX_SHIFT)); \
	} while (0)

#define FP_DEV_GET_HDL_IDX(HNDL) \
	(((HNDL)>>FP_DEV_HDL_IDX_SHIFT) & FP_DEV_HDL_IDX_MASK)

/////////////////////////////////////////////////////////////////////////////

#define CAM_FP_MAX_NUM_PLANES 3
#define CAM_FP_MAX_BUFS 64
#define CAM_FP_MAX_BUF_SETS_IN_CHAIN 2

/**
 * enum cam_fp_buffer_status - Camera fastpath buffer status.
 *
 * @CAM_FP_BUFFER_STATUS_SUCCESS: Buffer is processed and contain valid data.
 * @CAM_FP_BUFFER_STATUS_ERROR: Error occoured during the processing. Data in the
 *                       buffer is not valid.
 */
enum cam_fp_buffer_status {
	CAM_FP_BUFFER_STATUS_SUCCESS,
	CAM_FP_BUFFER_STATUS_ERROR,
};

/**
 * struct cam_fp_buffer - Camera fastpath buffer.
 *
 * @num_planes: Number of valid planes in plane array.
 * @plane: Array containing buffer planes.
 * @plane.handle: Handle of the buffer plane.
 *                NOTE: the handle should be allocated and mapped in smmu.
 *	          Allocation should be done from cam_req_mgr.
 * @plane.offset: Buffer plane start offset.
 * @cache_maintenance: Flag set if cache maintenance need to be done
 *                     on driver side.
 * @reserved: Array of reserved fields for future use. Userspace should set
 *             those fields to 0.
 */
struct cam_fp_buffer {
	__u32 num_planes;
	struct {
		__s32 handle;
		__u32 offset;
	} plane[CAM_FP_MAX_NUM_PLANES];
	__u8 cache_maintenance;
	__u8 reserved[11];
};

/**
 * struct cam_fp_buffer_set - Camera fastpath buffer set. The structure
 *                            contain set of buffers to be processed.
 * @user_data: opaque user data - provided by user.
 * @request_id: Request id.
 * @in_buffer_set_mask: Bitmask contain valid buffers in in_bufs array.
 * @in_bufs: Array of fastpath input buffers.
 * @out_buffer_set_mask: Bitmask contain valid buffers in out_bufs array.
 * @out_bufs: Array of fastpath output buffers.
 * @timestamp: Timestamp of the buffer filled by the driver.
 *             Valid only on CAM_FP_DEQUEUE_BUFSET ioctl.
 * @status: Buffer set status. Status is of enum cam_fp_buffer_status type.
 *          Filled by the driver. Valid only on CAM_FP_DEQUEUE_BUFSET ioctl.
 * @sof_index: Counts sensor frames (SOF enents) received from CSI interface.
 */
struct cam_fp_buffer_set {
	__u64 user_data;

	__u64 request_id;

	__u64 in_buffer_set_mask;
	struct cam_fp_buffer in_bufs[CAM_FP_MAX_BUFS];

	__u64 out_buffer_set_mask;
	struct cam_fp_buffer out_bufs[CAM_FP_MAX_BUFS];

	__u64 timestamp;

	__u32 status;

	__u32 sof_index;
};

/**
 * struct cam_fp_chain - Camera fastpath chain of buffer sets. The structure
 *                       contains chain of buffer sets to be processed.
 *                       Next set will be enqueued when previous finishes.
 * @num: Number of valid fd's and buffer sets in the array's.
 * @fd: Array оf Valid fd's on which the buffer set with corresponding index
 *      need to be enqueued. Fd on first index will be ignored by the driver.
 *      Buffer set on index 0 will be enqueued on the device this ioctl was
 *      called.
 * @buf_set: Array of buffers sets in the chain. Each buffer set will be
 *           enqueued on corresponding fd's when previous buffer set is done.
 */
struct cam_fp_chain {
	__u32 num;
	__s32 fd[CAM_FP_MAX_BUF_SETS_IN_CHAIN];
	struct cam_fp_buffer_set buf_set[CAM_FP_MAX_BUF_SETS_IN_CHAIN];
};

/**
 * CAM_FP_ENQUEUE_BUFSET - Queue camera buffer set
 *
 * Enqueue camera buffer set for processing. When processing is done,
 * userspace can retrieve back the buffer with CAM_FP_DEQUEUE_BUFSET ioctl.
 */
#define CAM_FP_ENQUEUE_BUFSET _IOWR('p', 0, struct cam_fp_buffer_set)

/**
 * CAM_FP_DEQUEUE_BUFSET - Dequeue camera buffer set
 *
 * Dequeue camera buffer set for processing. If buffer set is not
 * available error will be returned. Userspace can wait for ready buffer set
 * with poll.
 */
#define CAM_FP_DEQUEUE_BUFSET _IOWR('p', 1, struct cam_fp_buffer_set)

/**
 * CAM_FP_QUEUE_CHAIN - Queue camera chain of buffer sets
 *
 * Enqueue camera chain of buffer sets for processing. When processing is done,
 * userspace won't be notified, but the next buffer set will be enqueued.
 * The set should be enqueued on first device and will be dequeued on last
 * device as a final buffer set in the chain. This is an agreement between uapi
 * and kernel. The main reason this is done is to reduce ioctl calls.
 */
#define CAM_FP_QUEUE_CHAIN _IOWR('p', 2, struct cam_fp_chain)

#endif  /* CAM_FASTPATH_UAPI_H_ */
