/* SPDX-License-Identifier: LGPL-2.0+ WITH Linux-syscall-note */
/*
 * Driver interface for Adaptive Memory Kernel mode driver.
 *
 * Copyright Meta Platforms, Inc. and its affiliates.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define AMEM_IOCTL_CREATE_AMEM_MANAGER	1
#define AMEM_IOCTL_CREATE_AMEM		2
#define AMEM_IOCTL_LOAD			3
#define AMEM_IOCTL_EVICT		4
#define AMEM_IOCTL_GET_ID		5

/* The memory is zero initialized */
#define ZERO_INIT		BIT(0)

/* The memory is not retained while unpinned */
#define NOT_RETAINED		BIT(1)

/* The memory is incompressible (encrypted or already compressed) */
#define INCOMPRESSIBLE		BIT(2)

/* The memory needs to be physically contiguous for DMA usage.
 * This also means the physical memory can't be moved while pinned.
 */
#define DMA_BUF			BIT(3)

#define AMEM_VALID_FLAGS (ZERO_INIT | NOT_RETAINED | INCOMPRESSIBLE | DMA_BUF)


struct amem_create_manager_args {
	long client_tag;
};

struct amem_create_args {
	int amem_manager_fd;		/* file descriptor of the initialized amem_manager */
	size_t size;			/* Size of memory to allocate */
	size_t max_latency_ns;		/* maximum latency of memory in nanoseconds */
	u64 flags;
};
