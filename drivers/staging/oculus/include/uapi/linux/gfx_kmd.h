/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*****************************************************************************
 * @file gfx_kmd.h
 *
 * @brief Interface for Graphics PCIe kernel mode driver.
 *
 * @details This driver enables communications with GMCU on CP over PCIe bus.
 * Exposes a shared ring buffer region into the user space via
 * a direct memory mapping.
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#ifndef GFX_KMD_H
#define GFX_KMD_H

#include <linux/ioctl.h>

// Upper bound on how many sg list elements per
// buffer we support - for simplicity for now not variable.
#define GFX_KMD_BUFFER_CHUNKS_MAX 10

// If possible client should specify the direction
// for the buffer dma operations. For speeds.
enum gfx_kmd_buffer_direction {
	GFX_KMD_BUFFER_BIDIRECTIONAL,
	GFX_KMD_BUFFER_TO_DEVICE,
	GFX_KMD_BUFFER_FROM_DEVICE,
};

// One buffer might result in a number of dma
// mappings (sg list elements), this represents one of them.
struct __packed gfx_kmd_buffer_chunk {
	uint64_t addr;
	uint64_t size;
};

// We need to return bus addresses for the buffer here because
// there is no control queue as in Janus to send them outside of
// user-space ring buffer. So user-space is responsible for communicating it.
struct __packed gfx_kmd_ioct_register_buffer_req {
	enum gfx_kmd_buffer_direction in_direction;
	uint64_t in_fd;
	uint64_t in_size;
	uint32_t out_id;
	struct gfx_kmd_buffer_chunk out_chunks[GFX_KMD_BUFFER_CHUNKS_MAX];
	uint32_t out_chunks_count;
};

// When done with a buffer let the kernel know it's safe to clean up.
// This should not be done with in-flight dma or we all crash.
struct __packed gfx_kmd_ioct_unregister_buffer_req {
	uint32_t id;
};

#define GFX_KMD_IOCTL_MAGIC 0xC9
#define GFX_KMD_IOCTL_REGISTER_BUFFER \
	_IOWR(GFX_KMD_IOCTL_MAGIC, 0, struct gfx_kmd_ioct_register_buffer_req *)
#define GFX_KMD_IOCTL_UNREGISTER_BUFFER \
	_IOW(GFX_KMD_IOCTL_MAGIC, 1, struct gfx_kmd_ioct_unregister_buffer_req *)

#endif // !GFX_KMD_H
