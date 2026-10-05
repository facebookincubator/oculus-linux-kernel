// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_usb_queue.c
 *
 * @brief implementation of the arfw hw queues for USB
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include "arfw_usb_queue.h"

#include <linux/err.h>
#include <linux/limits.h>
#include <linux/stddef.h>
#include <linux/usb.h>

#include <ar_common.h>
#include <ar_atomics.h>
#include <ar_utils.h>
#include <arfw_log.h>

#include "arfw_usb_ctrl.h"

static unsigned long ep_data_inl_inflight = 5;
module_param(ep_data_inl_inflight, ulong, 0664);
static unsigned long queue_data_ext_inflight = 5;
module_param(queue_data_ext_inflight, ulong, 0664);

static void arfw_usb_queue_send_complete(struct work_struct *work);
static void arfw_usb_queue_send_buffers_complete(struct work_struct *work);

static void arfw_usb_queue_recv_complete(struct work_struct *work);
static void arfw_usb_queue_recv_buffers_complete(struct work_struct *work);

static uint16_t arfw_usb_msg_get_inline_size(const meta_usb_data_msg_t *msg)
{
	AR_ASSERT(msg);
	return msg->num_bufs ? ALIGN(msg->inline_size, sizeof(uint64_t)) :
				     msg->inline_size;
}

static uint16_t arfw_usb_msg_get_data_size(const meta_usb_data_msg_t *msg)
{
	AR_ASSERT(msg);
	return arfw_usb_msg_get_inline_size(msg) +
	       msg->num_bufs * sizeof(meta_usb_data_buf_t);
}

static uint16_t arfw_usb_msg_get_packet_size(const meta_usb_data_msg_t *msg)
{
	AR_ASSERT(msg);
	return sizeof(*msg) + arfw_usb_msg_get_data_size(msg);
}

static meta_usb_data_buf_t *arfw_usb_msg_get_buffer(meta_usb_data_msg_t *msg,
						    size_t slot)
{
	uintptr_t ptr;

	AR_ASSERT(msg);
	AR_ASSERT(msg->num_bufs);
	ptr = (uintptr_t)msg->data;
	ptr += arfw_usb_msg_get_inline_size(msg);
	ptr += sizeof(meta_usb_data_buf_t) * slot;

	return (meta_usb_data_buf_t *)ptr;
}

static struct arfw_usb_queue_entry *
arfw_usb_queue_get_entry(struct arfw_usb_queue *queue, size_t slot)
{
	uintptr_t ptr;

	AR_ASSERT(queue);
	ptr = (uintptr_t)queue->data->ptr;
	ptr += queue->params.element_size * slot;

	return (struct arfw_usb_queue_entry *)ptr;
}

static struct arfw_usb_queue_entry *
arfw_usb_queue_get_entry_for_packet(struct arfw_usb_queue *queue,
				    const struct arfw_usb_packet *packet)
{
	AR_ASSERT(queue);
	AR_ASSERT(packet);
	return arfw_usb_queue_get_entry(queue, packet->queue_index);
}

static struct arfw_usb_queue_entry *
arfw_usb_queue_get_entry_for_write(struct arfw_usb_queue *queue)
{
	AR_ASSERT(queue);
	AR_ASSERT(queue->data);
	AR_ASSERT(queue->data->context);
	return arfw_usb_queue_get_entry(
		queue,
		((struct arfw_usb_queue_data *)queue->data->context)->wr_idx);
}

static uint16_t *arfw_usb_queue_get_room_slot(struct arfw_usb_queue *queue)
{
	uint16_t *room_slot;

	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	room_slot = queue->driver->rings[queue->id.ring_type].room;
	AR_ASSERT(room_slot);
	room_slot += queue->id.ring_slot;

	return room_slot;
}

static void arfw_usb_queue_set_room(struct arfw_usb_queue *queue, uint16_t room)
{
	uint16_t *room_slot;

	// No lock needed: room is a 16-bit naturally aligned scalar.
	// Use store-release to ensure cross-CPU visibility for readers
	// that may check room on a different CPU via load-acquire.
	room_slot = arfw_usb_queue_get_room_slot(queue);
	AR_ASSERT((uintptr_t)room_slot % sizeof(*room_slot) == 0);
	smp_store_release(room_slot, room);
}

static void arfw_usb_queue_sync_room(struct arfw_usb_queue *queue)
{
	struct arfw_usb_queue_data *data;
	uint16_t room;

	AR_ASSERT(queue);
	AR_ASSERT(queue->data);
	data = queue->data->context;
	AR_ASSERT(data);

	// This does not require a lock because we are reading 32bit values.
	// On ARM64 this should produce a LDR single-copy operation.
	// We only need it to be naturally aligned to guarantee atomicity.
	// Check ARMv8 Architecture Reference Manual for "single-copy atomicity".
	AR_ASSERT((uintptr_t)&data->rd_idx % sizeof(data->rd_idx) == 0);
	AR_ASSERT((uintptr_t)&data->wr_idx % sizeof(data->wr_idx) == 0);

	// Calculate available room in the queue and update the slot.
	room = READ_ONCE(data->rd_idx) - READ_ONCE(data->wr_idx) - 1;
	room += queue->params.depth;
	room %= queue->params.depth;

	arfw_usb_queue_set_room(queue, room);
	arfw_usb_ctrl_sync_room_bump(queue->driver);
}

static void arfw_usb_queue_reset_room(struct arfw_usb_queue *queue)
{
	// Unused queues are as good as full. Just zero them out.
	// We will only send room sync for queues with capacity.
	arfw_usb_queue_set_room(queue, /* room = */ 0);
}

static bool arfw_usb_queue_check_room(struct arfw_usb_queue *queue)
{
	return smp_load_acquire(arfw_usb_queue_get_room_slot(queue)) > 0;
}

static bool arfw_usb_queue_can_consume(struct arfw_usb_queue *queue,
				       struct arfw_usb_packet *packet,
				       size_t size, bool drop)
{
	bool can_consume;
	struct urb *urb;

	AR_ASSERT(queue);
	can_consume = READ_ONCE(queue->active);
	AR_ASSERT(packet);
	urb = &packet->urb;
	AR_ASSERT(packet->ep);

	if (!can_consume) {
		if (drop) {
			AR_LOG_USB_QUEUE_DBG(
				packet->ep, queue, AR_LOG_SHUTDOWN,
				"Packet cannot be consumed - queue not active (dropping packet)");
			arfw_usb_packet_put(packet);
		} else {
			AR_LOG_USB_QUEUE_DBG(
				packet->ep, queue, AR_LOG_SHUTDOWN,
				"Packet cannot be consumed - queue not active (resubmitting packet)");
		}
	} else if (urb->actual_length != size) {
		AR_LOG_USB_QUEUE_ERR(
			packet->ep, queue, AR_LOG_SHUTDOWN,
			"Packet cannot be consumed - "
			"not all data processed, less than data message (resetting queue), "
			"actual=%u, expected=%zu, buffer_index=%u, queue_index=%zu",
			urb->actual_length, size, packet->buffer_index,
			packet->queue_index);
		if (drop)
			arfw_usb_packet_put(packet);
		arfw_usb_queue_reset(queue, AR_QUEUE_SHUTDOWN_PROTOCOL_STATE);
		can_consume = false;
	}

	return can_consume;
}

static void arfw_usb_queue_buffer_xfer_start_locked(
	struct arfw_usb_queue *queue, size_t queue_index, uint8_t buffers_total)
{
	unsigned long flags;
	struct arfw_usb_queue_buffer_xfer *xfer;

	AR_ASSERT(queue);
	AR_ASSERT(queue_index < queue->params.depth);
	AR_ASSERT(buffers_total);
	spin_lock_irqsave(&queue->xfers_lock, flags);

	// Lifetime of a xfer: start->next...next->done.
	// The objects are re-used for each transfer of the same slot.
	// Assert transitions were correct.
	xfer = &queue->xfers_all[queue_index];
	AR_ASSERT(!xfer->active);
	AR_ASSERT(!xfer->buffers_next);
	AR_ASSERT(!xfer->buffers_done);
	AR_ASSERT(!xfer->buffers_total);
	xfer->active = true;
	xfer->buffers_total = buffers_total;
	list_add_tail(&xfer->list, &queue->xfers_active);

	spin_unlock_irqrestore(&queue->xfers_lock, flags);
}

static bool arfw_usb_queue_buffer_xfer_next(struct arfw_usb_queue *queue,
					    size_t *queue_index,
					    uint8_t *buffer_index)
{
	struct arfw_usb_queue_buffer_xfer *xfer;

	// This function assumes a lock is held over the xfer list.
	AR_ASSERT(queue);

	// Lifetime of a xfer: start->next...next->done.
	// The objects are re-used for each transfer of the same slot.
	// Assert transitions were correct.
	xfer = list_first_entry_or_null(
		&queue->xfers_active, struct arfw_usb_queue_buffer_xfer, list);

	if (xfer) {
		AR_ASSERT(xfer->active);
		AR_ASSERT(xfer->buffers_next < xfer->buffers_total);
		AR_ASSERT(xfer->buffers_total);
		*queue_index = xfer->queue_index;
		*buffer_index = xfer->buffers_next++;
		if (xfer->buffers_next == xfer->buffers_total)
			list_del(&xfer->list);
	}

	return !!xfer;
}

static void arfw_usb_queue_buffer_xfer_prev(struct arfw_usb_queue *queue,
					    size_t queue_index)
{
	struct arfw_usb_queue_buffer_xfer *xfer;

	// This function assumes a lock is held over the xfer list.
	AR_ASSERT(queue);
	AR_ASSERT(queue_index < queue->params.depth);
	xfer = &queue->xfers_all[queue_index];

	// Move the pointer back and add to the head of the list if needed.
	if (xfer->buffers_next == xfer->buffers_total)
		list_add(&xfer->list, &queue->xfers_active);
	xfer->buffers_next--;
}

static bool arfw_usb_queue_buffer_xfer_done_locked(struct arfw_usb_queue *queue,
						   size_t queue_index)
{
	bool done;
	unsigned long flags;
	struct arfw_usb_queue_buffer_xfer *xfer;

	AR_ASSERT(queue);
	AR_ASSERT(queue_index < queue->params.depth);
	spin_lock_irqsave(&queue->xfers_lock, flags);

	// Lifetime of a xfer: start->next...next->done.
	// The objects are re-used for each transfer of the same slot.
	// Assert transitions were correct.
	xfer = &queue->xfers_all[queue_index];
	AR_ASSERT(xfer->active);
	AR_ASSERT(xfer->buffers_next);
	AR_ASSERT(xfer->buffers_done < xfer->buffers_total);
	AR_ASSERT(xfer->buffers_total);
	done = ++xfer->buffers_done == xfer->buffers_total;

	if (done) {
		xfer->active = false;
		xfer->buffers_next = 0;
		xfer->buffers_done = 0;
		xfer->buffers_total = 0;
	}

	spin_unlock_irqrestore(&queue->xfers_lock, flags);
	return done;
}

