// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_usb_ctrl.c
 *
 * @brief implementation of the arfw hw control packets utils for USB
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include "arfw_usb_ctrl.h"

#include <linux/container_of.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/usb.h>

#include <ar_common.h>
#include <arfw_log.h>

// We use normal CTRL messages and ROOM messages in the same endpoint.
// We cannot calculate size dynamically because we want to set up endpoints first.
// And ROOM messages would require knowledge of how many rings we will have (firmware info).
// So for IN endpoints we need to have some sort of a limit that we allow.
#define ARFW_USB_CTRL_ROOM_MSG_IN_SIZE                             \
	(sizeof(meta_usb_sync_room_msg_in_t) +                     \
	 sizeof(meta_usb_sync_room_entry_t) * ARFW_USB_MAX_RINGS / \
		 META_USB_RING_TYPES_MAX)
#define ARFW_USB_CTRL_MSG_IN_SIZE (sizeof(meta_usb_ctrl_msg_in_t))
#define ARFW_USB_CTRL_EP_IN_SIZE                                      \
	(ARFW_USB_CTRL_ROOM_MSG_IN_SIZE > ARFW_USB_CTRL_MSG_IN_SIZE ? \
		       ARFW_USB_CTRL_ROOM_MSG_IN_SIZE :                     \
		       ARFW_USB_CTRL_MSG_IN_SIZE)
#define ARFW_USB_CTRL_ROOM_MSG_OUT_MAX_SIZE(driver)                  \
	(sizeof(meta_usb_sync_room_msg_out_t) +                      \
	 sizeof(meta_usb_sync_room_entry_t) * driver->cp_rings_max / \
		 META_USB_RING_TYPES_MAX)

static unsigned long ep_ctrl_in_offset = META_USB_EP_CTRL_IN_OFFSET;
module_param(ep_ctrl_in_offset, ulong, 0664);
static unsigned long ep_ctrl_in_inflight = 5;
module_param(ep_ctrl_in_inflight, ulong, 0664);
static unsigned long ep_ctrl_in_size = ARFW_USB_CTRL_EP_IN_SIZE;
module_param(ep_ctrl_in_size, ulong, 0664);

static unsigned long ep_ctrl_out_offset = META_USB_EP_CTRL_OUT_OFFSET;
module_param(ep_ctrl_out_offset, ulong, 0664);
static unsigned long ep_ctrl_out_inflight = 5;
module_param(ep_ctrl_out_inflight, ulong, 0664);

static unsigned long ep_ctrl_room_sync_inflight = 1;
module_param(ep_ctrl_room_sync_inflight, ulong, 0664);
static unsigned long ep_ctrl_room_sync_delay_ms = 10;
module_param(ep_ctrl_room_sync_delay_ms, ulong, 0664);
static unsigned long ep_ctrl_room_sync_burst_size = 10;
module_param(ep_ctrl_room_sync_burst_size, ulong, 0664);

static int arfw_usb_ctrl_sync_room_from(struct arfw_usb_driver *driver,
					const meta_usb_sync_room_msg_in_t *msg,
					size_t msg_size)
{
	struct arfw_usb_rings *rings;
	const meta_usb_sync_room_entry_t *entry;
	size_t entry_idx, entries_size, entries_num;

	AR_ASSERT(driver);
	AR_ASSERT(msg);
	AR_ASSERT(msg_size);
	rings = &driver->rings[META_USB_DATA_RING_OUT];
	AR_ASSERT(rings->room);

	if (msg->common_header.flag != META_USB_FLAG_OK) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Unexpected reply flag=%u",
				  msg->common_header.flag);
		return -EIO;
	}

	// Figure out how many entries were sent.
	// We only send up to the last active. The size should match that layout.
	entries_size =
		msg_size - offsetof(meta_usb_sync_room_msg_in_t, entries);
	if (entries_size % sizeof(meta_usb_sync_room_entry_t)) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Unexpected reply size=%zu", entries_size);
		return -EINVAL;
	}

	// No lock needed: room is a 16-bit naturally aligned scalar.
	// Use store-release to ensure cross-CPU visibility for readers
	// that may check room on a different CPU via load-acquire.
	entries_num = entries_size / sizeof(meta_usb_sync_room_entry_t);
	for (entry_idx = 0; entry_idx < entries_num; entry_idx++) {
		entry = &msg->entries[entry_idx];
		if (!entry->room)
			continue;
		if (entry->slot >= ARFW_USB_MAX_RINGS) {
			AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
					  "Unexpected room slot=%u, max=%u",
					  entry->slot, ARFW_USB_MAX_RINGS);
			return -EINVAL;
		}
		smp_store_release(&rings->room[entry->slot], entry->room);
	}

	return 0;
}

