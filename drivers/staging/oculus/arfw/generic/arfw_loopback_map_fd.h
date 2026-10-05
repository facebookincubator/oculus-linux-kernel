/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfw_loopback_map_fd.h
 *
 * @brief Helper for mapping regions into userspace.
 *
 * @details
 *
 *******************************************************************************/

#pragma once

#include <linux/arfw_types.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>

#include "arfw_int.h"

struct arfw_loopback_map_fd;

typedef void (*arfw_loopback_map_fd_release_t)(
	struct arfw_loopback_map_fd *self);
typedef int (*arfw_loopback_map_fd_mmap_t)(struct arfw_loopback_map_fd *self,
					   struct vm_area_struct *vma);

struct arfw_loopback_map_fd_ops {
	arfw_loopback_map_fd_mmap_t mmap;
	arfw_loopback_map_fd_release_t release;
};

struct arfw_loopback_map_fd {
	void *vmaddr;
	size_t size;
	void *context;
	struct arfw_loopback_map_fd_ops *ops;
};

/**
 * Creates a new arfw_loopback_map_fd struct. This struct is used to map a region
 * to a file descriptor, which can be mmaped by the user.
 */
int arfw_loopback_map_fd_create(const char *name, void *context, void *vmaddr,
				size_t size,
				struct arfw_loopback_map_fd_ops *ops);

/**
 * Destroy the arfw_loopback_map_fd struct.
 */
void arfw_loopback_map_fd_destroy(
	struct arfw_loopback_map_fd *loopback_map_data);