typedef int (*arfw_usb_queue_buffer_xfer_func_t)(struct arfw_usb_queue *,
						 struct arfw_usb_packet *);

static int
arfw_usb_queue_drain_buffer_xfers(struct arfw_usb_queue *queue,
				  arfw_usb_queue_buffer_xfer_func_t submit_xfer)
{
	int err = 0;
	unsigned long flags;
	struct arfw_usb_packet *packet;
	size_t queue_index;
	uint8_t buffer_index;

	AR_ASSERT(queue);
	AR_ASSERT(queue->ep_ext);
	spin_lock_irqsave(&queue->xfers_lock, flags);

	while (!err) {
		// If we fail to get a packet we can defer the transfer, it's already queued up.
		packet = arfw_usb_packet_pool_get(queue->pool_ext);
		if (IS_ERR(packet)) {
			AR_LOG_USB_QUEUE_DBG(
				queue->ep_ext, queue, AR_LOG_WRITE,
				"Failed to allocate a buffer packet - "
				"not an issue (xfer deferred), err=%ld",
				PTR_ERR(packet));
			break;
		}

		// Transfers are drained, nothing else to do, we can return the packet.
		if (!arfw_usb_queue_buffer_xfer_next(queue, &queue_index,
						     &buffer_index)) {
			arfw_usb_packet_put(packet);
			break;
		}

		// Point the packet at this particular buffer in the queue.
		packet->queue_index = queue_index;
		packet->buffer_index = buffer_index;

		// We are in atomic context here, so we need to match that in
		// URB submit flags, mark this packet accordingly.
		// If we fail to submit, we just re-try later.
		packet->can_sleep = false;

		// If we failed to submit a transfer we will reset the queue in the callsite.
		// Submit functions do not sleep, they are fast, should only check/mutate some fields.
		err = submit_xfer(queue, packet);
		if (err)
			arfw_usb_packet_put(packet);

		// Submit can fail with -EAGAIN, in which case we need to defer the transfer.
		// We do not count it as an error for the callsites since it can be retried/recovered.
		// Just move the pointer back for the last xfer.
		if (err == -EAGAIN) {
			arfw_usb_queue_buffer_xfer_prev(queue, queue_index);
			err = 0;
			break;
		}
	}

	spin_unlock_irqrestore(&queue->xfers_lock, flags);
	return err;
}

static bool
arfw_usb_queue_check_desc(const struct arfw_usb_queue *queue,
			  const struct usb_endpoint_descriptor *desc)
{
	AR_ASSERT(queue);
	AR_ASSERT(desc);
	return usb_endpoint_xfer_bulk(desc) &&
	       (queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW ?
			      usb_endpoint_dir_out(desc) :
			      usb_endpoint_dir_in(desc));
}

static void arfw_usb_queue_move_idx(const struct arfw_usb_queue *queue,
				    uint32_t *idx)
{
	AR_ASSERT(queue);
	AR_ASSERT(idx);
	*idx = (*idx + 1) % queue->params.depth;
}

static int
arfw_usb_queue_process_create_nack(const struct arfw_usb_queue *queue,
				   const uint8_t flag)
{
	int err;

	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->driver->ep_ctrl_in);

	switch (flag) {
	case META_USB_CREATE_RING_FAIL_DUP:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, ring already exists");
		err = -EEXIST;
		break;
	case META_USB_CREATE_RING_FAIL_INVALID_RING_SLOT:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, invalid ring slot");
		err = -ERANGE;
		break;
	case META_USB_CREATE_RING_FAIL_INVALID_RING_TYPE:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, invalid ring type");
		err = -EINVAL;
		break;
	case META_USB_CREATE_RING_FAIL_INVALID_ITEM_SIZE:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, invalid inline message length");
		err = -E2BIG;
		break;
	case META_USB_CREATE_RING_FAIL_INVALID_EP:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, endpoint unreachable");
		err = -ENXIO;
		break;
	case META_USB_CREATE_RING_FAIL_INVALID_DEPTH:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, depth too small");
		err = -EINVAL;
		break;
	case META_USB_CREATE_RING_FAIL_RING_ID_IN_USE:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, ring id mismatch - slot in use");
		err = -EFAULT;
		break;
	case META_USB_CREATE_RING_FAIL_RING_SHUTDOWN:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, shutdown in progress");
		err = -ESHUTDOWN;
		break;
	case META_USB_CREATE_RING_FAIL_RING_OPENING:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, open request while still opening");
		err = -EALREADY;
		break;
	case META_USB_CREATE_RING_FAIL_INVALID_RING_SIZE:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, invalid ring size");
		err = -EINVAL;
		break;
	case META_USB_CREATE_RING_FAIL_TRANSPORT_NOT_READY:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, transport not ready");
		err = -EAGAIN;
		break;
	default:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, unknown error");
		err = -EIO;
		break;
	}

	return err;
}

static void
arfw_usb_queue_create_reply(struct arfw_usb_ctrl_reply_promise *promise,
			    meta_usb_ctrl_msg_in_t *msg)
{
	int err = -EIO;
	ar_future_t *future;
	struct arfw_usb_queue *queue;
	struct usb_interface *intf;
	meta_usb_create_ring_msg_in_t *resp;
	struct usb_host_interface *intf_host;
	struct usb_endpoint_descriptor *ep_desc;

	AR_ASSERT(promise);
	future = promise->base.future;
	AR_ASSERT(future);
	queue = promise->context;
	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->driver->ep_ctrl_in);
	intf = queue->driver->intf;
	AR_ASSERT(intf);
	intf_host = intf->cur_altsetting;
	AR_ASSERT(msg);
	resp = &msg->create_ring;

	if (resp->common_header.msg_type != META_USB_CREATE_RING_MSG_IN) {
		AR_LOG_USB_QUEUE_ERR(queue->driver->ep_ctrl_in, queue,
				     AR_LOG_CREATE_QUEUE,
				     "Firmware returned bad response type=%u",
				     resp->common_header.msg_type);
		goto error_reply;
	}
	if (resp->common_header.flag != META_USB_FLAG_OK) {
		err = arfw_usb_queue_process_create_nack(
			queue, resp->common_header.flag);
		goto error_reply;
	}

	if (resp->ep_inl >= ARFW_USB_EP_ALL_NUM ||
	    !queue->driver->eps[resp->ep_inl].active ||
	    !arfw_usb_ep_is_inline(&queue->driver->eps[resp->ep_inl]) ||
	    resp->ep_inl == META_USB_EP_CTRL_IN_OFFSET ||
	    resp->ep_inl == META_USB_EP_CTRL_OUT_OFFSET) {
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware returned bad data inline endpoint num=%u, active=%u, inline=%u",
			resp->ep_inl, queue->driver->eps[resp->ep_inl].active,
			arfw_usb_ep_is_inline(
				&queue->driver->eps[resp->ep_inl]));
		goto error_reply;
	}
	queue->ep_inl = &queue->driver->eps[resp->ep_inl];
	ep_desc = &intf_host->endpoint[queue->ep_inl->offset].desc;
	if (!arfw_usb_queue_check_desc(queue, ep_desc)) {
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware returned bad data inline endpoint num=%u, attrs=0x%x, addr=0x%x",
			queue->ep_inl->num, ep_desc->bmAttributes,
			ep_desc->bEndpointAddress);
		goto error_reply;
	}
	if (queue->pool_inl)
		arfw_usb_packet_pool_init(queue->pool_inl, queue->ep_inl);

	if (resp->ep_ext >= ARFW_USB_EP_ALL_NUM ||
	    !queue->driver->eps[resp->ep_ext].active ||
	    resp->ep_ext == META_USB_EP_CTRL_IN_OFFSET ||
	    resp->ep_ext == META_USB_EP_CTRL_OUT_OFFSET) {
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware returned bad data external endpoint num=%u, active=%u",
			resp->ep_ext, queue->driver->eps[resp->ep_ext].active);
		goto error_reply;
	}
	queue->ep_ext = &queue->driver->eps[resp->ep_ext];
	ep_desc = &intf_host->endpoint[queue->ep_ext->offset].desc;
	if (!arfw_usb_queue_check_desc(queue, ep_desc)) {
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
			"Firmware returned bad data external endpoint num=%u, attrs=0x%x, addr=0x%x",
			queue->ep_ext->num, ep_desc->bmAttributes,
			ep_desc->bEndpointAddress);
		goto error_reply;
	}
	arfw_usb_packet_pool_init(queue->pool_ext, queue->ep_ext);

	arfw_usb_queue_sync_room(queue);
	ar_future_complete_data(future, queue);
	return;

error_reply:
	ar_future_complete(future, err);
}

static void arfw_usb_queue_create_complete(int code, void *data, void *context)
{
	struct arfw_usb_ctrl_reply_promise *promise = context;
	struct arfw_usb_queue *queue;
	struct usb_interface *intf;

	(void)data;
	AR_ASSERT(promise);
	queue = promise->context;
	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	intf = queue->driver->intf;
	AR_ASSERT(intf);

	if (!code)
		return;

	if (queue->pool_ext)
		arfw_usb_packet_pool_free(queue->pool_ext);
	if (queue->pool_inl)
		arfw_usb_packet_pool_free(queue->pool_inl);

	xa_erase(&queue->driver->rings[queue->id.ring_type].queues,
		 queue->id.ring_slot);
	devm_kfree(&intf->dev, queue);
}