static size_t arfw_usb_ctrl_sync_room_into(struct arfw_usb_driver *driver,
					   meta_usb_sync_room_msg_out_t *msg)
{
	struct arfw_usb_rings *rings;
	meta_usb_sync_room_entry_t *entry;
	size_t entry_idx = 0;
	uint16_t slot_idx, slots_num;
	size_t msg_size;

	AR_ASSERT(driver);
	AR_ASSERT(msg);
	rings = &driver->rings[META_USB_DATA_RING_IN];
	AR_ASSERT(rings->room);
	slots_num = driver->cp_rings_max / META_USB_RING_TYPES_MAX;
	msg_size = sizeof(meta_usb_sync_room_msg_out_t);

	// No lock needed: room is a 16-bit naturally aligned scalar.
	// Use load-acquire to pair with store-release on writers,
	// ensuring cross-CPU visibility of room updates.
	for (slot_idx = 0; slot_idx < slots_num; slot_idx++) {
		uint16_t room_val = smp_load_acquire(&rings->room[slot_idx]);

		if (!room_val)
			continue;
		entry = &msg->entries[entry_idx++];
		entry->slot = slot_idx;
		entry->room = room_val;
		msg_size += sizeof(meta_usb_sync_room_entry_t);
	}

	return msg_size;
}

static void arfw_usb_ctrl_sync_room_schedule(struct arfw_usb_packet *packet)
{
	struct arfw_usb_driver *driver;

	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	driver = packet->ep->driver;
	AR_ASSERT(driver);
	AR_ASSERT(driver->sync_context);
	AR_ASSERT(packet->ep == driver->ep_ctrl_out);
	AR_ASSERT(packet->ep->offload_queue);

	atomic_set(&driver->sync_burst_total, 0);
	driver->sync_context[packet->pool_index].delayed = true;
	queue_delayed_work(packet->ep->offload_queue, &packet->offload_work,
			   msecs_to_jiffies(ep_ctrl_room_sync_delay_ms));
}

static int arfw_usb_ctrl_resync_room(struct arfw_usb_packet *packet);

static void arfw_usb_ctrl_sync_room_complete(struct work_struct *work)
{
	int err;
	struct arfw_usb_driver *driver;
	struct arfw_usb_packet *packet;
	struct urb *urb;

	// This takes care of the transport level errors.
	AR_ASSERT(work);
	packet = container_of(work, struct arfw_usb_packet, offload_work.work);
	if (!arfw_usb_packet_can_consume(packet))
		return;

	urb = &packet->urb;
	driver = urb->context;
	AR_ASSERT(driver);
	AR_ASSERT(driver->ep_ctrl_out);
	AR_ASSERT(driver->sync_context);

	if (driver->sync_context[packet->pool_index].delayed) {
		err = arfw_usb_ctrl_resync_room(packet);
		if (err)
			arfw_usb_sys_reset_on_fault(driver, err);
		return;
	}

	// Something is seriously wrong here, reset the subsystem.
	if (urb->actual_length != urb->transfer_buffer_length) {
		AR_LOG_USB_EP_ERR(
			driver->ep_ctrl_out, AR_LOG_CTRL,
			"Packet send failed - not all data processed (resetting subsystem), "
			"actual=%d, buffer=%d, idx=%zu",
			urb->actual_length, urb->transfer_buffer_length,
			packet->pool_index);
		goto reset_subsystem;
	}

	// Check that at least the header was sent. This is a sanity check.
	AR_ASSERT(urb->actual_length >= sizeof(meta_usb_sync_room_msg_out_t));
	arfw_usb_ctrl_sync_room_schedule(packet);

	return;

reset_subsystem:
	arfw_usb_sys_reset(driver);
}

