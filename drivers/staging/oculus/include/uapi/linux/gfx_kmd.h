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

enum gfx_kmd_buffer_direction {
	GFX_KMD_BUFFER_BIDIRECTIONAL,
	GFX_KMD_BUFFER_TO_DEVICE,
	GFX_KMD_BUFFER_FROM_DEVICE,
};

// We need to return bus address for the buffer here because
// there is no control queue as in Janus to send them outside of
// user-space ring buffer. So user-space is responsible for communicating it.
struct __packed gfx_kmd_ioct_register_buffer_req {
	enum gfx_kmd_buffer_direction in_direction;
	uint64_t in_fd;
	uint64_t in_size;
	uint32_t out_id;
	uint64_t out_addr;
};

struct __packed gfx_kmd_ioct_unregister_buffer_req {
	uint32_t id;
};

#define GFX_KMD_IOCTL_MAGIC 0xC9
#define GFX_KMD_IOCTL_REGISTER_BUFFER \
	_IOWR(GFX_KMD_IOCTL_MAGIC, 0, struct gfx_kmd_ioct_register_buffer_req *)
#define GFX_KMD_IOCTL_UNREGISTER_BUFFER \
	_IOW(GFX_KMD_IOCTL_MAGIC, 1, struct gfx_kmd_ioct_unregister_buffer_req *)

#endif // !GFX_KMD_H
