/*
 * drivers/staging/android/ion/msm_ion_priv.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MSM_ION_PRIV_H
#define _MSM_ION_PRIV_H

#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/seq_file.h>

struct ion_heap *ion_iommu_heap_create(struct ion_platform_heap *heap);
void ion_iommu_heap_destroy(struct ion_heap *heap);

struct ion_heap *ion_cp_heap_create(struct ion_platform_heap *heap);
void ion_cp_heap_destroy(struct ion_heap *heap);

struct ion_heap *ion_system_secure_heap_create(struct ion_platform_heap *heap);
void ion_system_secure_heap_destroy(struct ion_heap *heap);
int ion_system_secure_heap_prefetch(struct ion_heap *heap, void *data);
int ion_system_secure_heap_drain(struct ion_heap *heap, void *data);

struct ion_heap *ion_cma_secure_heap_create(struct ion_platform_heap *heap);
void ion_cma_secure_heap_destroy(struct ion_heap *heap);

struct ion_heap *ion_secure_carveout_heap_create(
			struct ion_platform_heap *heap);
void ion_secure_carveout_heap_destroy(struct ion_heap *heap);

long msm_ion_custom_ioctl(struct ion_client *client,
			  unsigned int cmd,
			  unsigned long arg);

#ifdef CONFIG_CMA
struct ion_heap *ion_secure_cma_heap_create(struct ion_platform_heap *heap);
void ion_secure_cma_heap_destroy(struct ion_heap *heap);

int ion_secure_cma_prefetch(struct ion_heap *heap, void *data);

int ion_secure_cma_drain_pool(struct ion_heap *heap, void *unused);

#else
static inline int ion_secure_cma_prefetch(struct ion_heap *heap, void *data)
{
	return -ENODEV;
}

static inline int ion_secure_cma_drain_pool(struct ion_heap *heap, void *unused)
{
	return -ENODEV;
}

#endif

struct ion_heap *ion_removed_heap_create(struct ion_platform_heap *pheap);
void ion_removed_heap_destroy(struct ion_heap *heap);

#define ION_CP_ALLOCATE_FAIL -1
#define ION_RESERVED_ALLOCATE_FAIL -1

void ion_cp_heap_get_base(struct ion_heap *heap, unsigned long *base,
			  unsigned long *size);

void ion_mem_map_show(struct ion_heap *heap);

int ion_heap_is_system_secure_heap_type(enum ion_heap_type type);

int ion_heap_allow_secure_allocation(enum ion_heap_type type);

int ion_heap_allow_heap_secure(enum ion_heap_type type);

int ion_heap_allow_handle_secure(enum ion_heap_type type);

int get_secure_vmid(unsigned long flags);

bool is_secure_vmid_valid(int vmid);

/**
 * Functions to help assign/unassign sg_table for System Secure Heap
 */

int ion_system_secure_heap_unassign_sg(struct sg_table *sgt, int source_vmid);
int ion_system_secure_heap_assign_sg(struct sg_table *sgt, int dest_vmid);

/**
 * ion_create_chunked_sg_table - helper function to create sg table
 * with specified chunk size
 * @buffer_base:	The starting address used for the sg dma address
 * @chunk_size:		The size of each entry in the sg table
 * @total_size:		The total size of the sg table (i.e. the sum of the
 *			entries). This will be rounded up to the nearest
 *			multiple of `chunk_size'
 */
struct sg_table *ion_create_chunked_sg_table(phys_addr_t buffer_base,
					     size_t chunk_size,
					     size_t total_size);

void show_ion_usage(struct ion_device *dev);
#endif /* _MSM_ION_PRIV_H */