ar_future_t *
arfw_usb_queue_create(struct arfw_usb_driver *driver,
		      const struct arfw_client_queue_create_params *params,
		      const arfw_client_queue_t client,
		      struct arfw_queue_mem_region *queue_data,
		      const unsigned long timeout_ms)
{
	int err;
	unsigned long flags;
	uint32_t ring_slot;
	size_t xfer_idx;
	ar_future_t *future;
	struct arfw_usb_queue_data *data_context;
	struct usb_interface *intf;
	struct arfw_usb_packet *packet;
	meta_usb_create_ring_msg_out_t *msg;
	struct arfw_usb_ep *ep;
	struct arfw_usb_ctrl_reply_promise *promise;
	struct arfw_usb_queue *queue;
	struct xa_limit ring_limit;

	AR_ASSERT(driver);
	AR_ASSERT(params);
	AR_ASSERT(client);
	AR_ASSERT(queue_data);
	AR_ASSERT(queue_data->context);
	ep = driver->ep_ctrl_out;
	AR_ASSERT(ep);
	intf = driver->intf;
	AR_ASSERT(intf);

	if (params->queue_direction == AR_QUEUE_FW_TO_HLOS &&
	    params->element_size > driver->cp_inline_max) {
		AR_LOG_USB_QUEUE_PARAMS_ERR(
			ep, params, AR_LOG_CREATE_QUEUE,
			"Element size is too big, limit=%u, actual=%u",
			driver->cp_inline_max, params->element_size);
		err = -EINVAL;
		goto error_params;
	}

	future = ar_create_future(&intf->dev);
	if (!future) {
		AR_LOG_USB_QUEUE_PARAMS_ERR(ep, params, AR_LOG_CREATE_QUEUE,
					    "Failed to allocate a future");
		err = -ENOMEM;
		goto error_create_future;
	}

	promise = devm_kzalloc(&intf->dev, sizeof(*promise), GFP_KERNEL);
	if (!promise) {
		AR_LOG_USB_QUEUE_PARAMS_ERR(ep, params, AR_LOG_CREATE_QUEUE,
					    "Failed to allocate a promise");
		err = -ENOMEM;
		goto error_create_promise;
	}
	arfw_usb_ctrl_reply_promise_init(driver, promise, future,
					 arfw_usb_queue_create_reply);

	packet = arfw_usb_ctrl_packet_get(driver, sizeof(*msg));
	if (IS_ERR(packet)) {
		err = PTR_ERR(packet);
		AR_LOG_USB_QUEUE_PARAMS_ERR(
			ep, params, AR_LOG_CTRL,
			"Failed to allocate a packet, err=%d", err);
		goto error_create_packet;
	}

	queue = devm_kzalloc(&intf->dev, sizeof(*queue), GFP_KERNEL);
	if (!queue) {
		AR_LOG_USB_QUEUE_PARAMS_ERR(ep, params, AR_LOG_CREATE_QUEUE,
					    "Failed to allocate a queue");
		err = -ENOMEM;
		goto error_create_queue;
	}

	queue->xfers_all = devm_kcalloc(&intf->dev, params->depth,
					sizeof(*queue->xfers_all), GFP_KERNEL);
	if (!queue->xfers_all) {
		AR_LOG_USB_QUEUE_PARAMS_ERR(ep, params, AR_LOG_CREATE_QUEUE,
					    "Failed to allocate a queue xfers");
		err = -ENOMEM;
		goto error_create_xfers;
	}
	for (xfer_idx = 0; xfer_idx < params->depth; xfer_idx++)
		queue->xfers_all[xfer_idx].queue_index = xfer_idx;

	queue->id.ring_type = params->queue_direction == AR_QUEUE_HLOS_TO_FW ?
					    META_USB_DATA_RING_OUT :
					    META_USB_DATA_RING_IN;
	ring_limit.min = 0;
	ring_limit.max = driver->cp_rings_max / META_USB_RING_TYPES_MAX;

	err = xa_alloc(&driver->rings[queue->id.ring_type].queues, &ring_slot,
		       queue, ring_limit, GFP_KERNEL);
	if (err) {
		AR_LOG_USB_QUEUE_PARAMS_ERR(ep, params, AR_LOG_CREATE_QUEUE,
					    "Failed to allocate a queue id");
		err = -ENOMEM;
		goto error_create_queue_id;
	}

	queue->id.ring_slot = (uint16_t)ring_slot;
	queue->client = client;
	queue->driver = driver;
	queue->params = *params;
	queue->data = queue_data;
	data_context = queue_data->context;
	data_context->rd_idx = 0;
	data_context->wr_idx = 0;
	xa_init_flags(&queue->bufs, XA_FLAGS_ALLOC);
	spin_lock_init(&queue->bufs_lock);
	INIT_LIST_HEAD(&queue->xfers_active);
	spin_lock_init(&queue->xfers_lock);

	msg = packet->buffer;
	msg->common_header.msg_type = META_USB_CREATE_RING_MSG_OUT;
	msg->common_header.flag = META_USB_FLAG_OK;
	msg->common_header.seq_num = promise->seq_num;
	msg->ring_id = queue->id;
	msg->ring_size = params->depth;
	msg->head_room = ARFW_USB_QUEUE_HEADER_SIZE;
	msg->tail_room = 0;
	msg->item_size = params->element_size - msg->head_room - msg->tail_room;
	msg->ap_ep_id = params->hlos_endpoint_id;
	msg->cp_ep_id = params->fw_endpoint_id;

	promise->context = queue;
	ar_future_set_timeout(future, timeout_ms);
	ar_future_set_on_complete(future, arfw_usb_queue_create_complete,
				  promise);

	// This only needed for RECV queues, SEND queues have no pended buffers.
	if (queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS)
		xa_init_flags(&queue->pends, XA_FLAGS_ALLOC);

	// All IN queues use the endpoint pool for inflight packets.
	// We do not know which queue the packets are scoped to when we get them.
	if (queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW) {
		queue->pool_inl = arfw_usb_packet_pool_alloc(
			driver, arfw_usb_queue_send_complete,
			queue->params.depth - 1,
			queue->params.element_size -
				ARFW_USB_QUEUE_HEADER_SIZE);
		if (IS_ERR(queue->pool_inl)) {
			err = PTR_ERR(queue->pool_inl);
			queue->pool_inl = NULL;
			AR_LOG_USB_QUEUE_ERR(
				driver->ep_ctrl_in, queue, AR_LOG_CREATE_QUEUE,
				"Failed to alloc queue inl pool, "
				"pkt_inflight=%u, pkt_size=%u, err=%d",
				queue->params.depth - 1,
				queue->params.element_size -
					ARFW_USB_QUEUE_HEADER_SIZE,
				err);
			goto error_alloc_inl_pool;
		}
	}

	// We always init the external pool since when we schedule packets there we have queue context.
	// Even for IN queues, we already peeked into the packet and can schedule things correctly.
	// No packet size specified since we will point the the external buffers directly.
	queue->pool_ext = arfw_usb_packet_pool_alloc(
		driver,
		queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW ?
			      arfw_usb_queue_send_buffers_complete :
			      arfw_usb_queue_recv_buffers_complete,
		queue_data_ext_inflight, /* pkt_size = */ 0);
	if (IS_ERR(queue->pool_ext)) {
		err = PTR_ERR(queue->pool_ext);
		queue->pool_ext = NULL;
		AR_LOG_USB_QUEUE_ERR(driver->ep_ctrl_in, queue,
				     AR_LOG_CREATE_QUEUE,
				     "Failed to init queue ext pool, "
				     "pkt_inflight=%lu, pkt_size=0, err=%d",
				     queue_data_ext_inflight, err);
		goto error_alloc_ext_pool;
	}

	spin_lock_irqsave(promise->base.lock, flags);
	list_add_tail(&promise->base.list, &driver->ctrl_reply_list);
	spin_unlock_irqrestore(promise->base.lock, flags);

	err = arfw_usb_ctrl_packet_send(packet);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_CREATE_QUEUE,
			"Failed to send a create queue request with err=%d",
			err);
		goto error_send_ctrl_packet;
	}

	AR_LOG_USB_QUEUE_DBG(ep, queue, AR_LOG_CREATE_QUEUE,
			     "Sent a create queue request with id=%u, seq=%u",
			     msg->ring_id.ring_slot,
			     msg->common_header.seq_num);

	return future;

error_send_ctrl_packet:
	spin_lock_irqsave(promise->base.lock, flags);
	list_del(&promise->base.list);
	spin_unlock_irqrestore(promise->base.lock, flags);
	arfw_usb_packet_pool_free(queue->pool_ext);
error_alloc_ext_pool:
	if (queue->pool_inl)
		arfw_usb_packet_pool_free(queue->pool_inl);
error_alloc_inl_pool:
	xa_erase(&driver->rings[queue->id.ring_type].queues,
		 queue->id.ring_slot);
error_create_queue_id:
	devm_kfree(&intf->dev, queue->xfers_all);
error_create_xfers:
	devm_kfree(&intf->dev, queue);
error_create_queue:
	arfw_usb_ctrl_packet_put(packet);
error_create_packet:
	devm_kfree(&intf->dev, promise);
error_create_promise:
	devm_kfree(&intf->dev, future);
error_create_future:
error_params:
	return ERR_PTR(err);
}

static int
arfw_usb_queue_process_destroy_nack(const struct arfw_usb_queue *queue,
				    const uint8_t flag)
{
	int err;

	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->driver->ep_ctrl_in);

	switch (flag) {
	case META_USB_DELETE_RING_FAIL_ACTIVE:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_DESTROY_QUEUE,
			"Firmware nacked destroy queue request, ring is still active");
		err = -EBUSY;
		break;
	case META_USB_DELETE_RING_FAIL_INVALID_RING_ID:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_DESTROY_QUEUE,
			"Firmware nacked destroy queue request, ring does not exist");
		err = -EINVAL;
		break;
	case META_USB_DELETE_RING_FAIL:
		/* fallthrough */
	default:
		AR_LOG_USB_QUEUE_ERR(
			queue->driver->ep_ctrl_in, queue, AR_LOG_DESTROY_QUEUE,
			"Firmware nacked destroy queue request, unknown error");
		err = -EIO;
		break;
	}

	return err;
}

static void
arfw_usb_queue_destroy_reply(struct arfw_usb_ctrl_reply_promise *promise,
			     meta_usb_ctrl_msg_in_t *msg)
{
	int err = 0;
	struct arfw_usb_queue *queue;
	ar_future_t *future;

	AR_ASSERT(promise);
	AR_ASSERT(msg);
	future = promise->base.future;
	AR_ASSERT(future);
	queue = promise->context;
	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->driver->ep_ctrl_in);

	if (msg->common_header.msg_type != META_USB_DELETE_RING_MSG_IN) {
		AR_LOG_USB_QUEUE_ERR(queue->driver->ep_ctrl_in, queue,
				     AR_LOG_DESTROY_QUEUE,
				     "Firmware returned bad response type=%u",
				     msg->common_header.msg_type);
		err = -EIO;
	} else if (msg->common_header.flag != META_USB_FLAG_OK) {
		err = arfw_usb_queue_process_destroy_nack(
			queue, msg->common_header.flag);
	}

	ar_future_complete(future, err);
}

