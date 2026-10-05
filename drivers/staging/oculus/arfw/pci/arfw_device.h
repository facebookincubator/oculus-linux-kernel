/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_device.h
 *
 * @brief Defines for ar pci device representation in the driver
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFIRMWARE_DEVICE_H
#define ARFIRMWARE_DEVICE_H

#include <arfw_ops.h>

// main driver source file will provide control ring timeout
extern unsigned long ctrl_ring_timeout_ms_param;

/**
 * Driver entry point to handle an arfw_io_request.
 *
 * Note: The driver may assume calls to this function are serialized within a
 * given queue but possibly concurrent across queues.
 *
 * @param[in] queue_context The hardware queue context.
 * @param[in] req Pointer to arfw_io_request.
 * @param[out] consumed Should the data be marked as consumed. If set to false,
 * the driver must later call, arfw_request_consumed.
 *
 * @retval TRUE Polling should continue
 */
bool ar_dev_handle_send_queue_request(void *queue_context,
				      const struct arfw_io_request *req,
				      bool *consumed);

/**
 * Driver callback when the client has consumed a buffer from a receive queue.
 * This may be required if the driver is sharing the queue directly to hardware
 * as a DMA target and read indexes need to be updated.
 *
 * @param[in] queue handle for the current queue.
 * @param[in] queue_context The hardware queue context.
 */
void ar_dev_handle_rcv_queue_consume(arfw_client_queue_t queue,
				     void *queue_context);

/**
 * Driver entry point when a client allocates memory for the queue data.
 * The queue data context is returned and could be used to get information about
 * the queue data.
 *
 * @param[in] base_context The base hw dev context.
 * @param[in] queue_params Queue information.
 *
 * @retval Pointer to the queue data memory region structure in case of success,
 *         otherwise NULL.
 */
struct arfw_queue_mem_region *ar_dev_handle_queue_data_alloc(
	void *base_context,
	const struct arfw_client_queue_create_params *queue_params);

/**
 * Driver entry point when a client frees memory for the queue data.
 *
 * @param[in] base_context The base hw dev context.
 * @param[in] queue_data The queue data memory region to clean up.
 */
void ar_dev_handle_queue_data_free(void *base_context,
				   struct arfw_queue_mem_region *queue_data);

/**
 * Driver entry point when a client queue is created.
 * This serves as a point for the driver to allocate a context and set it for
 * the client queue.
 *
 * @param[in] queue Client queue created for the request.
 * @param[in] base_context The base hw dev context, passed into arfw_device_unregister.
 * @param[in] queue_params Queue information.
 * @param[in] queue_data Queue data memory region information
 * @param[out] queue_context The context associated with the queue.
 *
 * @retval 0 No error.
 *
 * @retval -ENOSPC    Failed to reserve a send/recv ring slot, all in use.
 * @retval -ENOMEM    Failed to allocate memory for future/promise/queue.
 * @retval -ENOMEM    Failed to produce request to firmware, ctrl ring is full.
 * @retval -EINVAL    Passed invalid arguments to produce into ctrl ring (type, size).
 * @retval -EIO       Firmware response is unexpected/malformed.
 * @retval -EXISTS    Firmware nacked request, ring already exists.
 * @retval -ERANGE    Firmware nacked request, invalid ring count.
 * @retval -EINVAL    Firmware nacked request, invalid ring type.
 * @retval -E2BIG     Firmware nacked request, invalid inline message length.
 * @retval -ENXIO     Firmware nacked request, endpoint unreachable.
 * @retval -EINVAL    Firmware nacked request, depth too small.
 * @retval -EFAULT    Firmware nacked request, slot in use.
 * @retval -ETIMEDOUT Firmware request timed out.
 */
int ar_dev_handle_queue_create_request(
	arfw_client_queue_t queue, void *base_context,
	const struct arfw_client_queue_create_params *queue_params,
	struct arfw_queue_mem_region *queue_data, void **queue_context);

/**
 * Driver entry point when a queue is destroyed.
 * This allows the driver a chance to cancel any outstanding IO, and
 * release hardware resources.
 *
 * @param[in] base_context The base context from arfw_device_register.
 * @param[in] queue_context The context associated with this queue.
 *
 * @retval 0 No error.
 *
 * @retval -ENOLINK   Failed to notify firmware due to link being down.
 * @retval -ENOMEM    Failed to allocate memory for future/promise.
 * @retval -ENOMEM    Failed to produce request to firmware, ctrl ring is full.
 * @retval -EINVAL    Passed invalid arguments to produce into ctrl ring (type, size).
 * @retval -EIO       Firmware response is unexpected/malformed.
 * @retval -EBUSY     Firmware nacked request, ring is still active.
 * @retval -EINVAL    Firmware nacked request, ring does not exist.
 * @retval -ETIMEDOUT Firmware request timed out.
 */
