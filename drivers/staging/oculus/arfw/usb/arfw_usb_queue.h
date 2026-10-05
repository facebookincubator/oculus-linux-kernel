/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_usb_queue.h
 *
 * @brief implementation of the arfw hw queues for USB
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_USB_QUEUE_H
#define ARFW_USB_QUEUE_H

#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/xarray.h>

#include <ar_future.h>
#include <ar_fw_message.h>
#include <arfw_ops.h>

#include "arfw_usb_int.h"
#include "meta_usb_protocol_shared.h"

// This is an expectation on the firmware side.
// Cannot be changed without breaking the protocol.
#define ARFW_USB_QUEUE_HEADER_SIZE 64
#define ARFW_USB_QUEUE_HEADER_PADDING_SIZE \
	(ARFW_USB_QUEUE_HEADER_SIZE - sizeof(ar_firmware_message_header_t))
static_assert(ARFW_USB_QUEUE_HEADER_SIZE >=
	      sizeof(ar_firmware_message_header_t));

// Sanity check that the layouts can fit converted elements.
// We do not need to map it field by field, but need to fit all data into it.
static_assert(sizeof(struct arfw_dma_sg_elem) >= sizeof(meta_usb_data_buf_t));
static_assert(sizeof(ar_firmware_msg_sg_elem_t) >= sizeof(meta_usb_data_buf_t));
static_assert(sizeof(ar_firmware_msg_sg_recv_t) >= sizeof(meta_usb_data_buf_t));

struct arfw_usb_queue_entry {
	ar_firmware_message_header_t arfw_header;
	uint8_t padding[ARFW_USB_QUEUE_HEADER_PADDING_SIZE];
	meta_usb_data_msg_t message;
};

struct arfw_usb_queue_data {
	dma_addr_t dma_addr;
	uint16_t seq_num;
	uint32_t wr_idx;
	uint32_t rd_idx;
};

struct arfw_usb_queue_buffer {
	roundtrip_id_t buf_id;
	size_t buf_size;
	dma_addr_t dma_addr;
	struct list_head list;
};

struct arfw_usb_queue_buffer_xfer {
	bool active;
	size_t queue_index;
	uint8_t buffers_next;
	uint8_t buffers_done;
	uint8_t buffers_total;
	struct list_head list;
};

struct arfw_usb_queue {
	meta_usb_ring_id_t id;
	bool active;
	arfw_client_queue_t client;
	struct arfw_usb_driver *driver;
	struct arfw_client_queue_create_params params;
	struct arfw_queue_mem_region *data;
	struct xarray bufs;
	struct xarray pends;
	spinlock_t bufs_lock;
	struct arfw_usb_queue_buffer_xfer *xfers_all;
	struct list_head xfers_active;
	spinlock_t xfers_lock;
	struct arfw_usb_ep *ep_inl;
	struct arfw_usb_packet_pool *pool_inl;
	struct arfw_usb_ep *ep_ext;
	struct arfw_usb_packet_pool *pool_ext;
};

/**
 * Send request to create a queue.
 * Does all queue init pre-send and dispatches a
 * control packet to create it on the firmware side.
 * Queue itself will be returned as data part of the future.
 *
 * NOTE:
 * This also allocates packet pools.
 *
 * @param[in] driver     Struct that holds subsystem context.
 * @param[in] params     Queue parameters.
 * @param[in] client     Opaque pointer to generic queue.
 * @param[in] queue_data Data segment of the queue.
 * @param[in] timeout_ms Timeout to set on the returned future.
 *
 * @retval    PTR        No error.
 * @retval    PTR_ERR    Otherwise.
 */
ar_future_t *
arfw_usb_queue_create(struct arfw_usb_driver *driver,
		      const struct arfw_client_queue_create_params *params,
		      arfw_client_queue_t client,
		      struct arfw_queue_mem_region *queue_data,
		      unsigned long timeout_ms);

/**
 * Send request to destroy a queue.
 * Clean up all resources in completion.
 *
 * NOTE:
 * This also de-allocates packet pools.
 *
 * @param[in] queue      Struct that holds queue context.
 * @param[in] timeout_ms Timeout to set on the returned future.
 *
 * @retval    PTR        No error.
 * @retval    PTR_ERR    Otherwise.
 */
