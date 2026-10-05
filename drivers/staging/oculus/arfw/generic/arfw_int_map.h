/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfw_int_map.h
 *
 * @brief Internal header for arfw client queue common dma operations.
 *
 * @details
 *
 *******************************************************************************/

#ifndef ARFW_INT_MAP_H
#define ARFW_INT_MAP_H

#include "arfw_ops.h"
#include "arfw_int.h"

/**
 * Map the queue memory for a arfw_client_queue. This will take the memory
 * segment described by the queue segment (assumed to be in userspace), and map
 * it to be used in kernel code.
 *
 * Assumes client queue exists and lifetime lock is held.
 *
 * @param[in] client The client. Segment is mapped for this queue.
 * @param[in] queue_meta_segment The memory segment backing the client queue metadata, which
 * is assumed to describe a userspace memory segment.
 */
int arfw_map_queue(struct arfw_client_queue *client,
		   struct ar_mem_segment *queue_meta_segment);

/**
 * Unmap the queue memory for a arfw_client_queue.
 *
 * Assumes client queue exists.
 *
 * @param[in] client the queue for which to unmap the memory
 */
void arfw_unmap_queue(struct arfw_client_queue *client);

/**
 * Takes a user region, pins the pages, and then maps them into contiguous
 * kernel virtual memory if specified.
 *
 * @param[in]  region     the user region to be mapped
 * @param[out] mapping    metadata for the kernel mapping, required for unmap
 * @param[in]  map_kernel whether to map the region into kernel virtual memory
 */
int arfw_map_region(const struct ar_mem_region *region,
		    struct arfw_client_region_mapping *mapping,
		    bool map_kernel);

/**
 * Unmaps a mapped user region, unpinning the underlying pages as well.
 *
 * @param[in] mapping the metadata for the kernel mapping.
 */
void arfw_unmap_region(struct arfw_client_region_mapping *mapping);

/**
 * Takes a mapped region and dma maps it.
 *
 * Assumes client queue exists and lifetime lock is held.
 *
 * @param[in] client    the queue for which to unmap the memory
 * @param[in] region_id unique id for the region owning the mapping
 * @param[in] mapping   a mapped region
 */
int arfw_dma_map_region(struct arfw_client_queue *client, uint16_t region_id,
			struct arfw_client_region_mapping *mapping);

/**
 * Takes a mapped region and dma unmaps it.
 *
 * Assumes client queue exists.
 *
 * @param[in] client    the queue for which to unmap the memory
 * @param[in] region_id unique id for the region owning the mapping
 * @param[in] mapping   a mapped region
 */
void arfw_dma_unmap_region(struct arfw_client_queue *client, uint16_t region_id,
			   struct arfw_client_region_mapping *mapping);

#endif // !ARFW_INT_MAP_H