int ar_dev_handle_queue_destroy_request(void *base_context,
					void *queue_context);
/**
 * Driver entry point the device information query.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[out] device_information The device information structure to populate
 *
 * @retval 0 The structure was populated
 */
int ar_dev_handle_get_device_information(
	void *base_context,
	struct arfw_device_information_req *device_information);

/**
 * Driver entry point to pend a receive external payload
 *
 * @param[in] queue_context The hardware queue context.
 * @param[in] req external payload pend request.
 *
 * @retval 0 No error.
 *
 * @retval -ENOMEM  Failed to produce request to firmware, buffer ring is full.
 * @retval -EINVAL  Passed invalid arguments to produce into buffer ring (type, size).
 * @retval -E2BIG   Passed buffer has too many sg elems (over the queue limit).
 */
int ar_dev_handle_receive_payload_pend(void *queue_context,
				       const struct arfw_payload_pend_req *req);

/**
 * Driver quirk for dma allocations
 *
 * @param[in] queue handle for the current queue
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] direction The device to/from direction used for DMA
 * @param[in] sgt The scatter-gatherer table used for DMA
 * @param[in] pages pages to dma map
 * @param[in] num_pages number of pages to map
 * @param[out] dma_addr dma address of the memory
 *
 * @retval 0 on success
 *
 * @retval -ENOMEM Failed to allocate sg chunks.
 * @retval -EINVAL Passed invalid arguments to kernel sg functions.
 * @retval -EFAULT Failed to DMA map the sg list.
 * @retval -ENOSPC Number of sg list elements returned is too big.
 */
int ar_dev_handle_dma_map_quirk(arfw_client_queue_t queue, void *base_context,
				void *queue_context,
				enum dma_data_direction direction,
				struct sg_table *sgt, struct page **pages,
				int num_pages, dma_addr_t *dma_addr);

/**
 * Driver quirk for dma frees
 *
 * @param[in] queue handle for the current queue
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associate
 * @param[in] direction The device to/from direction used for DMA
 * @param[in] sgt The scatter-gatherer table used for DMA
 * @param[in] va virtual address of the memory (should be page aligned)
 * @param[in] size Size of the memory (should be a multiple of PAGE_SIZE)
 * @param[in] dma_addr dma address of the memory
 */
void ar_dev_handle_dma_unmap_quirk(arfw_client_queue_t queue,
				   void *base_context, void *queue_context,
				   enum dma_data_direction direction,
				   struct sg_table *sgt, void *va, size_t size,
				   dma_addr_t dma_addr);

/**
 * Driver API to notify until the queue is constructed
 *
 * @param[in] queue_context The hardware queue context.
 */
void ar_dev_handle_notify_queue_ready(void *queue_context);

/**
 * Mmap the DMA address region to the user space.
 *
 * @param[in] base_context The device context which is used to mmap memory
 * @param[in] queue_data Pointer to the queue data memory region context to mmap
 * @param[in] vma VMA to map in.
 *
 * @retval 0 in case of success, otherwise return error code.
 */
int ar_dev_handle_queue_data_mmap(void *base_context,
				  struct arfw_queue_mem_region *queue_data,
				  struct vm_area_struct *vma);

int ar_dev_handle_aperture_alloc(void *base_context, void *queue_context,
				 size_t size, void **vaddr,
				 dma_addr_t *dma_addr);
void ar_dev_handle_aperture_free(void *base_context, void *queue_context,
				 void *vaddr, size_t size);
int ar_dev_handle_aperture_mmap(void *base_context, void *queue_context,
				void *va, size_t size,
				struct vm_area_struct *vma);

/**
 * ar_pci_wake_functions() - Wake specific PCIe functions from D3hot state
 * @func_bit_mask: Bitmask of functions to wake (bit N = function N)
 *
 * Return: 0 on success, negative error code on failure
 */
int ar_pci_wake_functions(uint8_t func_bit_mask);

#endif // !ARFIRMWARE_DEVICE_H
