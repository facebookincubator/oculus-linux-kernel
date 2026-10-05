/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfw_ops.h
 *
 * @brief Interface definiton for building arfirmware char devices by arfirmware
 *        device drivers.
 *
 *******************************************************************************/

#ifndef ARFW_OPS_H
#define ARFW_OPS_H

#include <linux/arfw_types.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/uuid.h>

#include <arfw_mem_util.h>
#include <ar_common.h>
#include <ar_fw_message.h>

/**
 * Opaque type for a specific ar firmware client queue.
 */
typedef void *arfw_client_queue_t;

/**
 * IO request as read from the data queue
 */
struct arfw_io_request {
	/// pointer to the data
	void *data;
	/// size of the data
	uint32_t data_size;
	/// Index of the data in the circular queue
	uint32_t index;
};

/**
 * Params for creating the AR Firmware queue sent to the firmware.
 */
struct arfw_client_queue_create_params {
	/// Direction that this queue transfers data in. Expected to be a arfw_client_queue_direction.
	enum ar_queue_direction queue_direction;
	/// The buffer size of queue elements
	uint32_t element_size;
	/// The count of elements in the queue
	uint16_t depth;
	/// The hlos id of the queue for firmware IPC
	ar_endpoint_id_t hlos_endpoint_id;
	/// The fw id of the queue for firmware IPC
	ar_endpoint_id_t fw_endpoint_id;
	/// Virtual address of queue.
	void *queue_location;
	/// index the queue is currently reserved up to
	int initial_read_index;
	/// Whether this queue is mirroring another one in the user space
	bool queue_mirror;
};

/**
 * Holds device information returned by arfw devices.
 */
struct arfw_device_information_req {
	uint16_t transport_header_size;
	uint16_t inline_data_offset;
	uint16_t send_ring_max;
	uint16_t rcv_ring_max;
	bool require_contiguous_memory_for_queues;
	uint16_t rcv_ring_pend_buff_count_max;
};

/**
 * DMA scatter-gather element
 */
struct arfw_dma_sg_elem {
	dma_addr_t addr;
	uint32_t len;
	uint16_t mem_id;
	uint16_t reserved;
};

#define ARFW_PEND_SG_LEN_MAX 8

/*
 * There is inplace conversion from the user known the ar_firmware_msg_sg_elem_t structure
 * to the kernel one. Because of it we rely on the size and offsets. If one of the structures
 * changed, the inplace conversion should be also changed.
 */
static_assert(sizeof(struct arfw_dma_sg_elem) ==
	      sizeof(ar_firmware_msg_sg_elem_t));
static_assert(offsetof(struct arfw_dma_sg_elem, addr) ==
	      offsetof(ar_firmware_msg_sg_elem_t, roundtrip));
static_assert((offsetof(struct arfw_dma_sg_elem, addr) + 4) ==
	      offsetof(ar_firmware_msg_sg_elem_t, offset));
static_assert(offsetof(struct arfw_dma_sg_elem, len) ==
	      offsetof(ar_firmware_msg_sg_elem_t, size));

/**
 * Request to pend a payload, sent to HW specific driver.
 * Here roundtrip_id = mem_region_id + buf_pend_id.
 */
struct arfw_payload_pend_req {
	roundtrip_id_t roundtrip;
	struct arfw_dma_sg_elem sg_list[ARFW_PEND_SG_LEN_MAX];
	size_t sg_list_len;
};

/**
 * Allocate memory for the data region in the queue. The allocation is
 * device specific. Some devices could require DMA, some devices could just
 * use kmalloc, vmalloc, etc.
 *
 * @param[in] base_context The backend device context which is used for the
 *                         specific allocation
 * @param[in] queue_params Queue information used to allocate data
 *
 * @retval Valid pointer to the queue memory region structure with the queue
 *         data context, otherwise returns NULL. This structure should be dynamically
 *         allocated by the device module with the kmalloc call. This memory should
 *         be manually freed by the caller.
 */
typedef struct arfw_queue_mem_region *(*arfw_handle_queue_data_alloc_request_t)(
	void *base_context,
	const struct arfw_client_queue_create_params *queue_params);

/**
 * Free the queue data memory. This is a device specific call to clean up
 * the resources used (mappings, DMA if any, etc).
 *
 * @param[in] base_context The backend device context which is used to clean
 *                         up specific allocations
 * @param[in] queue_data Pointer to the queue data memory region to
 *            perform device specific clean up
 */
typedef void (*arfw_handle_queue_data_free_request_t)(
	void *base_context, struct arfw_queue_mem_region *queue_data);

