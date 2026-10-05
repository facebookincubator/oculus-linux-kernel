/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfw_int_queue.h
 *
 * @brief Internal header for arfw client queue operations
 *
 * @details
 *
 *******************************************************************************/

#ifndef ARFW_INT_QUEUE_H
#define ARFW_INT_QUEUE_H

#include <linux/arfw_io_interface.h>
#include <linux/device.h>

#include <arfw_ops.h>
#include <ar_fw.h>

/**
 * Create an arfw_client_queue. Blocks until the client queue is created.
 *
 * Acquires the client queue lifetime lock for construction, which blocks work
 * on the queue until queue creation completes.
 *
 * @param[in] driver_ops the underlying hw driver ops
 * @param[in] base_context the base context for queue creation
 * @param[in] dev device handle to store in the created client queue
 * @param[in] alloc_device device to allocate and track queue data context
 * @param[in] req the queue create request
 * @param[in] dev_info the device information
 * @param[in, out] client The client to create the queue for.
 *
 * @retval 0 No error.
 *
 * @retval -EINVAL Passed client request params are invalid.
 * @retval -ENOMEM Failed to allocate queue/sampler.
 * @retval -EFAULT Failed to map/pin queue pages.
 */
int arfw_queue_create(const struct arfw_driver_ops *driver_ops,
		      void *base_context, struct device *dev,
		      struct device *alloc_device,
		      struct ar_queue_create_req *req,
		      struct arfw_device_information_req *dev_info,
		      struct arfw_client_queue *client);

/**
 * Destroy an arfw_client_queue.
 *
 * Assumes the lifetime lock was previously acquired for destruction.
 *
 * @param[in] client_queue The client queue to destroy.
 * @param[in] base_context The base context from arfw_device_register
 *
 * @retval 0 if client_queue was destroyed and freed.
 * @retval -err if client_queue was not destroyed
 */
int arfw_queue_destroy(struct arfw_client_queue *client_queue,
		       void *base_context);

/**
 * Enqueue an event onto an arfirmware queue.
 *
 * @param[in] client   The client queue to enqueue an event onto
 * @param[in] event_in The event to enqueue
 *
 * @retval 0 of event was successfully popped off the client queue
 * @retval -err on failure
 */
int arfw_queue_event_enqueue(struct arfw_client_queue *client,
			     struct arfw_client_event *event_in);

/**
 * Dequeue an event off of an arfirmware queue.
 *
 * @param[in]  client     The client queue to dequeue an event from
 * @param[out] events_out The events to fill out
 *
 * @retval count of events that were successfully popped off the client queue
 * @retval -err  on failure
 */
int arfw_queue_event_dequeue(struct arfw_client_queue *client,
			     struct arfw_client_event_batch *events_out);

/**
 * Wait until an event can be deqeued off of an arfirmware queue, and dequeue it.
 *
 * @param[in]  client    The client queue to dequeue an event from
 * @param[out] event_out The events to fill out
 *
 * @retval count of events that were successfully popped off the client queue
 * @retval -err on failure
 */
int arfw_queue_event_wait_dequeue(struct arfw_client_queue *client,
				  struct arfw_client_event_batch *events_out);

/**
 * Check if there is an event ready on a queue.
 *
 * @retval 1 if message is ready
 * @retval 0 if no message
 * @retval -err on err
 */
int arfw_queue_event_ready(struct arfw_client_queue *client);

/**
 * Register a memory region for a queue.
 *
 * @param[in] client The client queue to register a region for.
 * @param[in] region The region to register.
 * @param[in] managed Whether the region is managed by the client library.
 *
 * @retval 0 No error.
 *
 * @retval -ENOMEM Failed to allocate region.
 * @retval -EFAULT Failed to map/pin region pages.
 */
int arfw_queue_register_region(struct arfw_client_queue *client,
			       struct ar_mem_region *region, bool managed);

/**
 * Register an aperture memory region for a queue.
 *
 * @param[in] client The client queue to register a region for.
 * @param[in] va kernel va of aperture alloc region
 * @param[in] dma_addr dma addr of aperture alloc region
 * @param[in] size size of aperture alloc region
 *
 * @retval region id, >= ARFW_REGION_ID_MIN on no error.
 *
 * @retval -ENOMEM Failed to allocate region.
 * @retval -EFAULT Failed to map/pin region pages.
 */
int arfw_queue_register_aperture_region(struct arfw_client_queue *client,
					void *va, dma_addr_t dma_addr,
					size_t size);

/**
 * Register an aperture memory region for a queue.
 *
 * @param[in] client The client queue to register a region for.
 * @param[in] region_id the region id to unregister
 *
 * @retval 0 No error.
 *
 * @retval -ENOENT Failed to locate the region by id.
 */
int arfw_queue_unregister_aperture_region(struct arfw_client_queue *client,
					  int region_id);

/**
 * Drain all the events queue and free all the events. It is expected that
 * this function should be called only from the release() callback. Because
 * of it this routine isn't protected by any lock.
 *
 * @param[in] client The client queue to free all the events.
 *
 * @retval None
 */
void arfw_queue_event_cleanup(struct arfw_client_queue *client);

/**
 * Unregister a memory region for a queue.
 *
 * @param[in] client The client queue to unregister a region for.
 * @param[in] region_id The region id to unregister.
 *
 * @retval 0 No error.
 *
 * @retval -ENOENT Failed to locate the region by id.
 */
int arfw_queue_unregister_region(struct arfw_client_queue *client,
				 int region_id);

/**
 * Reference a memory region for a queue.
 * This is done to ensure regions cannot be unpinned with inflight dma operations.
 *
 * IMPORTANT: this assumes we already hold a lock on queue regions.
 *
 * @param[in] client The client queue to reference a region for
 * @param[in] region_id The region to reference
 *
 * @retval 0 on success
 * @retval -err on failure
 */
int arfw_queue_ref_region(struct arfw_client_queue *client, uint16_t region_id);

/**
 * Dereference a memory region for a queue.
 * This is done to ensure regions cannot be unpinned with inflight dma operations.
 * If the region was previously requested to be released/unregistered we also
 * clean it up and unmap/unpin.
 *
 * IMPORTANT: acquires a lock on queue regions.
 *
 * @param[in] client The client queue to reference a region for
 * @param[in] region_id The region to reference
 *
 * @retval 0 on success
 * @retval -err on failure
 */
int arfw_queue_deref_region(struct arfw_client_queue *client,
			    uint16_t region_id);

/**
 * Mark the external payload as consumed by dereferencing the used memory region ids and
 * queue the events for the user space.
 *
 * @retval 0 on success
 * @retval -err on failure
 */
int arfw_queue_external_payload_consumed(struct arfw_client_queue *client,
					 void *header);

#endif // !ARFW_INT_QUEUE_H