static int arfw_usb_ctrl_resync_room(struct arfw_usb_packet *packet)
{
	int err;
	struct arfw_usb_driver *driver;
	meta_usb_sync_room_msg_out_t *msg;
	size_t msg_size;

	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	driver = packet->ep->driver;
	AR_ASSERT(driver);
	AR_ASSERT(driver->sync_context);
	AR_ASSERT(packet->ep == driver->ep_ctrl_out);
	msg = packet->buffer;
	AR_ASSERT(msg);

	msg->common_header.msg_type = META_USB_SYNC_ROOM_MSG_OUT;
	msg->common_header.flag = META_USB_FLAG_OK;
	// Specifically set seq_num to zero since promises are not supported.
	msg->common_header.seq_num = 0;
	msg_size = arfw_usb_ctrl_sync_room_into(driver, msg);

	// Check if there is anything to send by comparing msg to the header.
	// When there is nothing to send - just reschedule the sync packet.
	// We use a configured delay to send these beats.
	if (msg_size == sizeof(*msg)) {
		arfw_usb_ctrl_sync_room_schedule(packet);
		return 0;
	}

	// Check that we fit into the overall packet size allocated.
	// We should always fit, but sanity checking is good.
	// Overwrite the size with the actual size we are transfering.
	AR_ASSERT(ARFW_USB_CTRL_ROOM_MSG_OUT_MAX_SIZE(driver) >= msg_size);
	packet->buffer_size = msg_size;
	driver->sync_context[packet->pool_index].delayed = false;

	err = arfw_usb_packet_send(packet, /* sid = */ 0, driver);
	if (err) {
		AR_LOG_USB_EP_ERR(
			packet->ep, AR_LOG_CTRL,
			"Failed to send a room sync message with err=%d", err);
		return err;
	}

	return 0;
}

void arfw_usb_ctrl_reply_promise_init(
	struct arfw_usb_driver *driver,
	struct arfw_usb_ctrl_reply_promise *promise, ar_future_t *future,
	arfw_usb_ctrl_reply_complete_t *complete)
{
	struct usb_interface *intf;
	uint16_t seq;

	AR_ASSERT(driver);
	intf = driver->intf;
	AR_ASSERT(intf);
	AR_ASSERT(future);
	AR_ASSERT(complete);

	// The protocol seq_num field is uint16_t, mask explicitly to avoid
	// relying on implicit truncation from the 32-bit atomic counter.
	// Skip 0 since it is reserved for promise-less messages (room sync).
	do {
		seq = (uint16_t)atomic_inc_return(&driver->ctrl_seq_num);
	} while (!seq);

	promise->seq_num = seq;
	promise->context = driver;
	promise->complete = complete;
	promise->base.lock = &driver->ctrl_reply_lock;
	ar_future_set_promise(future, &promise->base);
}

static int arfw_usb_ctrl_handle_reply(struct arfw_usb_driver *driver,
				      meta_usb_ctrl_msg_in_t *msg,
				      size_t msg_size)
{
	unsigned long flags;
	ar_promise_t *promise, *tmp;
	struct arfw_usb_ctrl_reply_promise *ctrl_promise;
	bool promise_found = false;

	AR_ASSERT(driver);
	AR_ASSERT(driver->ep_ctrl_in);
	AR_ASSERT(msg);
	AR_ASSERT(msg_size);

	// We are not checking flag here since we need each
	// handler to check against the specific type of errors.
	switch (msg->common_header.msg_type) {
	case META_USB_CREATE_RING_MSG_IN:
		/* fallthrough */
	case META_USB_DELETE_RING_MSG_IN:
		/* fallthrough */
	case META_USB_QUERY_INFO_MSG_IN:
		break;
	case META_USB_SYNC_ROOM_MSG_IN:
		// This is a special type of message that does not go back
		// to the client, this is handled within the driver internally.
		// There is no future/promise associated with this message.
		return arfw_usb_ctrl_sync_room_from(
			driver, (meta_usb_sync_room_msg_in_t *)msg, msg_size);
	case META_USB_CTRL_GEN_MSG_IN:
		// To keep parity with our PCIe processing.
		// Each generic nack will be handled the same - drop the packet and log.
		// We will not reset the device and user will timeout on this request.
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Unexpected nack seq=0x%x",
				  msg->common_header.seq_num);
		return 0;
	default:
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Unexpected reply type=0x%x, seq=0x%x",
				  msg->common_header.msg_type,
				  msg->common_header.seq_num);
		return -EINVAL;
	}

	// Select a promise that matches our seq_num. Current implementation is
	// a bit wonky, since we traverse a list here, but really we should only
	// have just a few promises there at a time. And it is control path.
	// Maybe we could use xarray later.
	spin_lock_irqsave(&driver->ctrl_reply_lock, flags);
	list_for_each_entry_safe(promise, tmp, &driver->ctrl_reply_list, list) {
		ctrl_promise = container_of(
			promise, struct arfw_usb_ctrl_reply_promise, base);
		if (ctrl_promise->seq_num == msg->common_header.seq_num) {
			promise_found = true;
			list_del_init(&promise->list);
			break;
		}
	}
	spin_unlock_irqrestore(&driver->ctrl_reply_lock, flags);

	if (!promise_found)
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "No promise for seq=%u",
				  msg->common_header.seq_num);
	else
		// At this point the promise now owns the packet memory
		// and is responsible for freeing it.
		ctrl_promise->complete(ctrl_promise, msg);

	return 0;
}

