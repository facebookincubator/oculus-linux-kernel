/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef CAM_FASTRPC_H
#define CAM_FASTRPC_H

#include <linux/types.h>

#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/fastrpc.h>
#include "cam_mem_mgr.h"

/** struct fastrpc_apps - fastrpc info structure
 *
 * @state                    : State of fasrpc driver
 * @debug_mask               : debug log level for fastrpc
 * @fastrpc_driver           : Fastrpc driver structure
 * @fastrpc_device           : Fastrpc device structure
 * @fastrpc_probe_completion : completion to track fastrpc probe
 */

struct fastrpc_apps {
	uint32_t state;
	uint32_t debug_mask;
	uint32_t handle;
	struct fastrpc_driver fastrpc_driver;
	struct fastrpc_device *fastrpc_device;
	struct completion fastrpc_probe_completion;
};
/**
 * cam_fastrpc_driver_register()
 *
 * @brief:  Register nsp jpeg handle with fastrpc.
 *
 * @handle: handle to nsp jpeg driver.
 *
 * @return 0 on success
 */
int cam_fastrpc_driver_register(uint32_t handle);

/**
 * cam_fastrpc_driver_unregister()
 *
 * @brief:  unregister nsp jpeg handle with fastrpc.
 *
 * cam_jpeg_mgr_nsp_acquire_hw()
 *
 * @return 0 on success
 */
int cam_fastrpc_driver_unregister(uint32_t handle);

int cam_fastrpc_dev_unmap_dma(
		struct cam_mem_buf_queue *buf);

int cam_fastrpc_dev_map_dma(struct cam_mem_buf_queue *buf,
			uint32_t dsp_remote_map,
			uint64_t *v_dsp_addr);

extern struct fastrpc_apps gfa_cv;
#endif // CAM_FASTRPC_H

