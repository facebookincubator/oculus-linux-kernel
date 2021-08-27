/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_FASTPATH_QUEUE_H_
#define CAM_FASTPATH_QUEUE_H_

#include <linux/list.h>
#include <linux/miscdevice.h>
#include <media/cam_fastpath_uapi.h>

#define CAM_FP_MAX_NAME_SIZE 32

struct cam_fp_process_chain {
	atomic_t idx;
	struct cam_fp_chain chain;
	void *fpq[CAM_FP_MAX_BUF_SETS_IN_CHAIN];
};

struct cam_fp_buffer_list {
	struct list_head list;
	struct cam_fp_buffer_set buf_set;
	struct cam_fp_process_chain *proc_chain;
};

struct cam_fp_queue_ops {
	int (*queue_buf_notify)(void *priv);
	int (*flush)(void *priv);
};

struct cam_fp_queue {
	struct mutex mutex;

	struct miscdevice misc_device;
	char name[CAM_FP_MAX_NAME_SIZE];

	struct list_head processing_queue;
	struct list_head pending_queue;
	struct list_head done_queue;
	struct list_head free_queue;

	wait_queue_head_t done_wq;

	unsigned int max_bufs;
	struct cam_fp_buffer_list *buf_mem;

	const struct cam_fp_queue_ops *ops;
	void *priv;

	struct kmem_cache *chain_mem;
};

struct cam_fp_buffer_set *cam_fp_queue_get_buffer_set(struct cam_fp_queue *fpq);
int cam_fp_queue_buffer_set_done(struct cam_fp_queue *fpq,
				 u64 request_id, u64 timestamp,
				 enum cam_fp_buffer_status status);

int cam_fp_queue_init(struct cam_fp_queue *fpq,
		      const char *name,
		      unsigned int max_bufs,
		      const struct cam_fp_queue_ops *ops,
		      void *priv);
int cam_fp_queue_deinit(struct cam_fp_queue *fpq);

#endif  /* CAM_FASTPATH_QUEUE_H_ */
