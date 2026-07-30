/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_DMABUF_REORDER_H
#define _UAPI_LINUX_DMABUF_REORDER_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DMABUF_REORDER_MAX_REGIONS 16

/**
 * struct dmabuf_reorder_region - one contiguous byte range in the source buffer
 * @offset: byte offset in source (must be page-aligned)
 * @length: byte length of this region (must be page-aligned)
 */
struct dmabuf_reorder_region {
	__u64 offset;
	__u64 length;
};

/**
 * struct dmabuf_reorder_request - ioctl request to reorder dma-buf regions
 * @src_fd:      source dma-buf fd
 * @out_fd:      returned new dma-buf fd with reordered layout
 * @num_regions: number of regions in the array (1..DMABUF_REORDER_MAX_REGIONS)
 * @pad:         reserved, must be zero
 * @regions_ptr: userspace pointer to struct dmabuf_reorder_region[]
 *
 * Creates a new dma-buf that maps the same physical pages as @src_fd
 * but with the scatterlist reordered according to the regions array.
 * Region 0 appears first in the new buffer, then region 1, etc.
 * All offsets and lengths must be page-aligned.
 * Regions must not overlap.
 *
 * The wrapper is DMA-map only — mmap() is not supported.  CPU access
 * ops (begin/end_cpu_access) sync the wrapper's reordered DMA mappings,
 * not the source buffer directly.
 *
 * The exported dma-buf size equals the sum of all region lengths.
 */
struct dmabuf_reorder_request {
	__s32 src_fd;
	__s32 out_fd;
	__u32 num_regions;
	__u32 pad;
	__u64 regions_ptr;
};

#define DMABUF_REORDER_MAGIC 'R'
#define DMABUF_REORDER _IOWR(DMABUF_REORDER_MAGIC, 1, struct dmabuf_reorder_request)

#endif /* _UAPI_LINUX_DMABUF_REORDER_H */