static void arfw_usb_ctrl_recv_complete(struct work_struct *work)
{
	int err;
	struct arfw_usb_driver *driver;
	struct arfw_usb_packet *packet;
	struct urb *urb;

	// This takes care of the transport level errors.
	AR_ASSERT(work);
	packet = container_of(work, struct arfw_usb_packet, offload_work.work);
	if (!arfw_usb_packet_can_consume(packet))
		return;

	urb = &packet->urb;
	driver = urb->context;
	AR_ASSERT(driver);
	AR_ASSERT(driver->ep_ctrl_in);

	// Something is seriously wrong here, reset the subsystem.
	// All messages should include the header.
	if (urb->actual_length < sizeof(meta_usb_common_header_t)) {
		AR_LOG_USB_EP_ERR(
			driver->ep_ctrl_in, AR_LOG_CTRL,
			"Packet recv failed - not all data processed (resetting subsystem), "
			"actual=%d, buffer=%d, idx=%zu",
			urb->actual_length, urb->transfer_buffer_length,
			packet->pool_index);
		goto reset_subsystem;
	}

	err = arfw_usb_ctrl_handle_reply(driver, urb->transfer_buffer,
					 urb->actual_length);
	if (err) {
		AR_LOG_USB_EP_ERR(
			driver->ep_ctrl_in, AR_LOG_CTRL,
			"Packet handle failed - cannot process the packet (resetting subsystem), "
			"size=%d, idx=%zu",
			urb->actual_length, packet->pool_index);
		goto reset_subsystem;
	}

	// After we process each in-packet we need re-submit it.
	// This is done only on in-endpoints since host is polling for data there.
	// For out-endpoints we only send packets on demand from the client.
	arfw_usb_packet_resubmit(packet);
	return;

reset_subsystem:
	arfw_usb_packet_put(packet);
	arfw_usb_sys_reset(driver);
}

static void arfw_usb_ctrl_send_complete(struct work_struct *work)
{
	struct arfw_usb_driver *driver;
	struct arfw_usb_packet *packet;
	struct urb *urb;

	// This takes care of the transport level errors.
	AR_ASSERT(work);
	packet = container_of(work, struct arfw_usb_packet, offload_work.work);
	if (!arfw_usb_packet_can_consume(packet))
		return;

	urb = &packet->urb;
	driver = urb->context;
	AR_ASSERT(driver);
	AR_ASSERT(driver->ep_ctrl_out);

	// Something is seriously wrong here, reset the subsystem.
	if (urb->actual_length != urb->transfer_buffer_length) {
		AR_LOG_USB_EP_ERR(
			driver->ep_ctrl_out, AR_LOG_CTRL,
			"Packet send failed - not all data processed (resetting subsystem), "
			"actual=%d, buffer=%d, idx=%zu",
			urb->actual_length, urb->transfer_buffer_length,
			packet->pool_index);
		goto reset_subsystem;
	}

	// Sanity check that we at least sending the header.
	// All messages should include the header.
	AR_ASSERT(urb->actual_length >= sizeof(meta_usb_common_header_t));

	// We never resubmit ctrl out packets, so we can just drop them here
	// and the client will retry if needed since we block there until we get
	// a reply from the firmware.
	arfw_usb_ctrl_packet_put(packet);
	return;

reset_subsystem:
	arfw_usb_ctrl_packet_put(packet);
	arfw_usb_sys_reset(driver);
}

