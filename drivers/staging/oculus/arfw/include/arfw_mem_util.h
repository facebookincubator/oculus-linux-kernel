/* SPDX-License-Identifier: GPL-2.0                                           */
/*******************************************************************************
 * @file arfw_mem_util.h
 *
 * @brief Internal header for arfw utility functions.
 *
 *******************************************************************************/

#ifndef ARFW_MEM_UTIL_H
#define ARFW_MEM_UTIL_H

#include <linux/kref.h>
#include <linux/scatterlist.h>
#include <linux/wait.h>

#define VOID_PTR_OFFSET(ptr, offset) ((void *)((uint8_t *)(ptr) + (offset)))

/// Structure to hold the memory region allocated by the lower level module
/// like PCI, USB, loopback, etc
struct arfw_queue_mem_region {
	/// Kernel cpu address to the queue data.
	void *ptr;
	/// The size of the queue data region.
	size_t size;
	/// Device specific context information.
	void *context;
	/// Reference counter for the memory region.
	struct kref refcnt;
};

/**
 * Validate an arfw device id string. It should be a human readable string
 * consisting of [a-zA-Z-] and up to max_len characters in
 * length.
 */
int arfw_mem_util_check_arfw_device_id(const char *device_id, size_t max_len);

// Helper to get the dma address for an offset in a sg table
dma_addr_t arfw_mem_util_get_dma_addr(const struct sg_table *sgt,
				      size_t offset);

/**
 * Read functions for io memory
 *
 * @param[in] base_va the start virtual address of the bar
 * @param[in] offset the offset to the target field
 *
 * @retval the value of the field
 */
uint8_t arfw_mem_util_iomem_read_8(void *base_va, size_t offset);
uint16_t arfw_mem_util_iomem_read_16(void *base_va, size_t offset);
uint32_t arfw_mem_util_iomem_read_32(void *base_va, size_t offset);
uint64_t arfw_mem_util_iomem_read_64(void *base_va, size_t offset);

/**
 * Block read for io memory
 *
 * @param[in] base_va the start virtual address of the bar
 * @param[in] offset the offset to the target field
 * @param[out] dst the destination buffer
 * @param[in] len the number of bytes to read
 */
void arfw_mem_util_iomem_read_block(void *base_va, size_t offset, void *dst,
				    size_t len);

/**
 * Write functions for io memory
 *
 * @param[in] base_va the start virtual address of the bar
 * @param[in] offset the offset to the target field
 * @param[in] value value to set
 *
 */
void arfw_mem_util_iomem_write_8(void *base_va, size_t offset, uint8_t value);
void arfw_mem_util_iomem_write_16(void *base_va, size_t offset, uint16_t value);
void arfw_mem_util_iomem_write_32(void *base_va, size_t offset, uint32_t value);
void arfw_mem_util_iomem_write_64(void *base_va, size_t offset, uint64_t value);

/**
 * Block write for io memory
 *
 * @param[in] base_va the start virtual address of the bar
 * @param[in] offset the offset to the target field
 * @param[in] src the source buffer
 * @param[in] len the number of bytes to write
 */
void arfw_mem_util_iomem_write_block(void *base_va, size_t offset,
				     const void *src, size_t len);

/**
 * Perform clean up for the memory region allocated by device backend. Usually it is
 * the final step, when both FW and user space stop using it.
 *
 * @param[in] refcnt Reference counter of the arfw_queue_mem_region structure
 *
 * @retval None
 */
void arfw_mem_region_free(struct kref *refcnt);

#endif // !ARFW_MEM_UTIL_H