/**
 * Driver entry point when a client queue is created.
 * This serves as a point for the driver to allocate a context and set it for
 * the client queue.
 *
 * NOTE:
 * Will only be called once per file descriptor.
 * Acquires & releases the queue lifetime lock for construction.
 *
 * @param[in] queue Client queue created for the request
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_params Queue information
 * @param[in] queue_data The pointer to the queue data memory region structure
 * @param[out] queue_context The context associated with the queue
 *
 * @retval 0 No error.
 */
typedef int (*arfw_handle_queue_create_request_t)(
	arfw_client_queue_t queue, void *base_context,
	const struct arfw_client_queue_create_params *queue_params,
	struct arfw_queue_mem_region *queue_data, void **queue_context);

/**
 * Driver entry point to handle an arfw_io_request.
 *
 * NOTE:
 * The driver may assume calls to this function are serialized within a
 * given queue but possibly concurrent across queues.
 * Only valid for queue direction AR_QUEUE_HLOS_TO_FW.
 * Unsafe to invoke APIs which grab the queue serial work lock. Callback is invoked
 * with queue serial work lock already held.
 *
 * @param[in] queue_context The hardware queue context.
 * @param[in] req Pointer to arfw_io_request.
 * @param[out] consumed Should the data be marked as consumed. If set to false,
 * the driver must later call, arfw_request_consumed.
 *
 * @retval TRUE Polling should continue
 */
typedef bool (*arfw_handle_send_queue_request_t)(
	void *queue_context, const struct arfw_io_request *req, bool *consumed);

/**
 * Mark a previous IO as consumed, allowing the slot in the queue to be reused.
 *
 * NOTE:
 * Only valid for queue direction AR_QUEUE_HLOS_TO_FW.
 * Acquires & releases the queue serial work lock.
 *
 * @param[in] queue handle for the current queue
 * @param[in] index The index from req in
 * arfw_handle_send_queue_request_t (index of the slot in the circular queue)
 *
 * @retval 0 on success
 * @retval -ESHUTDOWN The client is already torn down
 * @retval -EINTR The call was interrupted.
 */
typedef int (*arfw_handle_client_queue_index_consumed_t)(
	arfw_client_queue_t queue, uint32_t index);

/**
 * Driver callback when the client has consumed a buffer from a receive queue.
 * This may be required if the driver is sharing the queue directly to hardware
 * as a DMA target and read indexes need to be updated.
 *
 * NOTE:
 * Only valid for queue direction AR_QUEUE_FW_TO_HLOS.
 * Safe to call APIs that grab client work lock. Callback is invoked without
 * having queue serial work lock held.
 *
 * @param[in] queue Handle for the current queue
 * @param[in] queue_context The hardware queue context.
 */
typedef void (*arfw_handle_rcv_queue_consume_t)(arfw_client_queue_t queue,
						void *queue_context);

/**
 * Driver entry point when a queue is destroyed.
 * This allows the driver a chance to cancel any outstanding IO, and
 * release hardware resources.
 *
 * NOTE:
 * Is invoked without having queue serial work lock held. However, this will only be
 * invoked once after all outstanding async work has completed, and no further
 * async work will be scheduled after invoking this callback.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue
 *
 * @retval 0 No error.
 */
typedef int (*arfw_handle_queue_destroy_request_t)(void *base_context,
						   void *queue_context);

/**
 * Driver entry point the device information query.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[out] device_information The device information structure to populate
 *
 * @retval 0 The structure was populated
 */
typedef int (*arfw_handle_get_device_information_t)(
	void *base_context,
	struct arfw_device_information_req *device_information);

/**
 * Driver entry point to pend a receive external payload
 *
 * NOTE:
 * Invoked without having queue serial work lock held.
 *
 * @param[in] queue_context The hardware queue context.
 * @param[in] req External payload pend request.
 *
 * @retval 0 No error.
 */
typedef int (*arfw_handle_receive_payload_pend_t)(
	void *queue_context, const struct arfw_payload_pend_req *req);

/**
 * Driver quirk for dma mappings
 *
 * @param[in] queue Handle for the current queue
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] direction The device to/from direction used for DMA
 * @param[in] sgt The scatter-gatherer table used for DMA
 * @param[in] pages Pages to dma map
 * @param[in] num_pages Number of pages to map
 * @param[out] dma_addr DMA address of the memory
 *
 * @retval 0 on success
 */
typedef int (*arfw_dma_map_quirk_t)(arfw_client_queue_t queue,
				    void *base_context, void *queue_context,
				    enum dma_data_direction direction,
				    struct sg_table *sgt, struct page **pages,
				    int num_pages, dma_addr_t *dma_addr);