static void arfw_usb_queue_destroy_complete(int code, void *data, void *context)
{
	unsigned long index;
	struct list_head *pends_entry;
	struct arfw_usb_queue_buffer *buf_entry;
	struct arfw_usb_ctrl_reply_promise *promise = context;
	struct arfw_usb_queue *queue;
	struct usb_interface *intf;

	(void)code;
	(void)data;
	AR_ASSERT(promise);
	queue = promise->context;
	AR_ASSERT(queue);
	AR_ASSERT(queue->xfers_all);
	AR_ASSERT(queue->driver);
	intf = queue->driver->intf;
	AR_ASSERT(intf);

	// Should be safe to erase right away, assuming we serialize
	// execution for ctrl and data per queue.
	xa_erase(&queue->driver->rings[queue->id.ring_type].queues,
		 queue->id.ring_slot);

	// When we free pools we will kill all packets and cancel all work.
	// When we exit here nothing should be scheduled for this queue.
	if (queue->pool_ext)
		arfw_usb_packet_pool_free(queue->pool_ext);
	if (queue->pool_inl)
		arfw_usb_packet_pool_free(queue->pool_inl);

	// At this point we cannot get any packets, queue is fully in shutdown mode.
	// Clean up all allocated pend buckets (only on RECV queues, SEND queues have none).
	if (queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS) {
		xa_for_each(&queue->pends, index, pends_entry)
			devm_kfree(&intf->dev, pends_entry);
		xa_destroy(&queue->pends);
	}

	// There are cases when some buffers might still be present when we get here.
	// Even if we wait for all URBs to finish, we only guarantee that DMA finished.
	// The processing itself is offloaded and generic layer might try to call buffer delete later.
	// For those cases just remove all the buffers manually here.
	// When generic comes to us - there will be no queue context anymore and we can avoid delete callback.
	xa_for_each(&queue->bufs, index, buf_entry)
		devm_kfree(&intf->dev, buf_entry);
	xa_destroy(&queue->bufs);

	arfw_usb_queue_reset_room(queue);
	devm_kfree(&intf->dev, queue->xfers_all);
	devm_kfree(&intf->dev, queue);
}

ar_future_t *arfw_usb_queue_destroy(struct arfw_usb_queue *queue,
				    const unsigned long timeout_ms)
{
	int err;
	unsigned long flags;
	ar_future_t *future;
	struct arfw_usb_driver *driver;
	struct usb_interface *intf;
	struct arfw_usb_ep *ep;
	struct arfw_usb_packet *packet;
	meta_usb_delete_ring_msg_out_t *msg;
	struct arfw_usb_ctrl_reply_promise *promise;

	AR_ASSERT(queue);
	driver = queue->driver;
	AR_ASSERT(driver);
	ep = driver->ep_ctrl_out;
	AR_ASSERT(ep);
	intf = driver->intf;
	AR_ASSERT(intf);

	// Mark the queue as inactive right away.
	// From this point on we do not support data flow comms for this queue.
	// All the comms will happen via control endpoint to delete it.
	WRITE_ONCE(queue->active, false);
	atomic_dec(&queue->ep_inl->active_queues);

	future = ar_create_future(&intf->dev);
	if (!future) {
		AR_LOG_USB_QUEUE_ERR(ep, queue, AR_LOG_DESTROY_QUEUE,
				     "Failed to allocate a future");
		err = -ENOMEM;
		goto error_create_future;
	}

	promise = devm_kzalloc(&intf->dev, sizeof(*promise), GFP_KERNEL);
	if (!promise) {
		AR_LOG_USB_QUEUE_ERR(ep, queue, AR_LOG_DESTROY_QUEUE,
				     "Failed to allocate a promise");
		err = -ENOMEM;
		goto error_create_promise;
	}
	arfw_usb_ctrl_reply_promise_init(driver, promise, future,
					 arfw_usb_queue_destroy_reply);

	packet = arfw_usb_ctrl_packet_get(driver, sizeof(*msg));
	if (IS_ERR(packet)) {
		err = PTR_ERR(packet);
		AR_LOG_USB_QUEUE_ERR(ep, queue, AR_LOG_CTRL,
				     "Failed to allocate a packet, err=%d",
				     err);
		goto error_create_packet;
	}

	promise->context = queue;
	ar_future_set_timeout(future, timeout_ms);
	ar_future_set_on_complete(future, arfw_usb_queue_destroy_complete,
				  promise);

	spin_lock_irqsave(promise->base.lock, flags);
	list_add_tail(&promise->base.list, &driver->ctrl_reply_list);
	spin_unlock_irqrestore(promise->base.lock, flags);

	msg = packet->buffer;
	msg->common_header.msg_type = META_USB_DELETE_RING_MSG_OUT;
	msg->common_header.flag = META_USB_FLAG_OK;
	msg->common_header.seq_num = promise->seq_num;
	msg->ring_id = queue->id;

	err = arfw_usb_ctrl_packet_send(packet);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_DESTROY_QUEUE,
			"Failed to send a destroy queue request with err=%d",
			err);
		goto error_send_ctrl_packet;
	}

	AR_LOG_USB_QUEUE_DBG(ep, queue, AR_LOG_DESTROY_QUEUE,
			     "Sent a destroy queue request with id=%u, seq=%u",
			     msg->ring_id.ring_slot,
			     msg->common_header.seq_num);

	return future;

error_send_ctrl_packet:
	spin_lock_irqsave(promise->base.lock, flags);
	list_del(&promise->base.list);
	spin_unlock_irqrestore(promise->base.lock, flags);
	arfw_usb_ctrl_packet_put(packet);
error_create_packet:
	devm_kfree(&intf->dev, promise);
error_create_promise:
	devm_kfree(&intf->dev, future);
error_create_future:
	return ERR_PTR(err);
}

int arfw_usb_queue_reset(struct arfw_usb_queue *queue,
			 enum ar_queue_shutdown_reason reason)
{
	int err;

	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->driver->client_ops);
	AR_ASSERT(queue->driver->client_ops->handle_client_queue_shutdown);

	// Already in shutdown mode, nothing to do. Avoid spamming the client.
	if (!READ_ONCE(queue->active))
		return 0;

	// Mark the queue as inactive, no more packets can be processed.
	// We do not need to go after the inflight packets here, the client will do that.
	// We just need to notify the client to clean up the resources.
	WRITE_ONCE(queue->active, false);

	// Notify the client that the queue is shutting down.
	err = queue->driver->client_ops->handle_client_queue_shutdown(
		queue->client, reason);

	// When the client is already shutting down we treat this as a success.
	// The client will release resources for us later when they process the event.
	return err == -ESHUTDOWN ? 0 : err;
}

static int arfw_usb_queue_send_process(struct arfw_usb_ep *ep,
				       struct arfw_usb_queue *queue,
				       size_t index)
{
	int err = 0;
	struct arfw_usb_driver *driver;
	struct arfw_usb_queue_data *data_context;

	AR_ASSERT(queue);
	AR_ASSERT(queue->data);
	data_context = queue->data->context;
	AR_ASSERT(data_context);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW);
	driver = queue->driver;
	AR_ASSERT(driver);
	AR_ASSERT(driver->client_ops);
	AR_ASSERT(ep);

	if (driver->client_ops->handle_client_queue_index_consumed)
		err = driver->client_ops->handle_client_queue_index_consumed(
			queue->client, index);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_WRITE,
			"Failed to consume a packet with queue_index=%zu, err=%d",
			index, err);
		return err;
	}

	arfw_usb_queue_move_idx(queue, &data_context->rd_idx);
	return 0;
}

static int arfw_usb_queue_send_buffers(struct arfw_usb_queue *queue,
				       struct arfw_usb_packet *packet);

static void arfw_usb_queue_send_buffers_complete(struct work_struct *work)
{
	int err = 0;
	struct arfw_usb_ep *ep;
	struct arfw_usb_queue *queue;
	struct arfw_usb_packet *packet;
	struct urb *urb;
	struct arfw_usb_queue_entry *entry;
	meta_usb_data_msg_t *msg;
	size_t queue_index, msg_size;

	// This takes care of the transport level errors.
	AR_ASSERT(work);
	packet = container_of(work, struct arfw_usb_packet, offload_work.work);
	if (!arfw_usb_packet_can_consume(packet))
		return;

	AR_ASSERT(packet->ep);
	AR_ASSERT(packet->buffer_size);
	AR_ASSERT(!packet->buffer);
	urb = &packet->urb;
	msg_size = urb->transfer_buffer_length;
	queue = urb->context;
	AR_ASSERT(queue);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW);

	// Check if we can consume given packet. Are we active? Is the message malformed?
	if (!arfw_usb_queue_can_consume(queue, packet, msg_size,
					/* drop = */ true))
		return;

	// Do some sanity checking, we should not be sending requests for invalid indexes.
	// Make sure to move buffer index for this packet.
	entry = arfw_usb_queue_get_entry_for_packet(queue, packet);
	msg = &entry->message;
	AR_ASSERT(msg->num_bufs);
	AR_ASSERT(msg->num_bufs > packet->buffer_index);

	// Grab a few things from the packet and release it asap.
	// We do not really need to hold it for the whole duration here.
	ep = packet->ep;
	queue_index = packet->queue_index;
	arfw_usb_packet_put(packet);

	if (arfw_usb_queue_buffer_xfer_done_locked(queue, queue_index)) {
		err = arfw_usb_queue_send_process(ep, queue, queue_index);
		if (err) {
			AR_LOG_USB_QUEUE_ERR(
				ep, queue, AR_LOG_WRITE,
				"Packet buffer send failed - "
				"failed to process index (resetting queue), queue_index=%zu, err=%d",
				queue_index, err);
			goto reset_queue;
		}
	}

	err = arfw_usb_queue_drain_buffer_xfers(queue,
						arfw_usb_queue_send_buffers);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_WRITE,
			"Packet buffer send failed - "
			"failed to submit buffers (resetting queue), queue_index=%zu, err=%d",
			queue_index, err);
		goto reset_queue;
	}

	// Packet is already released, just exit.
	return;

reset_queue:
	arfw_usb_queue_reset(queue, AR_QUEUE_SHUTDOWN_PROTOCOL_STATE);
}

static int arfw_usb_queue_send_buffers(struct arfw_usb_queue *queue,
				       struct arfw_usb_packet *packet)
{
	int err;
	unsigned long flags;
	struct arfw_usb_queue_entry *entry;
	struct arfw_usb_queue_buffer *buf;
	meta_usb_data_msg_t *msg;

	AR_ASSERT(queue);
	AR_ASSERT(queue->data);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW);
	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);

	// Do some sanity checking, we should not be sending requests for invalid indexes.
	// Get all parse out relevant information from the queue slot layout.
	entry = arfw_usb_queue_get_entry_for_packet(queue, packet);
	msg = &entry->message;
	AR_ASSERT(msg->num_bufs);
	AR_ASSERT(msg->num_bufs > packet->buffer_index);

	spin_lock_irqsave(&queue->bufs_lock, flags);

	// This should always come up, otherwise we have an issue somewhere.
	// The memory regions should be refcounted in generic layer.
	// And region ids passed verified by generic, since that's where it gets populated.
	buf = xa_load(
		&queue->bufs,
		arfw_usb_msg_get_buffer(msg, packet->buffer_index)->mem_id);
	AR_ASSERT(buf);

	// We do not have a kernel address for external buffers.
	// It is owned by generic, we could vmap it, but it is not used - so no need.
	packet->buffer = NULL;
	packet->buffer_dma_addr = buf->dma_addr;
	packet->buffer_size =
		arfw_usb_msg_get_buffer(msg, packet->buffer_index)->size;

	// This should probably be checked/ensured in the generic layer.
	// But let's make sure our assumptions are correct.
	AR_ASSERT(packet->buffer_size <= buf->buf_size);

	spin_unlock_irqrestore(&queue->bufs_lock, flags);

	// We do not own the packet in this functions, it will be handled outside.
	err = arfw_usb_packet_send(
		packet, arfw_usb_msg_get_buffer(msg, packet->buffer_index)->sid,
		queue);
	if (err)
		AR_LOG_USB_QUEUE_ERR(
			packet->ep, queue, AR_LOG_WRITE,
			"Failed to send a buffer packet with "
			"queue_index=%zu, buffer_index=%u, num_bufs=%u, err=%d",
			packet->queue_index, packet->buffer_index,
			msg->num_bufs, err);

	return err;
}