static void
arfw_usb_ctrl_info_query_reply(struct arfw_usb_ctrl_reply_promise *promise,
			       meta_usb_ctrl_msg_in_t *msg)
{
	ar_future_t *future;
	struct arfw_usb_driver *driver;
	struct usb_interface *intf;
	meta_usb_query_info_msg_in_t *info;

	AR_ASSERT(promise);
	AR_ASSERT(msg);
	future = promise->base.future;
	AR_ASSERT(future);
	driver = promise->context;
	AR_ASSERT(driver);
	AR_ASSERT(driver->ep_ctrl_in);
	intf = driver->intf;
	AR_ASSERT(intf);

	if (msg->common_header.flag != META_USB_FLAG_OK) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Firmware returned bad response flag=%u",
				  msg->common_header.flag);
		ar_future_complete(future, -EIO);
	} else if (msg->common_header.msg_type != META_USB_QUERY_INFO_MSG_IN) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Firmware returned bad response type=%u",
				  msg->common_header.msg_type);
		ar_future_complete(future, -EIO);
	} else {
		// Copy the info out of the packet buffer before it gets resubmitted.
		// The msg pointer belongs to the ctrl IN pool and will be reused.
		info = devm_kmemdup(&intf->dev, &msg->query_info, sizeof(*info),
				    GFP_ATOMIC);
		if (!info) {
			ar_future_complete(future, -ENOMEM);
			return;
		}
		ar_future_complete_data(future, info);
	}
}

