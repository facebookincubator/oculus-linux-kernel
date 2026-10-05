// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_usb_int.c
 *
 * @brief internal symbols and functions used for USB protocol implementation
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include "arfw_usb_int.h"

#include <linux/bitops.h>
#include <linux/err.h>

#include <ar_common.h>
#include <arfw_log.h>

static bool arfw_usb_packet_can_schedule(struct arfw_usb_packet *packet)
{
	int err;
	struct urb *urb;

	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	urb = &packet->urb;
	err = urb->status;

	// Successful packet, can schedule.
	if (!err)
		return true;

	// Firmware is not responding, these are critical errors.
	// But we need to schedule them to recover the device/queue state.
	if (err == -EPIPE || err == -ETIMEDOUT)
		return true;

	AR_LOG_USB_EP_ERR(
		packet->ep, AR_LOG_CTRL,
		"Packet cannot be scheduled (dropping packet), err=%d", err);

	// Other errors may indicate the request was cancelled or
	// device got disconnected. We stop here and do not recover.
	// We will not enter thread context with these packets and just drop them.
	arfw_usb_packet_put(packet);

	return false;
}

bool arfw_usb_packet_can_consume(struct arfw_usb_packet *packet)
{
	int err;
	struct arfw_usb_driver *driver;

	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	driver = packet->ep->driver;
	AR_ASSERT(driver);
	err = packet->urb.status;

	// Successful packet, safe to consume at transport level.
	// We can still fail to consume the packet, but based on the data transfered.
	// So it will be a protocol level failure, in that case it depends on the context.
	// We could shut down the queue/endpoint/subsystem in the callsites.
	if (!err)
		return true;

	AR_LOG_USB_EP_ERR(
		packet->ep, AR_LOG_CTRL,
		"Packet cannot be consumed (resetting subsystem), err=%d", err);

	// Return packet first, we will be resetting the whole subsystem next.
	arfw_usb_packet_put(packet);

	// Sanity check the errors we allow here.
	// The recovery is done in the thread context callsite.
	AR_ASSERT(err == -EPIPE || err == -ETIMEDOUT);

	// For STALL states we also need to kick the firmware, so specify that here.
	// Try to recover by resetting the whole subsystem.
	arfw_usb_sys_reset(driver);

	return false;
}

static void arfw_usb_packet_schedule_complete_irq(struct urb *urb)
{
	struct arfw_usb_packet *packet;

	AR_ASSERT(urb);
	packet = container_of(urb, struct arfw_usb_packet, urb);
	AR_ASSERT(packet->offload_queue);

	// This flag should be reset after each completion.
	// It is only used in URB submit flow and needs to be flipped after sending.
	packet->can_sleep = true;

	// Only successful or recoverable packets are allowed.
	// It does not mean we can re-submit them, but we can recover the system.
	if (!arfw_usb_packet_can_schedule(packet))
		return;

	// Schedule packet offload immediately to bypass the timer tick.
	// This way it should be safe to drain workqueue instead of each work item.
	queue_delayed_work(packet->offload_queue, &packet->offload_work, 0);
}

struct arfw_usb_packet_pool *
arfw_usb_packet_pool_alloc(struct arfw_usb_driver *driver, work_func_t complete,
			   size_t pool_size, size_t buffer_size)
{
	int err;
	struct arfw_usb_packet_pool *pool;
	size_t idx;
	struct usb_device *dev;
	struct arfw_usb_packet *packet;
	uint8_t *buffer;
	size_t buffer_offset;

	AR_ASSERT(complete);
	AR_ASSERT(pool_size);
	AR_ASSERT(driver);
	AR_ASSERT(driver->intf);
	dev = interface_to_usbdev(driver->intf);
	AR_ASSERT(dev);

	if (pool_size >= ARFW_USB_PACKET_POOL_MAX_SIZE) {
		err = -EINVAL;
		goto error_pool_size;
	}

	pool = devm_kzalloc(&dev->dev, sizeof(*pool), GFP_KERNEL);
	if (!pool) {
		err = -ENOMEM;
		goto error_pool_alloc;
	}

	pool->packets = devm_kcalloc(&dev->dev, pool_size,
				     sizeof(*pool->packets), GFP_KERNEL);
	if (!pool->packets) {
		err = -ENOMEM;
		goto error_packets_alloc;
	}

	if (buffer_size) {
		pool->buffers = usb_alloc_coherent(dev, pool_size * buffer_size,
						   GFP_KERNEL,
						   &pool->buffers_dma_addr);
		if (!pool->buffers) {
			err = -ENOMEM;
			goto error_buffers_alloc;
		}
	}

	for (idx = 0; idx < pool_size; idx++) {
		packet = &pool->packets[idx];
		packet->pool = pool;
		packet->pool_index = idx;
		packet->buffer_owner = false;
		usb_init_urb(&packet->urb);
		packet->urb.transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		packet->urb.context = driver;
		if (pool->buffers) {
			buffer_offset = idx * buffer_size;
			packet->buffer_size = buffer_size;
			packet->buffer_dma_addr = pool->buffers_dma_addr;
			packet->buffer_dma_addr += buffer_offset;
			buffer = pool->buffers;
			buffer += buffer_offset;
			packet->buffer = buffer;
		}
		INIT_DELAYED_WORK(&packet->offload_work, complete);
	}

	pool->driver = driver;
	pool->pool_size = pool_size;
	pool->buffer_size = buffer_size;

	// Mark all allocated packets as inactive.
	atomic64_set(&pool->inactive, (1ULL << pool_size) - 1);
	return pool;

error_buffers_alloc:
	devm_kfree(&dev->dev, pool->packets);
error_packets_alloc:
	devm_kfree(&dev->dev, pool);
error_pool_alloc:
error_pool_size:
	return ERR_PTR(err);
}