static void arfw_usb_queue_send_complete(struct work_struct *work)
{
	int err = 0;
	struct arfw_usb_ep *ep;
	struct arfw_usb_queue *queue;
	struct arfw_usb_packet *packet;
	struct urb *urb;
	meta_usb_data_msg_t *msg;
	size_t queue_index, msg_size;
	uint8_t num_bufs;

	// This takes care of the transport level errors.
	AR_ASSERT(work);
	packet = container_of(work, struct arfw_usb_packet, offload_work.work);
	if (!arfw_usb_packet_can_consume(packet))
		return;

	AR_ASSERT(packet->ep);
	AR_ASSERT(packet->buffer_size);
	msg = packet->buffer;
	AR_ASSERT(msg);
	urb = &packet->urb;
	msg_size = urb->transfer_buffer_length;
	queue = urb->context;
	AR_ASSERT(queue);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW);

	// Check if we can consume given packet. Are we active? Is the message malformed?
	if (!arfw_usb_queue_can_consume(queue, packet, msg_size,
					/* drop = */ true))
		return;

	// Grab a few things from the packet and release it asap.
	// We do not really need to hold it for the whole duration here.
	ep = packet->ep;
	queue_index = packet->queue_index;
	num_bufs = msg->num_bufs;
	arfw_usb_packet_put(packet);

	// For queue messages that have external buffers attached we need to send them separately.
	// The slot is not processed by the client until we send them all.
	// Otherwise we finish processing here and reset the queue if failed to notify the client.
	if (num_bufs) {
		arfw_usb_queue_buffer_xfer_start_locked(queue, queue_index,
							num_bufs);
		err = arfw_usb_queue_drain_buffer_xfers(
			queue, arfw_usb_queue_send_buffers);
		if (err)
			AR_LOG_USB_QUEUE_ERR(
				ep, queue, AR_LOG_WRITE,
				"Packet send failed - "
				"failed to submit buffers (resetting queue), queue_index=%zu, err=%d",
				queue_index, err);
	} else {
		err = arfw_usb_queue_send_process(ep, queue, queue_index);
		if (err)
			AR_LOG_USB_QUEUE_ERR(
				ep, queue, AR_LOG_WRITE,
				"Packet send failed - "
				"failed to process index (resetting queue), queue_index=%zu, err=%d",
				queue_index, err);
	}

	if (err)
		// We expect these steps to succeed, if not - we are in a bad state - reset.
		arfw_usb_queue_reset(queue, AR_QUEUE_SHUTDOWN_PROTOCOL_STATE);
}

static void
arfw_usb_queue_patch_send_buffers(struct arfw_usb_queue *queue,
				  struct arfw_usb_queue_entry *entry)
{
	struct arfw_usb_driver *driver;
	meta_usb_data_msg_t *msg;
	ar_firmware_message_header_t *arfw_header;
	uint8_t buf_idx;
	uint16_t sid;
	size_t buf_list_size;

	AR_ASSERT(queue);
	driver = queue->driver;
	AR_ASSERT(driver);
	AR_ASSERT(entry);
	msg = &entry->message;
	arfw_header = &entry->arfw_header;
	AR_ASSERT(arfw_header->data_location == ARFW_BUFFER_LOC_EXTERNAL);
	buf_list_size = arfw_header->data_size - msg->inline_size;
	AR_ASSERT(buf_list_size <= U8_MAX);
	msg->num_bufs = buf_list_size / sizeof(meta_usb_data_buf_t);

	if (driver->cp_sid_max) {
		// For now we implement a simple flow using only two sid values.
		// Just grab next available for external. Inline is set by firmware.
		sid = (driver->cp_inline_sid + 1) % (driver->cp_sid_max + 1);
		AR_ASSERT(sid != driver->cp_inline_sid);
	} else {
		// No streams enabled, always set zero here.
		sid = 0;
	}

	for (buf_idx = 0; buf_idx < msg->num_bufs; buf_idx++)
		arfw_usb_msg_get_buffer(msg, buf_idx)->sid = sid;
}

int arfw_usb_queue_send(struct arfw_usb_queue *queue,
			const struct arfw_io_request *req)
{
	int err;
	uint32_t next_wr_idx;
	struct arfw_usb_packet *packet;
	struct arfw_usb_queue_entry *entry;
	meta_usb_data_msg_t *msg;
	ar_firmware_message_header_t *arfw_header;
	meta_usb_ipc_header_t *ipc_header;
	meta_usb_common_header_t *common_header;
	struct arfw_usb_queue_data *data_context;

	AR_ASSERT(queue);
	data_context = queue->data->context;
	AR_ASSERT(data_context);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->ep_inl);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_HLOS_TO_FW);
	AR_ASSERT(req);
	entry = req->data;
	AR_ASSERT(entry);
	msg = &entry->message;
	arfw_header = &entry->arfw_header;
	common_header = &msg->common_header;
	ipc_header = &msg->ipc_header;

	// Try to exit early if the queue is in shutdown state.
	if (!READ_ONCE(queue->active))
		return -ESHUTDOWN;

	// We only allow simple inline/external messages, nothing fancy like aperture.
	if (arfw_header->data_location != ARFW_BUFFER_LOC_NONE &&
	    arfw_header->data_location != ARFW_BUFFER_LOC_IN_LINE &&
	    arfw_header->data_location != ARFW_BUFFER_LOC_EXTERNAL) {
		AR_LOG_USB_QUEUE_ERR(queue->ep_inl, queue, AR_LOG_WRITE,
				     "Unexpected header data location value=%u",
				     arfw_header->data_location);
		return -EOPNOTSUPP;
	}

	next_wr_idx = data_context->wr_idx;
	arfw_usb_queue_move_idx(queue, &next_wr_idx);
	if (next_wr_idx == data_context->rd_idx) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_inl, queue, AR_LOG_WRITE,
			"Cannot send more entries, queue is full, rd_idx=%u, wr_idx=%u",
			data_context->rd_idx, data_context->wr_idx);
		return -ENOSPC;
	} else if (!arfw_usb_queue_check_room(queue)) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_inl, queue, AR_LOG_WRITE,
			"Cannot send more entries, firmware has no room, rd_idx=%u, wr_idx=%u",
			data_context->rd_idx, data_context->wr_idx);
		return -ENOSPC;
	}

	ipc_header->msg_id = arfw_header->msg_id;
	ipc_header->ap_ep_id = queue->params.hlos_endpoint_id;
	ipc_header->cp_ep_id = queue->params.fw_endpoint_id;
	ipc_header->track_num = arfw_header->tracking_id;
	ipc_header->seq_num = arfw_header->sequence_id;

	common_header->msg_type = META_USB_DATA_MSG_OUT;
	common_header->flag = META_USB_FLAG_OK;
	common_header->seq_num = data_context->seq_num++;

	msg->ring_id = queue->id;
	msg->inline_size = arfw_header->inline_msg_len;

	// Here we just rewrite the slot entries and nothing more.
	// Actual buffers will be sent once the inline packet completes.
	if (arfw_header->data_location == ARFW_BUFFER_LOC_EXTERNAL)
		arfw_usb_queue_patch_send_buffers(queue, entry);
	else
		msg->num_bufs = 0;

	packet = arfw_usb_packet_pool_get(queue->pool_inl);
	if (IS_ERR(packet)) {
		err = PTR_ERR(packet);
		AR_LOG_USB_QUEUE_ERR(queue->ep_inl, queue, AR_LOG_WRITE,
				     "Failed to allocate a packet, err=%d",
				     err);
		return err;
	}

	packet->buffer = msg;
	packet->buffer_dma_addr = data_context->dma_addr;
	packet->buffer_dma_addr += req->index * req->data_size;
	packet->buffer_dma_addr +=
		offsetof(struct arfw_usb_queue_entry, message);
	packet->buffer_size = arfw_usb_msg_get_packet_size(msg);
	packet->queue_index = req->index;

	err = arfw_usb_packet_send(packet, queue->driver->cp_inline_sid, queue);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_inl, queue, AR_LOG_WRITE,
			"Failed to send packet with err=%d, index=%u, msg_id=0x%x, seq=%u, track=%d",
			err, req->index, arfw_header->msg_id,
			arfw_header->sequence_id, arfw_header->tracking_id);
		arfw_usb_packet_put(packet);
		return err;
	}

	AR_LOG_USB_QUEUE_DBG(
		queue->ep_inl, queue, AR_LOG_WRITE,
		"Processed a packet with index=%u, msg_id=0x%x, seq=%u, track=%d",
		req->index, arfw_header->msg_id, arfw_header->sequence_id,
		arfw_header->tracking_id);

	arfw_usb_queue_move_idx(queue, &data_context->wr_idx);
	return 0;
}

static int arfw_usb_queue_recv_process(struct arfw_usb_ep *ep,
				       struct arfw_usb_queue *queue)
{
	int err = 0;
	struct arfw_usb_driver *driver;
	struct arfw_usb_queue_entry *entry;
	struct arfw_io_request req;
	struct arfw_usb_queue_data *data_context;

	AR_ASSERT(ep);
	driver = ep->driver;
	AR_ASSERT(driver);
	AR_ASSERT(driver->client_ops);

	entry = arfw_usb_queue_get_entry_for_write(queue);
	data_context = queue->data->context;
	req.index = data_context->wr_idx;
	req.data = entry;
	req.data_size = entry->arfw_header.data_size;

	if (driver->client_ops->handle_client_queue_produce_request) {
		err = driver->client_ops->handle_client_queue_produce_request(
			queue->client, &req);
	}
	if (err) {
		AR_LOG_USB_QUEUE_ERR(ep, queue, AR_LOG_READ,
				     "Failed to produce queue_index=%u",
				     req.index);
		return err;
	}

	arfw_usb_queue_move_idx(queue, &data_context->wr_idx);
	AR_LOG_USB_QUEUE_DBG(
		ep, queue, AR_LOG_READ,
		"Processed a packet with type=%u, queue_index=%u, msg_id=0x%x, seq=%u, track=%u",
		entry->message.common_header.msg_type, req.index,
		entry->arfw_header.msg_id, entry->arfw_header.sequence_id,
		entry->arfw_header.tracking_id);