ar_future_t *arfw_usb_queue_destroy(struct arfw_usb_queue *queue,
				    unsigned long timeout_ms);

/**
 * Reset the queue.
 * This shuts down the queue, deactivates it.
 * The queue gets cleaned up once the client is done with it.
 *
 * NOTE:
 * Sends notification to the client that it was shut down.
 *
 * @param[in] queue  Struct that holds queue context.
 * @param[in] reason Enum with possible reasons.
 *
 * @retval    0      No error.
 * @retval   -E...   Otherwise.
 */
int arfw_usb_queue_reset(struct arfw_usb_queue *queue,
			 enum ar_queue_shutdown_reason reason);

/**
 * Send a slot.
 * Reserves a packet from the queue pool.
 * It does not copy into the buffer, just points at the slot.
 *
 * NOTE:
 * Will fail if room data shows no room in firmware.
 * Even if the host ring buffer has slots.
 *
 * @param[in] queue Struct that holds queue context.
 * @param[in] req   Slot info.
 *
 * @retval    0     No error.
 * @retval   -E...  Otherwise.
 */
int arfw_usb_queue_send(struct arfw_usb_queue *queue,
			const struct arfw_io_request *req);

/**
 * Schedule all IN packets for the given queue's endpoint.
 * Reserves all packets from the endpoint inline pool and schedules them.
 *
 * NOTE:
 * The packets have their own buffer.
 * Completion callbacks will memcpy into queue slots.
 * Host cannot know which queue each packet will belong to.
 *
 * @param[in] queue Struct that holds queue context.
 *
 * @retval    0     No error.
 * @retval   -E...  Otherwise.
 */
int arfw_usb_queue_recv(struct arfw_usb_queue *queue);

/**
 * Mark slots consumed for IN queue.
 *
 * NOTE:
 * This reserves all slots for requests until we cannot.
 *
 * @param[in] queue Struct that holds queue context.
 */
void arfw_usb_queue_recv_consume(struct arfw_usb_queue *queue);

/**
 * Register a buffer with a queue.
 *
 * NOTE:
 * These are buffers used for external messages.
 * All buffers should be page aligned.
 *
 * @param[in] queue      Struct that holds queue context.
 * @param[in] mapping_id ID used for this region (lookup).
 * @param[in] buf_size   Size of the buffer.
 * @param[in] dma_addr   DMA address of the buffer.
 *
 * @retval    0          No error.
 * @retval   -E...       Otherwise.
 */
int arfw_usb_queue_buffer_add(struct arfw_usb_queue *queue, uint16_t mapping_id,
			      size_t buf_size, dma_addr_t dma_addr);

/**
 * Unregister a buffer from a queue.
 *
 * @param[in] queue      Struct that holds queue context.
 * @param[in] mapping_id ID used for this region (lookup).
 */
void arfw_usb_queue_buffer_del(struct arfw_usb_queue *queue,
			       uint16_t mapping_id);

/**
 * Pend a buffer to an IN queue.
 *
 * NOTE:
 * These buffers can be used to DMA buffers into from firmware.
 * We bucket them by size for lookup.
 *
 * @param[in] queue Struct that holds queue context.
 * @param[in] req   Pend buffer info.
 *
 * @retval    0     No error.
 * @retval   -E...  Otherwise.
 */
int arfw_usb_queue_buffer_pend(struct arfw_usb_queue *queue,
			       const struct arfw_payload_pend_req *req);

/**
 * Init all data endpoints and queue bits.
 *
 * NOTE:
 * Verifies and sets up all endpoints needed for queue tx/rx.
 * Allocates pools for packets, stream_ids.
 *
 * @param[in] driver Struct that holds subsystem context.
 *
 * @retval    0      No error.
 * @retval   -E...   Otherwise.
 */
int arfw_usb_queue_init(struct arfw_usb_driver *driver);

/**
 * De-init all data endpoints.
 *
 * NOTE:
 * Also shuts down and destroys all client queues in generic.
 *
 * @param[in] driver Struct that holds subsystem context.
 */
void arfw_usb_queue_exit(struct arfw_usb_driver *driver);

#endif // !ARFW_USB_QUEUE_H