void arfw_usb_packet_pool_init(struct arfw_usb_packet_pool *pool,
			       struct arfw_usb_ep *ep)
{
	size_t idx;
	struct arfw_usb_packet *packet;

	AR_ASSERT(pool);
	AR_ASSERT(ep);
	AR_ASSERT(ep->offload_queue);

	pool->ep = ep;
	for (idx = 0; idx < pool->pool_size; idx++) {
		packet = &pool->packets[idx];
		packet->ep = ep;
		packet->offload_queue = ep->offload_queue;
	}
}

void arfw_usb_packet_pool_free(struct arfw_usb_packet_pool *pool)
{
	size_t idx;
	struct usb_device *dev;

	AR_ASSERT(pool);
	AR_ASSERT(pool->driver);
	AR_ASSERT(pool->driver->intf);
	dev = interface_to_usbdev(pool->driver->intf);
	AR_ASSERT(dev);

	// Prevent new packets from being reserved.
	pool->released = true;

	// Wait for all completion callbacks to finish.
	for (idx = 0; idx < pool->pool_size; idx++) {
		usb_kill_urb(&pool->packets[idx].urb);
		cancel_delayed_work_sync(&pool->packets[idx].offload_work);
	}

	if (pool->buffers) {
		usb_free_coherent(dev, pool->pool_size * pool->buffer_size,
				  pool->buffers, pool->buffers_dma_addr);
		pool->buffers = NULL;
	}

	if (pool->packets) {
		devm_kfree(&dev->dev, pool->packets);
		pool->packets = NULL;
	}

	pool->pool_size = 0;
	pool->buffer_size = 0;

	devm_kfree(&dev->dev, pool);
}

struct arfw_usb_packet *
arfw_usb_packet_pool_get(struct arfw_usb_packet_pool *pool)
{
	struct arfw_usb_packet *packet;
	uint64_t old_inactive, new_inactive, selected;

	AR_ASSERT(pool);
	AR_ASSERT(pool->packets);

	if (pool->released)
		return ERR_PTR(-ESHUTDOWN);

	do {
		old_inactive = atomic64_read(&pool->inactive);
		if (!old_inactive)
			return ERR_PTR(-ENOSPC);

		selected = __ffs64(old_inactive);
		new_inactive = old_inactive ^ (1ULL << selected);
	} while (atomic64_cmpxchg(&pool->inactive, old_inactive,
				  new_inactive) != old_inactive);

	packet = &pool->packets[selected];
	packet->can_sleep = true;

	return packet;
}

struct arfw_usb_packet *arfw_usb_packet_get(struct arfw_usb_ep *ep,
					    work_func_t complete, size_t size,
					    struct usb_anchor *anchor)
{
	int err;
	struct arfw_usb_packet *packet;
	struct usb_device *dev;

	AR_ASSERT(ep);
	AR_ASSERT(ep->driver);
	AR_ASSERT(ep->driver->intf);
	dev = interface_to_usbdev(ep->driver->intf);
	AR_ASSERT(dev);
	AR_ASSERT(complete);
	AR_ASSERT(anchor);

	packet = devm_kzalloc(&dev->dev, sizeof(*packet), GFP_KERNEL);
	if (!packet) {
		err = -ENOMEM;
		goto error_packet_alloc;
	}

	if (size) {
		packet->buffer = usb_alloc_coherent(dev, size, GFP_KERNEL,
						    &packet->buffer_dma_addr);
		if (!packet->buffer) {
			err = -ENOMEM;
			goto error_buffer_alloc;
		}
	}