/**
 * Driver quirk for dma unmappings
 *
 * @param[in] queue Handle for the current queue
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] direction The device to/from direction used for DMA
 * @param[in] sgt The scatter-gatherer table used for DMA
 * @param[in] va Virtual address of the memory (should be page aligned)
 * @param[in] size Size of the memory (should be a multiple of PAGE_SIZE)
 * @param[in] dma_addr DMA address of the memory
 */
typedef void (*arfw_dma_unmap_quirk_t)(arfw_client_queue_t queue,
				       void *base_context, void *queue_context,
				       enum dma_data_direction direction,
				       struct sg_table *sgt, void *va,
				       size_t size, dma_addr_t dma_addr);

/**
 * Driver API to register a region.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] mapping_id The id for this mapping that is scoped to the queue
 * @param[in] dma_size DMA size of the memory
 * @param[in] dma_addr DMA address of the memory
 *
 * @retval 0 on success
 */
typedef int (*arfw_handle_region_add_t)(void *base_context, void *queue_context,
					uint16_t mapping_id, size_t dma_size,
					dma_addr_t dma_addr);

/**
 * Driver API to unregister a region.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] mapping_id The id for this mapping that is scoped to the queue
 */
typedef void (*arfw_handle_region_del_t)(void *base_context,
					 void *queue_context,
					 uint16_t mapping_id);

/**
 * Driver API to notify until the queue is constructed
 *
 * @param[in] queue_context The hardware queue context.
 */
typedef void (*arfw_handle_notify_queue_ready_t)(void *queue_context);

/**
 * Driver API to mmap kernel data queue to the user space.
 *
 * @param[in] base_context The device context which is used to mmap memory
 * @param[in] queue_data Pointer to the queue data memory structure with the device
 *            specific context
 * @param[in] vma VMA to map in.
 *
 * @retval 0 in case of success, otherwise return error code.
 */
typedef int (*arfw_handle_queue_data_mmap_t)(
	void *base_context, struct arfw_queue_mem_region *queue_data,
	struct vm_area_struct *vma);

/**
 * Notify the client that an external payload needs to be pended to deliver a
 * message.
 *
 * NOTE:
 * Only valid on AR_QUEUE_FW_TO_HLOS queues.
 * Does not acquire queue serial work lock.
 *
 * @param[in] queue Handle for the current queue
 * @param[in] required_size The minimum size the consumer must allocated
 *
 * @retval 0 On success.
 */
typedef int (*arfw_handle_client_payload_pend_required_t)(
	arfw_client_queue_t queue, uint32_t required_size);

/**
 * Notify the client that the pended buffer was received and it's safe to
 * release the buffer.
 *
 * NOTE:
 * Does not acquire queue serial work lock.
 *
 * @param[in] queue Handle for the current queue
 * @param[in] roundtrip_id Packed region_id and pend_id
 *
 * @retval 0 On success.
 */
typedef int (*arfw_handle_client_payload_pend_released_t)(
	arfw_client_queue_t queue, uint32_t roundtrip_id);

/**
 * Driver entry point to reserve a slot in the queue.
 *
 * NOTE:
 * Only valid for queue direction AR_QUEUE_FW_TO_HLOS (RECEIVE)
 *
 * IMPORTANT:
 * Assumes the queue serial work lock is held.
 * If not - construction/destruction lock is assumed.
 *
 * @param[in] queue Handle for the current queue
 * @param[out] req The io request in the queue. (to be filled out and passed
 * to arfw_client_queue_produce_request)
 *
 * @retval 0 On success
 */
typedef int (*arfw_handle_client_queue_reserve_request_t)(
	arfw_client_queue_t queue, struct arfw_io_request *req);

/**
 * Driver entry point to produce data to the queue.
 *
 * NOTE:
 * Only valid for queue direction AR_QUEUE_FW_TO_HLOS (RECEIVE)
 * Acquires & releases the queue serial work lock
 *
 * @param[in] queue Handle for the current queue
 * @param[in] req The io request data structure, previously acquired via
 * arfw_client_queue_reserve_request
 *
 * @retval 0 On success
 */
typedef int (*arfw_handle_client_queue_produce_request_t)(
	arfw_client_queue_t queue, struct arfw_io_request *req);

/**
 * Driver API to trigger shutdown of a client queue.
 * This will only tell the clients we are shutting down.
 *
 * NOTE:
 * Does not acquire queue serial work lock.
 * Marks the the lock as shutting down.
 *
 * @param[in] queue Handle for the current queue
 * @param[in] reason Shutdown reason to communicate to the client
 *
 * @retval 0 On success
 */
typedef int (*arfw_handle_client_queue_shutdown_t)(
	arfw_client_queue_t queue, enum ar_queue_shutdown_reason reason);