	return 0;
}

static int arfw_usb_queue_recv_buffers(struct arfw_usb_queue *queue,
				       struct arfw_usb_packet *packet);

static void arfw_usb_queue_recv_buffers_complete(struct work_struct *work)
{
	int err = 0;
	struct arfw_usb_ep *ep;
	struct arfw_usb_queue *queue;
	struct arfw_usb_driver *driver;
	struct arfw_usb_packet *packet;
	struct urb *urb;
	struct arfw_usb_queue_entry *entry;
	meta_usb_data_msg_t *msg;
	meta_usb_data_buf_t *buf;
	ar_firmware_msg_sg_recv_t *arfw_elem;
	size_t queue_index, msg_size;

	// This takes care of the transport level errors.
	AR_ASSERT(work);
	packet = container_of(work, struct arfw_usb_packet, offload_work.work);
	if (!arfw_usb_packet_can_consume(packet))
		return;

	ep = packet->ep;
	AR_ASSERT(packet->ep);
	queue_index = packet->queue_index;
	AR_ASSERT(packet->buffer_size);
	AR_ASSERT(!packet->buffer);
	urb = &packet->urb;
	msg_size = urb->transfer_buffer_length;
	queue = urb->context;
	AR_ASSERT(queue);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS);

	// Check if we can consume given packet. Are we active? Is the message malformed?
	if (!arfw_usb_queue_can_consume(queue, packet, msg_size,
					/* drop = */ true))
		return;

	driver = queue->driver;
	AR_ASSERT(driver);
	AR_ASSERT(driver->client_ops);
	AR_ASSERT(driver->client_ops->handle_client_payload_pend_released);

	// Do some sanity checking, we should not be sending requests for invalid indexes.
	// Make sure to move buffer index for this packet.
	entry = arfw_usb_queue_get_entry_for_packet(queue, packet);
	msg = &entry->message;
	AR_ASSERT(msg->num_bufs);
	AR_ASSERT(msg->num_bufs > packet->buffer_index);
	buf = arfw_usb_msg_get_buffer(msg, packet->buffer_index);
	arfw_elem = (ar_firmware_msg_sg_recv_t *)buf;

	// We already got all the things from the packet and can release it asap.
	// We do not really need to hold it for the whole duration here.
	arfw_usb_packet_put(packet);

	err = driver->client_ops->handle_client_payload_pend_released(
		queue->client, arfw_elem->roundtrip.id);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_READ,
			"Packet buffer recv failed - "
			"failed to release pend (resetting queue), "
			"queue_index=%zu, err=%d",
			queue_index, err);
		goto reset_queue;
	}

	if (arfw_usb_queue_buffer_xfer_done_locked(queue, queue_index)) {
		err = arfw_usb_queue_recv_process(ep, queue);
		if (err) {
			AR_LOG_USB_QUEUE_ERR(
				ep, queue, AR_LOG_READ,
				"Packet buffer recv failed - "
				"failed to process index (resetting queue), queue_index=%zu, err=%d",
				queue_index, err);
			goto reset_queue;
		}
	}

	err = arfw_usb_queue_drain_buffer_xfers(queue,
						arfw_usb_queue_recv_buffers);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_READ,
			"Packet buffer recv failed - "
			"failed to submit buffers (resetting queue), queue_index=%zu, err=%d",
			queue_index, err);
		goto reset_queue;
	}

	// Packet is already released, just exit.
	return;

reset_queue:
	arfw_usb_queue_reset(queue, AR_QUEUE_SHUTDOWN_PROTOCOL_STATE);
}

static int arfw_usb_queue_recv_buffers(struct arfw_usb_queue *queue,
				       struct arfw_usb_packet *packet)
{
	int err = 0;
	unsigned long flags, pend_idx;
	struct arfw_usb_driver *driver;
	struct arfw_usb_queue_entry *entry;
	struct arfw_usb_queue_buffer *buf = NULL;
	struct list_head *pends;
	meta_usb_data_msg_t *msg;
	meta_usb_data_buf_t *msg_elem;
	ar_firmware_msg_sg_recv_t *arfw_elem;
	size_t buf_size;
	uint64_t buf_sid;

	AR_ASSERT(queue);
	AR_ASSERT(queue->data);
	AR_ASSERT(queue->client);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS);
	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	driver = queue->driver;
	AR_ASSERT(driver);
	AR_ASSERT(driver->client_ops);
	AR_ASSERT(driver->client_ops->handle_client_payload_pend_required);

	// Do some sanity checking, we should not be sending requests for invalid indexes.
	// Get all parse out relevant information from the queue slot layout.
	entry = arfw_usb_queue_get_entry_for_packet(queue, packet);
	msg = &entry->message;
	AR_ASSERT(msg->num_bufs);
	AR_ASSERT(msg->num_bufs > packet->buffer_index);

	// Extract the fields now since we will overwrite them.
	msg_elem = arfw_usb_msg_get_buffer(msg, packet->buffer_index);
	arfw_elem = (ar_firmware_msg_sg_recv_t *)msg_elem;
	buf_size = msg_elem->size;
	buf_sid = msg_elem->sid;

	spin_lock_irqsave(&queue->bufs_lock, flags);

	// We search for active pended buffers based on the size.
	// All of the buckets for active pends are page sized, search within those.
	// Also if we cannot find it with the size given, check buckets with bigger sizes.
	pend_idx = ALIGN(buf_size, PAGE_SIZE);
	pends = xa_find(&queue->pends, &pend_idx, ULONG_MAX, XA_PRESENT);
	if (pends)
		buf = list_first_entry_or_null(
			pends, struct arfw_usb_queue_buffer, list);
	if (buf)
		list_del(&buf->list);

	// When we cannot find an already pre-pended buffer, we go to the client.
	// This is expensive, but nothing we can do here. Signal the caller to try again.
	if (!buf) {
		err = driver->client_ops->handle_client_payload_pend_required(
			queue->client, buf_size);
		if (err)
			AR_LOG_USB_QUEUE_ERR(
				packet->ep, queue, AR_LOG_READ,
				"Failed to request a pended buffer with "
				"queue_index=%zu, buffer_index=%u, num_bufs=%u, sid=%llu, err=%d",
				packet->queue_index, packet->buffer_index,
				msg->num_bufs, buf_sid, err);
		else
			err = -EAGAIN;
	}

	spin_unlock_irqrestore(&queue->bufs_lock, flags);
	if (err)
		return err;

	// Sanity checking, these should always hold.
	AR_ASSERT(buf->buf_id.pend_id);
	AR_ASSERT(buf->buf_size >= buf_size);

	// We do not have a kernel address for external buffers.
	// It is owned by generic, we could vmap it, but it is not used - so no need.
	packet->buffer = NULL;
	packet->buffer_dma_addr = buf->dma_addr;
	packet->buffer_size = buf_size;

	// Fill out the fields that the client expects for the processed slot.
	// We use the shared layout for the fields and overwrite data that came on the bus.
	arfw_elem->size = buf_size;
	arfw_elem->roundtrip = buf->buf_id;
	buf->buf_id.pend_id = 0;

	// We do not own the packet in this functions, it will be handled outside.
	err = arfw_usb_packet_recv(packet, buf_sid, queue);

	// No need to re-add the buffer to the pends list since if this fails
	// we are already going to be resetting the queue in the callsites, just let it be.
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			packet->ep, queue, AR_LOG_READ,
			"Failed to recv a packet buffer with "
			"queue_index=%zu, buffer_index=%u, num_bufs=%u, sid=%llu, err=%d",
			packet->queue_index, packet->buffer_index,
			msg->num_bufs, buf_sid, err);
		return err;
	}

	AR_LOG_USB_QUEUE_DBG(
		packet->ep, queue, AR_LOG_READ,
		"Scheduled recv into a pended buffer "
		"queue_index=%zu, buffer_index=%u, num_bufs=%u, pend_id=%u, region_id=%u",
		packet->queue_index, packet->buffer_index, msg->num_bufs,
		arfw_elem->roundtrip.pend_id, arfw_elem->roundtrip.region_id);

	return 0;
}