	packet->ep = ep;
	packet->urb_anchor = anchor;
	packet->buffer_size = size;
	packet->buffer_owner = size > 0;
	packet->can_sleep = true;
	usb_init_urb(&packet->urb);
	packet->urb.context = ep->driver;
	packet->urb.transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	packet->offload_queue = ep->offload_queue;
	INIT_DELAYED_WORK(&packet->offload_work, complete);

	return packet;

error_buffer_alloc:
	devm_kfree(&dev->dev, packet);
error_packet_alloc:
	return ERR_PTR(err);
}

void arfw_usb_packet_put(struct arfw_usb_packet *packet)
{
	struct usb_device *dev;

	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	AR_ASSERT(packet->ep->driver);
	AR_ASSERT(packet->ep->driver->intf);
	dev = interface_to_usbdev(packet->ep->driver->intf);
	AR_ASSERT(dev);

	packet->queue_index = 0;
	packet->buffer_index = 0;
	packet->urb_anchor = NULL;
	packet->can_sleep = true;

	// Only free the buffers if we own them and not from a pool.
	// Client owned buffers will be released by the client.
	// Pool will release all the buffers at once in the cleanup.
	if (packet->buffer_owner)
		usb_free_coherent(dev, packet->buffer_size, packet->buffer,
				  packet->buffer_dma_addr);

	// For packets from a pool that had no buffers we need to reset the buffer.
	// Clients will put them ad-hoc and we need to clear them after each return.
	// If packets use buffers from the pool - just keep them intact.
	if (packet->pool && !packet->pool->buffers) {
		packet->buffer = NULL;
		packet->buffer_dma_addr = 0;
		packet->buffer_size = 0;
	}

	// If the packet is from a pool, mark it for re-use. Release otherwise.
	if (packet->pool)
		atomic64_or(1ULL << packet->pool_index,
			    &packet->pool->inactive);
	else
		devm_kfree(&dev->dev, packet);
}

static int arfw_usb_packet_submit(struct arfw_usb_packet *packet, uint16_t sid,
				  void *context, int pipe)
{
	int err;
	struct arfw_usb_ep *ep;
	struct arfw_usb_driver *driver;
	struct usb_interface *intf;
	struct usb_device *dev;

	AR_ASSERT(packet);
	AR_ASSERT(packet->buffer_dma_addr);
	AR_ASSERT(packet->buffer_size);
	ep = packet->ep;
	AR_ASSERT(ep);
	driver = ep->driver;
	AR_ASSERT(driver);
	intf = driver->intf;
	AR_ASSERT(intf);
	dev = interface_to_usbdev(intf);
	AR_ASSERT(dev);

	if (packet->urb_anchor)
		usb_anchor_urb(&packet->urb, packet->urb_anchor);
	packet->urb.transfer_dma = packet->buffer_dma_addr;
	usb_fill_bulk_urb(&packet->urb, dev, pipe, packet->buffer,
			  packet->buffer_size,
			  arfw_usb_packet_schedule_complete_irq, context);

	packet->urb.stream_id = sid;
	err = usb_submit_urb(&packet->urb,
			     packet->can_sleep ? GFP_KERNEL : GFP_ATOMIC);
	if (err) {
		usb_unanchor_urb(&packet->urb);
		AR_LOG_USB_EP_ERR(
			ep, AR_LOG_CTRL,
			"Failed to schedule a packet, pool_index=%zu, err=%d",
			packet->pool_index, err);
		return err;
	}

	AR_LOG_USB_EP_DBG(ep, AR_LOG_CTRL,
			  "Scheduled a packet, pool_index=%zu, buffer_size=%zu",
			  packet->pool_index, packet->buffer_size);

	return 0;
}

void arfw_usb_packet_resubmit(struct arfw_usb_packet *packet)
{
	int err;

	AR_ASSERT(packet);

	err = arfw_usb_packet_submit(packet, packet->urb.stream_id,
				     packet->urb.context, packet->urb.pipe);

	// Instead of leaking the packet let's reset the system,
	// if this happens something is not right big time.
	// Only bypass reset if it's not a fault type of situation (disconnect/teardown).
	if (err) {
		AR_LOG_USB_EP_ERR(
			packet->ep, AR_LOG_CTRL,
			"Failed to resubmit a packet, pool_index=%zu, err=%d",
			packet->pool_index, err);
		arfw_usb_packet_put(packet);
		arfw_usb_sys_reset_on_fault(packet->ep->driver, err);
	}
}