/**
 * Driver API to trigger destruction of a client queue. This will result in an
 * async callback to the handle_client_destroy operation.
 *
 * NOTE:
 * Acquires queue lock for destruction.
 *
 * @param[in] queue Handle for the current queue
 */
typedef void (*arfw_handle_client_queue_destroy_t)(arfw_client_queue_t queue);

/**
 * Driver API to grab the client lock to protect operations with the
 * arfw_client_queue_t entries.
 */
typedef void (*arfw_handle_client_lock_t)(void);

/**
 * Driver API to release the client lock.
 */
typedef void (*arfw_handle_client_unlock_t)(void);

/**
 * Driver API to handle aperture allocation.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] size The size of the aperture
 * @param[out] va The kernel virtual address of the aperture buffer. The maximum supported size is
 * passed in, and the actual size is returned.
 * @param[out] dma_addr The DMA address of the aperture buffer
 */
typedef int (*arfw_handle_aperture_alloc_t)(void *base_context,
					    void *queue_context, size_t size,
					    void **va, dma_addr_t *dma_addr);

/**
 * Driver API to handle aperture mmap.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] va The kernel virtual address of the mmap start.
 * @param[in] size The size of the mapping
 * @param[in] vma VMA to map to.
 */
typedef int (*arfw_handle_aperture_mmap_t)(void *base_context,
					   void *queue_context, void *va,
					   size_t size,
					   struct vm_area_struct *vma);

/**
 * Driver API to handle aperture free.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with the queue, null if no queue
 * @param[in] va The kernel virtual address of the aperture
 */
typedef void (*arfw_handle_aperture_free_t)(void *base_context,
					    void *queue_context, void *va,
					    size_t size);

/**
 * Ar Firmware Driver Ops.
 *
 * This structure defines the list of operations the driver
 * must implement and is passed to arfw_device_register.
 */
struct arfw_driver_ops {
	/// Callback to be invoked on send_queue data arrival
	arfw_handle_send_queue_request_t handle_send_queue;

	/// Optional operation when client has consumed data from the queue
	arfw_handle_rcv_queue_consume_t handle_rcv_queue_consume;

	/// allocate queue data region
	arfw_handle_queue_data_alloc_request_t handle_queue_data_alloc;

	/// free queue data region
	arfw_handle_queue_data_free_request_t handle_queue_data_free;

	/// create data ring
	arfw_handle_queue_create_request_t handle_queue_create;

	/// destroy data ring
	arfw_handle_queue_destroy_request_t handle_queue_destroy;

	/// Invoked on device information ioctl
	arfw_handle_get_device_information_t handle_get_device_information;

	/// Invoked on buffer pend ioctl
	arfw_handle_receive_payload_pend_t handle_receive_payload_pend;

	/// Quirk for dma map
	arfw_dma_map_quirk_t handle_dma_map_quirk;

	/// Quirk for dma unmap
	arfw_dma_unmap_quirk_t handle_dma_unmap_quirk;

	/// Add a region to the driver, can be undefined
	arfw_handle_region_add_t handle_region_add;

	/// Remove a region from the driver, can be undefined
	arfw_handle_region_del_t handle_region_del;

	/// Notify the hw layer the queue is ready for work
	arfw_handle_notify_queue_ready_t handle_notify_queue_ready;

	/// Mmap data part of the queue to the user space
	arfw_handle_queue_data_mmap_t handle_queue_data_mmap;

	/// Invoked if an aperture is registered
	arfw_handle_aperture_alloc_t handle_aperture_alloc;

	// Invoked to mmap an aperture to userspace
	arfw_handle_aperture_mmap_t handle_aperture_mmap;

	/// Invoked if an aperture is unregistered
	arfw_handle_aperture_free_t handle_aperture_free;
};

/**
 * Ar Firmware Client Ops.
 *
 * This structure defines the list of operations the client
 * must implement and is passed to the driver on arfw_device_register.
 */
struct arfw_client_ops {
	arfw_handle_client_queue_index_consumed_t
		handle_client_queue_index_consumed;
	arfw_handle_client_payload_pend_required_t
		handle_client_payload_pend_required;
	arfw_handle_client_payload_pend_released_t
		handle_client_payload_pend_released;
	arfw_handle_client_queue_reserve_request_t
		handle_client_queue_reserve_request;
	arfw_handle_client_queue_produce_request_t
		handle_client_queue_produce_request;
	arfw_handle_client_queue_shutdown_t handle_client_queue_shutdown;
	arfw_handle_client_queue_destroy_t handle_client_queue_destroy;
	arfw_handle_client_lock_t handle_client_lock;
	arfw_handle_client_unlock_t handle_client_unlock;
};

/// Maximum length of a device id string
#define ARFW_DEVICE_ID_MAX_LEN 48

#endif // !ARFW_OPS_H