static void arfw_usb_queue_recv_complete(struct work_struct *work)
{
	int err;
	struct arfw_usb_queue *queue;
	struct arfw_usb_driver *driver;
	struct arfw_usb_packet *packet;
	struct arfw_usb_queue_entry *entry;
	struct urb *urb;
	uint16_t ring_type, ring_slot;
	uint32_t next_wr_idx;
	meta_usb_data_msg_t *msg;
	void *msg_dst, *msg_src;
	meta_usb_msg_type_t msg_type;
	uint8_t msg_flag;
	size_t msg_size;
	struct arfw_usb_queue_data *data_context;

	// This takes care of the transport level errors.
	AR_ASSERT(work);
	packet = container_of(work, struct arfw_usb_packet, offload_work.work);
	if (!arfw_usb_packet_can_consume(packet))
		return;

	AR_ASSERT(packet->ep);
	urb = &packet->urb;
	msg = urb->transfer_buffer;
	AR_ASSERT(msg);
	driver = urb->context;
	AR_ASSERT(driver);

	// Full packet size is checked later in this function.
	// For now check if we can even read the message header.
	if (urb->actual_length < sizeof(meta_usb_data_msg_t)) {
		AR_LOG_USB_EP_ERR(
			packet->ep, AR_LOG_READ,
			"Packet recv failed - "
			"not all data processed, less than data message (dropping packet), "
			"actual=%d, expected=%zu, pool_index=%zu",
			urb->actual_length, sizeof(meta_usb_data_msg_t),
			packet->pool_index);
		goto resubmit_packet;
	}

	msg_flag = msg->common_header.flag;
	if (msg_flag != META_USB_FLAG_OK) {
		AR_LOG_USB_EP_ERR(packet->ep, AR_LOG_READ,
				  "Wrong recv queue message flag, "
				  "expected=%u, actual=%u, pool_index=%zu",
				  META_USB_FLAG_OK, msg_flag,
				  packet->pool_index);
		goto resubmit_packet;
	}

	msg_type = msg->common_header.msg_type;
	if (msg_type != META_USB_DATA_MSG_IN) {
		AR_LOG_USB_EP_ERR(packet->ep, AR_LOG_READ,
				  "Wrong recv queue message type, "
				  "expected=%u, actual=%u, pool_index=%zu",
				  META_USB_DATA_MSG_IN, msg_type,
				  packet->pool_index);
		goto resubmit_packet;
	}

	msg_size = arfw_usb_msg_get_packet_size(msg);
	ring_type = msg->ring_id.ring_type;
	ring_slot = msg->ring_id.ring_slot;

	if (ring_type >= META_USB_RING_TYPES_MAX) {
		AR_LOG_USB_EP_ERR(
			packet->ep, AR_LOG_READ,
			"Invalid ring_type=%u, max=%u, pool_index=%zu",
			ring_type, META_USB_RING_TYPES_MAX, packet->pool_index);
		goto resubmit_packet;
	}

	queue = xa_load(&driver->rings[ring_type].queues, ring_slot);
	if (!queue) {
		AR_LOG_USB_EP_ERR(
			packet->ep, AR_LOG_READ,
			"Cannot find a queue with ring_slot=%u, pool_index=%zu",
			ring_slot, packet->pool_index);
		goto resubmit_packet;
	}

	if (queue->params.queue_direction != AR_QUEUE_FW_TO_HLOS) {
		AR_LOG_USB_QUEUE_ERR(
			packet->ep, queue, AR_LOG_READ,
			"Wrong queue found for the message, expected recv queues only "
			"with pool_index=%zu",
			packet->pool_index);
		goto resubmit_packet;
	}

	// Check if we can consume given packet. Are we active? Is the message malformed?
	if (!arfw_usb_queue_can_consume(queue, packet, msg_size,
					/* drop = */ false))
		goto resubmit_packet;

	// Just some sanity checking queue params.
	AR_ASSERT(queue->data);
	data_context = queue->data->context;
	AR_ASSERT(data_context);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->ep_inl == packet->ep);

	next_wr_idx = data_context->wr_idx;
	arfw_usb_queue_move_idx(queue, &next_wr_idx);

	// This should never happen if backpressure is working correctly.
	if (next_wr_idx == data_context->rd_idx) {
		AR_LOG_USB_QUEUE_ERR(
			packet->ep, queue, AR_LOG_READ,
			"Queue is full, but we got a packet, back pressure failed (resetting queue)");
		goto reset_queue;
	}

	entry = arfw_usb_queue_get_entry_for_write(queue);
	entry->arfw_header.msg_id = msg->ipc_header.msg_id;
	entry->arfw_header.sequence_id = msg->ipc_header.seq_num;
	entry->arfw_header.tracking_id = msg->ipc_header.track_num;
	entry->arfw_header.inline_msg_len = msg->inline_size;
	entry->arfw_header.data_size = arfw_usb_msg_get_data_size(msg);
	entry->arfw_header.data_location = ARFW_BUFFER_LOC_IN_LINE;

	// Validate that the message fits within the queue slot.
	// msg_size is derived from firmware-supplied inline_size and num_bufs,
	// if it exceeds the slot capacity the memcpy below would overflow.
	if (msg_size >
	    queue->params.element_size - ARFW_USB_QUEUE_HEADER_SIZE) {
		AR_LOG_USB_QUEUE_ERR(
			packet->ep, queue, AR_LOG_READ,
			"Packet recv failed - "
			"message size exceeds queue slot capacity (resetting queue), "
			"msg_size=%zu, slot_size=%u",
			msg_size,
			queue->params.element_size -
				ARFW_USB_QUEUE_HEADER_SIZE);
		goto reset_queue;
	}

	// Stripping types to avoid memcpy warnings since we are going over
	// the struct size. The struct only covers header and not the data part.
	msg_dst = &entry->message;
	msg_src = msg;
	memcpy(msg_dst, msg_src, msg_size);

	AR_LOG_USB_QUEUE_DBG(
		packet->ep, queue, AR_LOG_READ,
		"Received a packet with type=%u, wr_idx=%u, rd_idx=%u, depth=%u, "
		"msg_id=0x%x, seq=%u, track=%u",
		msg_type, data_context->wr_idx, data_context->rd_idx,
		queue->params.depth, entry->arfw_header.msg_id,
		entry->arfw_header.sequence_id, entry->arfw_header.tracking_id);

	if (msg->num_bufs) {
		entry->arfw_header.data_location = ARFW_BUFFER_LOC_EXTERNAL;
		arfw_usb_queue_buffer_xfer_start_locked(
			queue, data_context->wr_idx, msg->num_bufs);
		err = arfw_usb_queue_drain_buffer_xfers(
			queue, arfw_usb_queue_recv_buffers);
		if (err)
			AR_LOG_USB_QUEUE_ERR(
				packet->ep, queue, AR_LOG_READ,
				"Packet recv failed - "
				"failed to submit buffers (resetting queue), queue_index=%u, err=%d",
				data_context->wr_idx, err);
	} else {
		err = arfw_usb_queue_recv_process(packet->ep, queue);
		if (err)
			AR_LOG_USB_QUEUE_ERR(
				packet->ep, queue, AR_LOG_READ,
				"Packet recv failed - "
				"failed to process index (resetting queue), queue_index=%u, err=%d",
				data_context->wr_idx, err);
	}
	if (err)
		goto reset_queue;

	// After we process each in-packet we need re-submit it.
	// This is done only on in-endpoints since host is polling for data there.
	// For out-endpoints we only send packets on demand from the client.
	goto resubmit_packet;

reset_queue:
	arfw_usb_queue_reset(queue, AR_QUEUE_SHUTDOWN_PROTOCOL_STATE);
resubmit_packet:
	arfw_usb_packet_resubmit(packet);
}

int arfw_usb_queue_recv(struct arfw_usb_queue *queue)
{
	int err = 0;

	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	AR_ASSERT(queue->ep_inl);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS);
	AR_ASSERT(queue->active);

	if (atomic_fetch_inc(&queue->ep_inl->active_queues) == 0)
		err = arfw_usb_packet_recv_all(queue->ep_inl,
					       queue->driver->cp_inline_sid,
					       queue->driver);
	if (err) {
		atomic_dec(&queue->ep_inl->active_queues);
		return err;
	}

	return 0;
}

void arfw_usb_queue_recv_consume(struct arfw_usb_queue *queue)
{
	int err;
	struct arfw_usb_driver *driver;
	bool reserved = false;
	struct arfw_io_request req;
	struct arfw_usb_queue_data *data_context;

	AR_ASSERT(queue);
	AR_ASSERT(queue->data);
	data_context = queue->data->context;
	AR_ASSERT(data_context);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS);
	AR_ASSERT(queue->client);
	driver = queue->driver;
	AR_ASSERT(driver->client_ops);

	// Try to exit early if the queue is in shutdown state.
	if (!READ_ONCE(queue->active))
		return;

	if (!driver->client_ops->handle_client_queue_reserve_request)
		return;

	do {
		err = driver->client_ops->handle_client_queue_reserve_request(
			queue->client, &req);
		reserved = reserved || !err;
	} while (!err);

	if (reserved) {
		data_context->rd_idx = req.index;
		arfw_usb_queue_move_idx(queue, &data_context->rd_idx);
		arfw_usb_queue_sync_room(queue);
	}

	AR_LOG_USB_QUEUE_DBG(
		queue->ep_inl, queue, AR_LOG_READ,
		"Reserved slots updated, rd_idx=%u, wr_idx=%u, depth=%u",
		data_context->rd_idx, data_context->wr_idx,
		queue->params.depth);
}

int arfw_usb_queue_buffer_add(struct arfw_usb_queue *queue, uint16_t mapping_id,
			      size_t buf_size, dma_addr_t dma_addr)
{
	int err;
	unsigned long flags;
	struct usb_interface *intf;
	struct arfw_usb_queue_buffer *buf;
	struct list_head *pends;
	bool pends_added = false;

	AR_ASSERT(queue);
	AR_ASSERT(queue->ep_ext);
	AR_ASSERT(queue->driver);
	intf = queue->driver->intf;
	AR_ASSERT(intf);
	AR_ASSERT(buf_size);
	AR_ASSERT(dma_addr);

	// Try to exit early if the queue is in shutdown state.
	if (!READ_ONCE(queue->active))
		return -ESHUTDOWN;

	buf = devm_kzalloc(&intf->dev, sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		AR_LOG_USB_QUEUE_ERR(queue->ep_ext, queue, AR_LOG_REG_MEM,
				     "Failed to allocate buffer metadata");
		return -ENOMEM;
	}

	buf->buf_id.region_id = mapping_id;
	buf->buf_size = buf_size;
	buf->dma_addr = dma_addr;

	spin_lock_irqsave(&queue->bufs_lock, flags);

	err = xa_insert(&queue->bufs, mapping_id, buf, GFP_ATOMIC);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_REG_MEM,
			"Failed to insert buffer metadata, mapping_id=%u, err=%d",
			mapping_id, err);
		goto error_insert_buf;
	}

	// We check if the active pend bucket list for this size exists.
	// If not, we populate it here instead of pend flow since this is control path.
	pends = xa_load(&queue->pends, buf->buf_size);
	if (!pends) {
		pends = devm_kzalloc(&intf->dev, sizeof(*pends), GFP_ATOMIC);
		err = pends ? err : -ENOMEM;
		pends_added = !err;
	}
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_REG_MEM,
			"Failed to allocate pends for the size bucket, size=%zu, err=%d",
			buf->buf_size, err);
		goto error_alloc_pends;
	}

	// If we managed to allocate it fine we also need to init and insert as a bucket.
	// These will be looked up each time we want to DMA a buffer on RECV path.
	if (pends_added) {
		INIT_LIST_HEAD(pends);
		err = xa_insert(&queue->pends, buf->buf_size, pends,
				GFP_ATOMIC);
	}
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_REG_MEM,
			"Failed to insert pends for the size bucket, size=%zu, err=%d",
			buf->buf_size, err);
		goto error_insert_pends;
	}

	spin_unlock_irqrestore(&queue->bufs_lock, flags);
	AR_LOG_USB_QUEUE_DBG(queue->ep_ext, queue, AR_LOG_REG_MEM,
			     "Inserted buffer metadata, mapping_id=%u",
			     mapping_id);
	return 0;

error_insert_pends:
	devm_kfree(&intf->dev, pends);
error_alloc_pends:
	xa_erase(&queue->bufs, mapping_id);
error_insert_buf:
	spin_unlock_irqrestore(&queue->bufs_lock, flags);
	devm_kfree(&intf->dev, buf);
	return err;
}

void arfw_usb_queue_buffer_del(struct arfw_usb_queue *queue,
			       uint16_t mapping_id)
{
	unsigned long flags;
	struct arfw_usb_queue_buffer *buf;
	struct usb_interface *intf;

	AR_ASSERT(queue);
	AR_ASSERT(queue->ep_ext);
	AR_ASSERT(queue->driver);
	intf = queue->driver->intf;
	AR_ASSERT(intf);

	// Try to exit early if the queue is in shutdown state.
	if (!READ_ONCE(queue->active))
		return;

	spin_lock_irqsave(&queue->bufs_lock, flags);
	buf = xa_erase(&queue->bufs, mapping_id);
	if (!buf) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_REG_MEM,
			"Failed to erase buffer metadata - not found, mapping_id=%u",
			mapping_id);
		goto unlock_bufs;
	}

	// Delete from the list of active pends first.
	// Keep the bucket itself, we clean those up when the queue is deleted.
	// If we get incoming packets and pends are gone we will error out on client ops.
	// Since the queue is in shutdown state.
	if (buf->buf_id.pend_id)
		list_del(&buf->list);

	devm_kfree(&intf->dev, buf);
	AR_LOG_USB_QUEUE_DBG(queue->ep_ext, queue, AR_LOG_REG_MEM,
			     "Erased buffer metadata, mapping_id=%u",
			     mapping_id);