int arfw_usb_packet_send(struct arfw_usb_packet *packet, uint16_t sid,
			 void *context)
{
	struct arfw_usb_ep *ep;
	struct usb_device *dev;

	AR_ASSERT(packet);
	ep = packet->ep;
	AR_ASSERT(ep);
	AR_ASSERT(ep->driver);
	AR_ASSERT(ep->driver->intf);
	dev = interface_to_usbdev(ep->driver->intf);
	AR_ASSERT(dev);

	return arfw_usb_packet_submit(packet, sid, context,
				      usb_sndbulkpipe(dev, ep->num));
}

int arfw_usb_packet_recv(struct arfw_usb_packet *packet, uint16_t sid,
			 void *context)
{
	struct arfw_usb_ep *ep;
	struct usb_device *dev;

	AR_ASSERT(packet);
	ep = packet->ep;
	AR_ASSERT(ep);
	AR_ASSERT(ep->driver);
	AR_ASSERT(ep->driver->intf);
	dev = interface_to_usbdev(ep->driver->intf);
	AR_ASSERT(dev);

	return arfw_usb_packet_submit(packet, sid, context,
				      usb_rcvbulkpipe(dev, ep->num));
}

int arfw_usb_packet_recv_all(struct arfw_usb_ep *ep, uint16_t sid,
			     void *context)
{
	int err;
	struct arfw_usb_packet *packet;

	AR_ASSERT(ep);

	packet = arfw_usb_packet_pool_get(ep->pool);
	while (!IS_ERR_OR_NULL(packet)) {
		err = arfw_usb_packet_recv(packet, sid, context);
		if (err) {
			arfw_usb_packet_put(packet);
			return err;
		}
		packet = arfw_usb_packet_pool_get(ep->pool);
	}

	return 0;
}

int arfw_usb_ep_init(struct arfw_usb_ep *ep)
{
	struct arfw_usb_driver *driver;

	AR_ASSERT(ep);
	driver = ep->driver;
	AR_ASSERT(driver);
	AR_ASSERT(driver->intf);
	AR_ASSERT(driver->offload_queue);

	ep->offload_queue = driver->offload_queue;
	init_usb_anchor(&ep->urbs);
	atomic_set(&ep->active_queues, 0);

	ep->active = true;
	AR_LOG_USB_EP_DBG(ep, AR_LOG_CTRL, "Endpoint initialized");

	return 0;
}

void arfw_usb_ep_exit(struct arfw_usb_ep *ep)
{
	AR_ASSERT(ep);

	// Skip un-initialized endpoints. No cleanup to do for those.
	if (!ep->active)
		return;

	// Drain all packets not tracked in a pool.
	if (!usb_anchor_empty(&ep->urbs)) {
		usb_kill_anchored_urbs(&ep->urbs);
		flush_workqueue(ep->offload_queue);
	}

	// Drain all packets from the pool and release it.
	// We kill all URBs and cancel work before releasing.
	if (ep->pool)
		arfw_usb_packet_pool_free(ep->pool);

	ep->active = false;
	AR_LOG_USB_EP_DBG(ep, AR_LOG_CTRL, "Endpoint de-initialized");
}

void arfw_usb_sys_reset(struct arfw_usb_driver *driver)
{
	AR_ASSERT(driver);
	AR_ASSERT(driver->intf);

	if (atomic_cmpxchg(&driver->reset, 0, 1))
		return;

	schedule_delayed_work(&driver->reset_work, 0);
	AR_LOG_USB_DEV_ERR(&driver->intf->dev, AR_LOG_CTRL,
			   "System reset (offloaded)");
}

void arfw_usb_sys_reset_on_fault(struct arfw_usb_driver *driver, int err)
{
	// Bypass unlinked packets.
	// This can happen if we are trying to disconnect and killing packets.
	if (err == -ENOENT || err == -ECONNRESET)
		return;
	arfw_usb_sys_reset(driver);
}

void arfw_usb_sys_reset_offload(struct work_struct *work)
{
	struct arfw_usb_driver *driver;
	struct usb_interface *intf;

	AR_ASSERT(work);
	driver = container_of(work, struct arfw_usb_driver, reset_work.work);
	intf = driver->intf;
	AR_ASSERT(intf);

	// This is for sanity, if we are not in reset - just leave.
	if (!atomic_read(&driver->reset))
		return;

	// This will lead to disconnect/probe invoked and system re-init.
	// We should not touch anything after this call.
	usb_reset_device(interface_to_usbdev(intf));
}

bool arfw_usb_ep_is_inline(const struct arfw_usb_ep *ep)
{
	AR_ASSERT(ep);
	AR_ASSERT(ep->driver);

	// During probe CP reports which endpoints are going to be used for inline messages.
	// If the endpoint is used for both inline and external - this will be set as well.
	// This indicates that the endpoint is inline-capable and requires pre-allocated buffers.

	return !!(ep->driver->cp_inline_eps & BIT(ep->offset));
}