ar_future_t *arfw_usb_ctrl_info_query(struct arfw_usb_driver *driver,
				      const unsigned long timeout_ms)
{
	int err;
	unsigned long flags;
	struct arfw_usb_ep *ep;
	struct arfw_usb_ctrl_reply_promise *promise;
	ar_future_t *future;
	uint16_t seq_num;
	struct arfw_usb_packet *packet;
	meta_usb_query_info_msg_out_t *msg;
	struct usb_interface *intf;

	AR_ASSERT(driver);
	ep = driver->ep_ctrl_out;
	AR_ASSERT(ep);
	intf = driver->intf;
	AR_ASSERT(intf);

	future = ar_create_future(&intf->dev);
	if (!future) {
		AR_LOG_USB_EP_ERR(ep, AR_LOG_CTRL,
				  "Failed to allocate a future");
		err = -ENOMEM;
		goto error_create_future;
	}

	promise = devm_kzalloc(&intf->dev, sizeof(*promise), GFP_KERNEL);
	if (!promise) {
		AR_LOG_USB_EP_ERR(ep, AR_LOG_CTRL,
				  "Failed to allocate a promise");
		err = -ENOMEM;
		goto error_create_promise;
	}
	arfw_usb_ctrl_reply_promise_init(driver, promise, future,
					 arfw_usb_ctrl_info_query_reply);

	packet = arfw_usb_ctrl_packet_get(driver, sizeof(*msg));
	if (IS_ERR(packet)) {
		err = PTR_ERR(packet);
		AR_LOG_USB_EP_ERR(ep, AR_LOG_CTRL,
				  "Failed to allocate a packet, err=%d", err);
		goto error_create_packet;
	}

	seq_num = promise->seq_num;
	promise->context = driver;
	ar_future_set_timeout(future, timeout_ms);

	spin_lock_irqsave(promise->base.lock, flags);
	list_add_tail(&promise->base.list, &driver->ctrl_reply_list);
	spin_unlock_irqrestore(promise->base.lock, flags);

	msg = packet->buffer;
	msg->common_header.msg_type = META_USB_QUERY_INFO_MSG_OUT;
	msg->common_header.flag = META_USB_FLAG_OK;
	msg->common_header.seq_num = seq_num;
	msg->ap_version = driver->ap_version;

	err = arfw_usb_ctrl_packet_send(packet);
	if (err) {
		AR_LOG_USB_EP_ERR(
			ep, AR_LOG_CTRL,
			"Failed to send a query info request with err=%d", err);
		goto error_send_packet;
	}

	AR_LOG_USB_EP_DBG(ep, AR_LOG_CTRL,
			  "Sent a query info request with seq=%u", seq_num);

	return future;

error_send_packet:
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

int arfw_usb_ctrl_sync_room_init(struct arfw_usb_driver *driver)
{
	int err;
	struct arfw_usb_ep *ep;
	struct arfw_usb_packet *packet;
	struct usb_interface *intf;
	size_t msg_size;

	AR_ASSERT(driver);
	ep = driver->ep_ctrl_out;
	AR_ASSERT(ep);
	intf = driver->intf;
	AR_ASSERT(intf);

	atomic_set(&driver->sync_burst_total, 0);
	driver->sync_burst_max = ep_ctrl_room_sync_burst_size;
	msg_size = ARFW_USB_CTRL_ROOM_MSG_OUT_MAX_SIZE(driver);

	driver->sync_pool = arfw_usb_packet_pool_alloc(
		driver, arfw_usb_ctrl_sync_room_complete,
		ep_ctrl_room_sync_inflight, msg_size);
	if (IS_ERR(driver->sync_pool)) {
		err = PTR_ERR(driver->sync_pool);
		AR_LOG_USB_EP_ERR(
			ep, AR_LOG_CTRL,
			"Failed to allocate a sync room packet pool, err=%d",
			err);
		goto error_alloc_packet_pool;
	}
	arfw_usb_packet_pool_init(driver->sync_pool, ep);

	driver->sync_context =
		devm_kcalloc(&intf->dev, ep_ctrl_room_sync_inflight,
			     sizeof(*driver->sync_context), GFP_KERNEL);
	if (!driver->sync_context) {
		err = -ENOMEM;
		AR_LOG_USB_EP_ERR(
			ep, AR_LOG_CTRL,
			"Failed to allocate a sync room context, err=%d", err);
		goto error_alloc_context;
	}

	// Currently only use one packet, but maybe we will need more later.
	// Still utilize a pool here to keep it generic.
	packet = arfw_usb_packet_pool_get(driver->sync_pool);
	if (IS_ERR(packet)) {
		err = PTR_ERR(packet);
		AR_LOG_USB_EP_ERR(
			ep, AR_LOG_CTRL,
			"Failed to allocate a sync room packet, err=%d", err);
		goto error_get_packet;
	}

	err = arfw_usb_ctrl_resync_room(packet);
	if (err) {
		AR_LOG_USB_EP_ERR(ep, AR_LOG_CTRL,
				  "Failed to start room sync, err=%d", err);
		goto error_resync_room;
	}

	AR_LOG_USB_EP_DBG(ep, AR_LOG_CTRL, "Started room sync");
	return 0;

error_resync_room:
	arfw_usb_packet_put(packet);
error_get_packet:
	devm_kfree(&intf->dev, driver->sync_context);
	driver->sync_context = NULL;
error_alloc_context:
	arfw_usb_packet_pool_free(driver->sync_pool);
	driver->sync_pool = NULL;
error_alloc_packet_pool:
	return err;
}

void arfw_usb_ctrl_sync_room_bump(struct arfw_usb_driver *driver)
{
	size_t idx;
	struct arfw_usb_packet *packet;

	AR_ASSERT(driver);
	AR_ASSERT(driver->sync_pool);
	AR_ASSERT(driver->ep_ctrl_out);
	AR_ASSERT(driver->ep_ctrl_out->offload_queue);

	// If burst buckets are not supported, skip.
	if (!driver->sync_burst_max)
		return;

	// If within the bucket, skip.
	if (atomic_inc_return(&driver->sync_burst_total) !=
	    driver->sync_burst_max)
		return;

	// Flush packet otherwise, still offload, just use immediate.
	for (idx = 0; idx < driver->sync_pool->pool_size; idx++) {
		packet = &driver->sync_pool->packets[idx];
		if (cancel_delayed_work(&packet->offload_work))
			queue_work(driver->ep_ctrl_out->offload_queue,
				   &packet->offload_work.work);
	}
}

static struct arfw_usb_ep *arfw_usb_ctrl_ep_init(struct arfw_usb_driver *driver,
						 size_t ep_offset)
{
	int err;
	struct arfw_usb_ep *ep;
	struct usb_host_interface *intf;
	struct usb_endpoint_descriptor *ep_desc;
	bool ep_out;

	AR_ASSERT(driver);
	AR_ASSERT(driver->intf);
	intf = driver->intf->cur_altsetting;
	AR_ASSERT(intf);
	AR_ASSERT(ep_offset == ep_ctrl_in_offset ||
		  ep_offset == ep_ctrl_out_offset);

	ep_desc = &intf->endpoint[ep_offset].desc;
	ep = &driver->eps[ep_offset];
	ep->num = usb_endpoint_num(ep_desc);
	ep->offset = ep_offset;
	ep->driver = driver;
	ep->ctrl = true;
	ep_out = ep_offset == ep_ctrl_out_offset;

	if (ep_out ? !usb_endpoint_is_bulk_out(ep_desc) :
			   !usb_endpoint_is_bulk_in(ep_desc)) {
		AR_LOG_USB_DEV_ERR(
			&driver->intf->dev, AR_LOG_CTRL,
			"Ctrl endpoint num=%d type/dir pair is incorrect, "
			"expected dir=%s type=2(bulk), got dir=%s type=%d",
			ep->num, (ep_out ? "out" : "in"),
			(usb_endpoint_dir_out(ep_desc) ? "out" : "in"),
			usb_endpoint_type(ep_desc));
		return ERR_PTR(-EINVAL);
	}

	err = arfw_usb_ep_init(ep);
	if (err)
		return ERR_PTR(err);

	return ep;
}

static int arfw_usb_ctrl_ep_init_in(struct arfw_usb_driver *driver)
{
	int err;
	struct arfw_usb_ep *ep;

	AR_ASSERT(driver);

	// For RECV/IN we need a pool because we blindly schedule packets.
	// This allows us to schedule multiple concurrent packets without back and forth.
	ep = arfw_usb_ctrl_ep_init(driver, ep_ctrl_in_offset);
	if (IS_ERR(ep)) {
		err = PTR_ERR(ep);
		goto error_ep_init;
	}

	ep->pool = arfw_usb_packet_pool_alloc(driver,
					      arfw_usb_ctrl_recv_complete,
					      ep_ctrl_in_inflight,
					      ep_ctrl_in_size);
	if (IS_ERR(ep->pool)) {
		err = PTR_ERR(ep->pool);
		AR_LOG_USB_EP_ERR(ep, AR_LOG_CTRL,
				  "Failed to allocate packet pool for endpoint "
				  "inflight=%lu, size=%zu, err=%d",
				  ep_ctrl_in_inflight, ep_ctrl_in_size, err);
		goto error_pool_alloc;
	}
	arfw_usb_packet_pool_init(ep->pool, ep);

	driver->ep_ctrl_in = ep;
	return 0;

error_pool_alloc:
	arfw_usb_ep_exit(ep);
error_ep_init:
	return err;
}

static int arfw_usb_ctrl_ep_init_out(struct arfw_usb_driver *driver)
{
	struct arfw_usb_ep *ep;

	AR_ASSERT(driver);

	// For SEND/OUT we need no pool because we can allocate packets on demand.
	// This avoids throttling the control flow over the pool size.
	ep = arfw_usb_ctrl_ep_init(driver, ep_ctrl_out_offset);
	if (IS_ERR(ep))
		return PTR_ERR(ep);

	driver->ep_ctrl_out = ep;
	return 0;
}

int arfw_usb_ctrl_init(struct arfw_usb_driver *driver)
{
	int err;
	struct usb_host_interface *intf;
	uint16_t ring_type;

	AR_ASSERT(driver);
	AR_ASSERT(driver->intf);
	intf = driver->intf->cur_altsetting;
	AR_ASSERT(intf);

	if (intf->desc.bNumEndpoints < ARFW_USB_EP_CTRL_NUM) {
		err = -EINVAL;
		AR_LOG_USB_DEV_ERR(
			&driver->intf->dev, AR_LOG_CTRL,
			"At least %d endpoints required to support control packets,"
			" got %d from firmware",
			ARFW_USB_EP_CTRL_NUM, intf->desc.bNumEndpoints);
		goto error_num_endpoints;
	}

	for (ring_type = 0; ring_type < META_USB_RING_TYPES_MAX; ring_type++) {
		// Allocate room sync entries for each possible ring/queue.
		// We do not have to send them all, we can send only the ones in use.
		driver->rings[ring_type].room = devm_kcalloc(
			&driver->intf->dev, ARFW_USB_MAX_RINGS,
			sizeof(*driver->rings[ring_type].room), GFP_KERNEL);
		if (!driver->rings[ring_type].room) {
			err = -ENOMEM;
			AR_LOG_USB_DEV_ERR(
				&driver->intf->dev, AR_LOG_CTRL,
				"Failed to allocate sync room storage");
			goto error_room_alloc;
		}
	}

	err = arfw_usb_ctrl_ep_init_in(driver);
	if (err)
		goto error_ep_init_in;

	err = arfw_usb_ctrl_ep_init_out(driver);
	if (err)
		goto error_ep_init_out;

	atomic_set(&driver->ctrl_seq_num, 0);
	INIT_LIST_HEAD(&driver->ctrl_reply_list);
	spin_lock_init(&driver->ctrl_reply_lock);
	driver->ap_version.major = META_USB_AP_PROTOCOL_MAJOR_VERSION;
	driver->ap_version.minor = META_USB_AP_PROTOCOL_MINOR_VERSION;

	err = arfw_usb_packet_recv_all(driver->ep_ctrl_in,
				       /* sid = */ 0, driver);
	if (err)
		goto error_ep_recv_all;

	return 0;

error_ep_recv_all:
	arfw_usb_ep_exit(driver->ep_ctrl_out);
error_ep_init_out:
	arfw_usb_ep_exit(driver->ep_ctrl_in);
error_ep_init_in:
error_room_alloc:
	for (ring_type = 0; ring_type < META_USB_RING_TYPES_MAX; ring_type++)
		if (driver->rings[ring_type].room)
			devm_kfree(&driver->intf->dev,
				   driver->rings[ring_type].room);
error_num_endpoints:
	return err;
}

void arfw_usb_ctrl_exit(struct arfw_usb_driver *driver)
{
	uint16_t ring_type;

	AR_ASSERT(driver);
	AR_ASSERT(driver->intf);

	if (driver->sync_pool) {
		// This will drain, kill and cancel all flow control work.
		arfw_usb_packet_pool_free(driver->sync_pool);
		driver->sync_pool = NULL;
	}
	if (driver->sync_context) {
		devm_kfree(&driver->intf->dev, driver->sync_context);
		driver->sync_context = NULL;
	}

	// No need to release the pool explicitly.
	// If set on an endpoint it will be take care of below.
	if (driver->ep_ctrl_out) {
		arfw_usb_ep_exit(driver->ep_ctrl_out);
		driver->ep_ctrl_out = NULL;
	}
	if (driver->ep_ctrl_in) {
		arfw_usb_ep_exit(driver->ep_ctrl_in);
		driver->ep_ctrl_in = NULL;
	}

	// Now should be safe to remove flow control bits.
	for (ring_type = 0; ring_type < META_USB_RING_TYPES_MAX; ring_type++) {
		if (driver->rings[ring_type].room) {
			devm_kfree(&driver->intf->dev,
				   driver->rings[ring_type].room);
			driver->rings[ring_type].room = NULL;
		}
	}
}

struct arfw_usb_packet *arfw_usb_ctrl_packet_get(struct arfw_usb_driver *driver,
						 size_t size)
{
	AR_ASSERT(driver);
	AR_ASSERT(driver->ep_ctrl_out);

	return arfw_usb_packet_get(driver->ep_ctrl_out,
				   arfw_usb_ctrl_send_complete, size,
				   &driver->ep_ctrl_out->urbs);
}

void arfw_usb_ctrl_packet_put(struct arfw_usb_packet *packet)
{
	struct arfw_usb_driver *driver;

	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	driver = packet->ep->driver;
	AR_ASSERT(driver);
	AR_ASSERT(packet->ep == driver->ep_ctrl_out);

	arfw_usb_packet_put(packet);
}

int arfw_usb_ctrl_packet_send(struct arfw_usb_packet *packet)
{
	struct arfw_usb_driver *driver;

	AR_ASSERT(packet);
	AR_ASSERT(packet->ep);
	driver = packet->ep->driver;
	AR_ASSERT(driver);
	AR_ASSERT(packet->ep == driver->ep_ctrl_out);

	return arfw_usb_packet_send(packet, /* sid = */ 0, driver);
}