unlock_bufs:
	spin_unlock_irqrestore(&queue->bufs_lock, flags);
}

int arfw_usb_queue_buffer_pend(struct arfw_usb_queue *queue,
			       const struct arfw_payload_pend_req *req)
{
	int err = 0;
	unsigned long flags;
	struct arfw_usb_queue_buffer *buf;
	size_t sg_size = 0, sg_item;
	struct list_head *pends;

	AR_ASSERT(queue);
	AR_ASSERT(queue->ep_ext);
	AR_ASSERT(queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS);
	AR_ASSERT(queue->driver);
	AR_ASSERT(req);
	AR_ASSERT(req->roundtrip.pend_id);
	AR_ASSERT(req->sg_list_len);

	// Try to exit early if the queue is in shutdown state.
	if (!READ_ONCE(queue->active))
		return -ESHUTDOWN;

	spin_lock_irqsave(&queue->bufs_lock, flags);

	// Load the buffer by region id only, discard the pend id part.
	// Pend id is used to communicate this particular pend back to the client.
	buf = xa_load(&queue->bufs, req->roundtrip.region_id);
	if (!buf) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_PEND,
			"Failed to pend a buffer, cannot load by region_id=%u",
			req->roundtrip.region_id);
		err = -EINVAL;
		goto unlock_bufs;
	}
	if (buf->buf_id.pend_id) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_PEND,
			"Failed to pend a buffer with pend_id=%u, already pended with pend_id=%u, region_id=%u",
			req->roundtrip.pend_id, buf->buf_id.pend_id,
			buf->buf_id.region_id);
		err = -EEXIST;
		goto unlock_bufs;
	}

	// All buffers should be page aligned in overall size, sanity check.
	AR_ASSERT(buf->buf_size % PAGE_SIZE == 0);
	AR_ASSERT(buf->buf_id.region_id == req->roundtrip.region_id);

	// Do some sanity checks, these might not be needed, but they are cheap, so why not.
	// Just verify that the sizes and addresses match. It's better than IOMMU faults.
	for (sg_item = 0; sg_item < req->sg_list_len; sg_item++)
		sg_size += req->sg_list[sg_item].len;

	if (sg_size != buf->buf_size) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_PEND,
			"Failed to pend a buffer, sizes do not match, actual=0x%zx, expected=0x%zx",
			sg_size, buf->buf_size);
		err = -EINVAL;
		goto unlock_bufs;
	}

	if (req->sg_list[0].addr != buf->dma_addr) {
		AR_LOG_USB_QUEUE_ERR(
			queue->ep_ext, queue, AR_LOG_PEND,
			"Failed to pend a buffer, addresses do not match, actual=0x%llx, expected=0x%llx",
			req->sg_list[0].addr, buf->dma_addr);
		err = -EINVAL;
		goto unlock_bufs;
	}

	// All pend buckets for buffer sizes should be added on control path.
	// When we register a buffer we also allocate these, so they have to exist.
	// Sanity check here, but we guarantee it by having buffer lookup.
	pends = xa_load(&queue->pends, buf->buf_size);
	AR_ASSERT(pends);

	// Refresh the pend_id on the fetched buffer and add to pends.
	// Ordering does not matter - all have the same size in this bucket.
	buf->buf_id.pend_id = req->roundtrip.pend_id;
	list_add_tail(&buf->list, pends);

	AR_LOG_USB_QUEUE_DBG(
		queue->ep_ext, queue, AR_LOG_PEND,
		"Pended a buffer pend_id=%u, region_id=%u, addr=0x%llx, dma_addr=0x%llx",
		req->roundtrip.pend_id, req->roundtrip.region_id,
		req->sg_list[0].addr, buf->dma_addr);

unlock_bufs:
	spin_unlock_irqrestore(&queue->bufs_lock, flags);
	if (!err)
		// In case xfers were blocked on a pending buffer we need to drain them.
		err = arfw_usb_queue_drain_buffer_xfers(
			queue, arfw_usb_queue_recv_buffers);

	return err;
}

int arfw_usb_queue_init(struct arfw_usb_driver *driver)
{
	int err = 0;
	size_t ep_offset;
	uint16_t ring_type;
	struct arfw_usb_ep *ep;
	struct usb_host_interface *intf;
	struct usb_host_endpoint *ep_ptr[1];
	struct usb_endpoint_descriptor *ep_desc;

	AR_ASSERT(driver);
	AR_ASSERT(driver->offload_queue);
	AR_ASSERT(driver->intf);
	intf = driver->intf->cur_altsetting;
	AR_ASSERT(intf);

	// Not enough endpoints on this interface to set up data path.
	if (intf->desc.bNumEndpoints <= ARFW_USB_EP_CTRL_NUM) {
		err = -EINVAL;
		goto error_endpoint_num;
	}

	for (ring_type = 0; ring_type < META_USB_RING_TYPES_MAX; ring_type++) {
		// Ring slots are allocated per ring type, so we need to have
		// separate xarray storages for each queue type.
		xa_init_flags(&driver->rings[ring_type].queues, XA_FLAGS_ALLOC);
	}

	for (ep_offset = 0; ep_offset < intf->desc.bNumEndpoints; ep_offset++) {
		// Skip control endpoints, only check data.
		// Be careful, since we need to use static offsets.
		if (ep_offset == META_USB_EP_CTRL_IN_OFFSET)
			continue;
		if (ep_offset == META_USB_EP_CTRL_OUT_OFFSET)
			continue;

		ep_desc = &intf->endpoint[ep_offset].desc;
		ep = &driver->eps[ep_offset];
		ep->driver = driver;
		ep->num = usb_endpoint_num(ep_desc);
		ep->offset = ep_offset;

		// We only expect bulk endpoints in the current protocol version.
		// This might change in the future. But who am I to predict it?
		if (!usb_endpoint_xfer_bulk(ep_desc)) {
			err = -EINVAL;
			goto error_endpoint_init;
		}

		// Some interfaces/subsystems might not have support for streams (old spec).
		// Avoid allocating resources for them if firmware indicates no usage.
		if (driver->cp_sid_max) {
			ep_ptr[0] = &intf->endpoint[ep->offset];
			err = usb_alloc_streams(driver->intf, ep_ptr,
						/* num_eps = */ 1,
						driver->cp_sid_max, GFP_KERNEL);
		}
		if (err)
			goto error_endpoint_init;

		// This will set up generic endpoint fields (packets, counters).
		// Also marks the endpoint as active - means initialized successfully.
		err = arfw_usb_ep_init(ep);
		// We need this extra stream release here since the endpoint will not be marked
		// as active if we failed to init it and we use active status to tear them down.
		if (err && driver->cp_sid_max)
			usb_free_streams(driver->intf, ep_ptr,
					 /* num_eps = */ 1, driver->cp_sid_max);
		if (err)
			goto error_endpoint_init;

		// For incoming inline messages we pre-allocate buffers to park data.
		// We eat a memcpy here and need to peek into the data before putting
		// it  into the right queue slot.
		// Other types of endpoints will use pool scoped to the queue.
		if (usb_endpoint_dir_out(ep_desc) || !arfw_usb_ep_is_inline(ep))
			continue;

		ep->pool = arfw_usb_packet_pool_alloc(
			driver, arfw_usb_queue_recv_complete,
			ep_data_inl_inflight, driver->cp_inline_max);
		if (IS_ERR(ep->pool)) {
			err = PTR_ERR(ep->pool);
			goto error_endpoint_init;
		}
		arfw_usb_packet_pool_init(ep->pool, ep);
	}

	return 0;

error_endpoint_init:
	arfw_usb_queue_exit(driver);
error_endpoint_num:
	return err;
}

void arfw_usb_queue_exit(struct arfw_usb_driver *driver)
{
	size_t ep_offset;
	uint16_t ring_type;
	struct arfw_usb_ep *ep;
	struct usb_host_interface *intf;
	struct usb_host_endpoint *ep_ptr[1];
	struct usb_endpoint_descriptor *ep_desc;
	unsigned long queue_idx;
	struct arfw_usb_queue *queue;

	AR_ASSERT(driver);
	AR_ASSERT(driver->client_ops);
	AR_ASSERT(driver->client_ops->handle_client_lock);
	AR_ASSERT(driver->client_ops->handle_client_unlock);
	AR_ASSERT(driver->client_ops->handle_client_queue_shutdown);
	AR_ASSERT(driver->client_ops->handle_client_queue_destroy);
	AR_ASSERT(driver->intf);
	intf = driver->intf->cur_altsetting;
	AR_ASSERT(intf);

	driver->client_ops->handle_client_lock();
	for (ring_type = 0; ring_type < META_USB_RING_TYPES_MAX; ring_type++) {
		xa_for_each(&driver->rings[ring_type].queues, queue_idx,
			    queue) {
			AR_ASSERT(queue);
			AR_ASSERT(queue->client);
			driver->client_ops->handle_client_queue_shutdown(
				queue->client, AR_QUEUE_SHUTDOWN_DRIVER_STATE);
			driver->client_ops->handle_client_queue_destroy(
				queue->client);
		}
	}
	driver->client_ops->handle_client_unlock();

	for (ep_offset = 0; ep_offset < intf->desc.bNumEndpoints; ep_offset++) {
		// Skip control endpoints, only check data.
		// Be careful, since we need to use static offsets.
		if (ep_offset == META_USB_EP_CTRL_IN_OFFSET)
			continue;
		if (ep_offset == META_USB_EP_CTRL_OUT_OFFSET)
			continue;

		ep_desc = &intf->endpoint[ep_offset].desc;
		ep = &driver->eps[ep_offset];

		// Skip un-initialized endpoints. No cleanup to do for those.
		// We reserve space statically for max spec endpoints, but each interface
		// could put specific endpoint numbers into different drivers, so a lot of
		// them will be inactive.
		if (!ep->active)
			continue;

		// This should drain the endpoint first.
		// Only after this we can release streams.
		// Pools will be released by the endpoint exit if set.
		arfw_usb_ep_exit(ep);

		// Only attempt to clean it up if firmware asked us to allocate it.
		// Endpoints are marked active only if all the resources were allocated.
		if (driver->cp_sid_max) {
			ep_ptr[0] = &intf->endpoint[ep->offset];
			usb_free_streams(driver->intf, ep_ptr,
					 /* num_eps = */ 1, driver->cp_sid_max);
		}
	}

	for (ring_type = 0; ring_type < META_USB_RING_TYPES_MAX; ring_type++)
		xa_destroy(&driver->rings[ring_type].queues);
}
