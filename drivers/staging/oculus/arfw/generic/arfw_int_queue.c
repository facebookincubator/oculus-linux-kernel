// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file arfw_int_queue.c
 *
 * @brief Contains operations to create, destroy, and use arfw_client_queue
 * objects.
 *
 *******************************************************************************/

#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/moduleparam.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <ar_atomics.h>
#include <ar_fw_message.h>
#include <arfw_log.h>

#include "arfw_int.h"
#include "arfw_int_map.h"
#include "arfw_int_queue.h"
#include "arfw_mem_util.h"

#include "arfw_debug.h"

// Limit queue pinned regions total size to 1GB by default.
static unsigned long queue_total_region_size_max = 0x40000000;
module_param(queue_total_region_size_max, ulong, 0664);

static ar_atomic_t queue_id_counter;

static int reformat_sg_payload(struct arfw_client_queue *client,
			       ar_firmware_message_header_t *header)
{
	int err = 0;
	int i;
	unsigned long flags = 0;
	ar_firmware_msg_sg_elem_t *sg_elem = NULL;
	uint16_t mem_id;
	struct arfw_client_region *region = NULL;
	uint32_t num_entries = 0;
	struct arfw_dma_sg_elem *dma_sg_elem = NULL;
	uint16_t sg_offset;

	// Get an offset for the SG list, which should be aligned so the firmware side
	// could optimize read/write IO.
	sg_offset = ALIGN(header->inline_msg_len, sizeof(uint64_t));
	if (sg_offset > header->data_size) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_WRITE,
				      "No sg payload",
				      "data size %u, inline msg len %u",
				      header->data_size,
				      header->inline_msg_len);
		return -EINVAL;
	}

	// sg list entries are based off data size
	if ((header->data_size - sg_offset) %
	    sizeof(ar_firmware_msg_sg_elem_t)) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_WRITE,
				      "Bad data size for sg list",
				      "data size %u", header->data_size);
		return -EINVAL;
	}

	num_entries = (header->data_size - sg_offset) / sizeof(sg_elem[0]);
	// The data_size contains the inline message size and sizeof SG array.
	if ((num_entries * AR_FIRMWARE_MSG_SG_SEND_SIZE) + sg_offset >
	    (client->queue->queue_meta.element_size -
	     client->queue->queue_device_info.inline_data_offset)) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_WRITE,
				      "The current element_size is too small",
				      "element_size %u, required size %lu",
				      client->queue->queue_meta.element_size,
				      num_entries * sizeof(sg_elem[0]) +
					      sg_offset +
					      client->queue->queue_device_info
						      .inline_data_offset);
		return -EINVAL;
	}

	sg_elem = (ar_firmware_msg_sg_elem_t *)((uintptr_t)header + sg_offset +
						client->queue->queue_device_info
							.inline_data_offset);
	dma_sg_elem = (struct arfw_dma_sg_elem *)sg_elem;

	spin_lock_irqsave(&client->queue->locked_regions.lock, flags);
	for (i = 0; i < num_entries; i++) {
		region = idr_find(&client->queue->locked_regions.region_idr,
				  sg_elem->roundtrip.region_id);
		if (region == NULL) {
			/**
			 * workaround for ARFW-LOOPBACK queues, which can send pend ids. In this case, just move
			 * on to the next value since we don't care about send consumed for pends, and we
			 * don't need to increment the buffer refcount either. (since the region lifetime
			 * is managed by the pend side)
			 */
			if (sg_elem->roundtrip.pend_id > 0) {
				sg_elem++;
				dma_sg_elem++;
				continue;
			}

			AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_WRITE,
					      "Bad region id for sg list",
					      "region id %u",
					      sg_elem->roundtrip.region_id);
			err = -EINVAL;
			goto unlock_error;
		}

		if (sg_elem->offset > region->mapping.region_size ||
		    sg_elem->offset + sg_elem->size >
			    region->mapping.region_size) {
			AR_LOG_ARFW_QUEUE_ERR(
				client->queue, AR_LOG_WRITE,
				"Bad offset/size for sg list item",
				"offset %u size %llu region %u size %zu",
				sg_elem->offset, sg_elem->size, region->id,
				region->mapping.region_size);
			err = -ERANGE;
			goto unlock_error;
		}

		err = arfw_queue_ref_region(client,
					    sg_elem->roundtrip.region_id);
		if (err)
			goto unlock_error;

		// If it is PCI based queue, then change format to DMA specific format.
		if (client->queue->driver_ops->handle_dma_map_quirk) {
			AR_LOG_ARFW_QUEUE_DBG(
				client->queue, AR_LOG_WRITE,
				"Rewrite offset to dma addr",
				"region %u, size %llu, dma addr %llx",
				sg_elem->roundtrip.region_id, sg_elem->size,
				region->mapping.dma_region_addr +
					sg_elem->offset);

			// Perform inplace conversion to pass this data to the PCI layer.
			// Because of the inplace conversion store region_id in the
			// temporary variable first. Otherwise it would be rewritten.
			mem_id = sg_elem->roundtrip.region_id;
			dma_sg_elem->addr = region->mapping.dma_region_addr +
					    sg_elem->offset;
			dma_sg_elem->len = sg_elem->size;
			dma_sg_elem->mem_id = mem_id;
		}
		// Do nothing for the loopback device.

		// move to next element
		sg_elem++;
		dma_sg_elem++;
	}

	spin_unlock_irqrestore(&client->queue->locked_regions.lock, flags);

	return 0;

unlock_error:
	// Since it is an error path dereference all the regions referenced before.
	i--;
	sg_elem = (ar_firmware_msg_sg_elem_t *)((uintptr_t)header + sg_offset +
						client->queue->queue_device_info
							.inline_data_offset);
	dma_sg_elem = (struct arfw_dma_sg_elem *)sg_elem;
	while (i >= 0) {
		if (client->queue->driver_ops->handle_dma_map_quirk)
			arfw_queue_deref_region(client, dma_sg_elem[i].mem_id);
		else
			arfw_queue_deref_region(client,
						sg_elem->roundtrip.region_id);
		i--;
	}
	spin_unlock_irqrestore(&client->queue->locked_regions.lock, flags);

	return err;
}

int arfw_queue_external_payload_consumed(struct arfw_client_queue *client,
					 void *header)
{
	ar_firmware_message_header_t *hdr;
	uint32_t num_entries;
	struct arfw_dma_sg_elem *dma_sg_elem;
	ar_firmware_msg_sg_recv_t *recv_sg_elem;
	uint16_t mem_id;
	int i;
	int ret = 0, err;
	struct arfw_client_event consumed_event = {};

	AR_ASSERT(header);

	hdr = (ar_firmware_message_header_t *)header;
	AR_ASSERT(data_location_external_to_queue(hdr->data_location));
	num_entries = arfw_sg_buf_count_of_msg(hdr);
	dma_sg_elem = (struct arfw_dma_sg_elem *)arfw_sg_list_of_msg(
		hdr, client->queue->queue_device_info.inline_data_offset);
	// The loopback device changes the SG format to recv_sg in the
	// arfw_loopback_dev_handle_send_queue_request() routine. Use it
	// instead.
	recv_sg_elem = (ar_firmware_msg_sg_recv_t *)dma_sg_elem;

	consumed_event.type = AR_QUEUE_PAYLOAD_CONSUMED;
	for (i = 0; i < num_entries; i++) {
		// DMA devices and loopback device passing data differently
		// on the send queue. Use different fields to get mem_id.
		if (client->queue->driver_ops->handle_dma_map_quirk)
			mem_id = dma_sg_elem[i].mem_id;
		else
			mem_id = recv_sg_elem[i].roundtrip.region_id;
		consumed_event.memory_id = mem_id;
		arfw_queue_event_enqueue(client, &consumed_event);
		err = arfw_queue_deref_region(client, mem_id);
		// Don't break and try to handle next buffer in the array.
		if (err) {
			AR_LOG_ARFW_QUEUE_ERR(client->queue,
					      AR_LOG_INDEX_CONSUMED,
					      "Couldn't decref region", "id %d",
					      mem_id);
			ret = err;
		}
	}

	return ret;
}

/**
 * Callback invoked when a new request is received on queue.
 *
 * Assumes the serial work lock is already held on a queue.
 *
 * @param[in] callback_context Callback context registered during queue creation.
 * @param[in] request Pointer to ar_fw_io_request_t
 * @param[in] request_context Request context passed to the
 * request_callback during the poll call.
 * @param[out] mark_consumed True if the slot is ready to free. False if
 * ar_fw_queue_mark_slot_ready will be called later by data consumer.
 *
 * @retval TRUE if polling should continue
 */
static bool arfw_queue_handle_request(void *callback_context,
				      ar_fw_io_request_t *request,
				      void *request_context,
				      bool *mark_consumed)
{
	int err = 0;
	bool ret;
	struct arfw_client_queue *client = NULL;
	struct arfw_io_request req;
	ar_firmware_message_header_t *header = NULL;
	time64_t now_seconds = ktime_get_real_seconds();

	client = (struct arfw_client_queue *)callback_context;

	req.data = request->data;
	req.data_size = request->data_size;
	req.index = request->index;

	header = (ar_firmware_message_header_t *)req.data;

	if (debugfs_initialized()) {
		header->kernel_creation_timestamp_us = ktime_to_us(ktime_get());
		ar_binned_sampler_add_sample(
			client->debug.latency_sampler,
			header->kernel_creation_timestamp_us -
				header->user_creation_timestamp_us);
		if (unlikely(header->kernel_creation_timestamp_us -
				     header->user_creation_timestamp_us >
			     THRESHOLD_TIME)) {
			client->debug.latency_threshold_last_ts = now_seconds;
			if (likely(now_seconds - client->debug.latency_log_last_ts >
				   MIN_LATENCY_LOG_DELAY)) {
				client->debug.latency_log_last_ts = now_seconds;
				AR_LOG_ARFW_QUEUE_WARN(
					client->queue, AR_LOG_WRITE,
					"Msg servicing longer than threshold",
					"took: %u us",
					header->kernel_creation_timestamp_us -
						header->user_creation_timestamp_us);
			}
		}
	}

	AR_LOG_ARFW_QUEUE_DBG(
		client->queue, AR_LOG_WRITE, "Handle req on queue.",
		"idx %d data loc %d, data size %u, message id 0x%x, tracking id %d, sequence id %d",
		req.index, header->data_location, header->data_size,
		header->msg_id, header->tracking_id, header->sequence_id);

	if (data_location_external_to_queue(header->data_location)) {
		err = reformat_sg_payload(client, header);
		// on failure, eat the bad msg and stop polling
		if (err) {
			AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_WRITE,
					      "Unable to reformat sg payload",
					      "err %d", err);
			goto exit;
		}
	}

exit:
	if (err) {
		ar_atomic64_fetch_add(&client->debug.drop_count, 1,
				      AR_MEMORY_ORDER_RELAXED);
		*mark_consumed = true;
		return false;
	}

	ret = client->queue->driver_ops->handle_send_queue(
		client->queue->queue_context, &req, mark_consumed);
	if (!ret)
		ar_atomic64_fetch_add(&client->debug.drop_count, 1,
				      AR_MEMORY_ORDER_RELAXED);
	else if (data_location_external_to_queue(header->data_location))
		ar_atomic64_fetch_add(&client->debug.external_count, 1,
				      AR_MEMORY_ORDER_RELAXED);
	else
		ar_atomic64_fetch_add(&client->debug.inline_count, 1,
				      AR_MEMORY_ORDER_RELAXED);

	return ret;
}

/**
 * Reserve the remaining slots in a queue.
 *
 * @param[in] int_queue The internal client queue to reserve
 *
 * @retval non-negative index in the queue that was reserved up to
 * @retval -ENOSPC if no reservations could be made
 */
static int reserve_int_queue(struct arfw_int_client_queue *int_queue)
{
	int queue_err = 0, ret = -ENOSPC;
	struct ar_fw_io_request req;

	// reserve the entire queue, saving the last succesfully reserved index
	while (!queue_err) {
		queue_err =
			ar_fw_queue_slot_reserve(&int_queue->arfw_queue, &req);
		if (!queue_err)
			ret = req.index;

		/**
		 * We are ignoring a dma sync operation here, as we expect support from the IOMMU to manage
		 * cache coherency.
		 */
	}

	// return last reserved index, will be -ENOSPC if no reservations could be made
	return ret;
}

int arfw_queue_create(const struct arfw_driver_ops *driver_ops,
		      void *base_context, struct device *dev,
		      struct device *alloc_device,
		      struct ar_queue_create_req *req,
		      struct arfw_device_information_req *dev_info,
		      struct arfw_client_queue *client)
{
	int err = 0, read_index = 0, id, handle;
	struct ar_fw_queue_creator_data creator_data;
	struct arfw_int_client_queue *int_queue;
	struct arfw_client_queue_create_params params;
	struct task_struct *task;

	if (!req->element_size || !req->depth) {
		AR_LOG_ARFW_DEV_ERR(
			dev, AR_LOG_CREATE_QUEUE,
			"Element size (%u) and depth (%u) cannot be zero [arfw queue: 0x%x%s0x%x]",
			req->element_size, req->depth, req->hlos_endpoint_id,
			req->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" :
									    "<-",
			req->fw_endpoint_id);
		return -EINVAL;
	} else if (req->element_size < dev_info->inline_data_offset) {
		AR_LOG_ARFW_DEV_ERR(
			dev, AR_LOG_CREATE_QUEUE,
			"Element size should be larger than inline data offset [arfw queue: 0x%x%s0x%x]",
			req->hlos_endpoint_id,
			req->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" :
									    "<-",
			req->fw_endpoint_id);
		return -EINVAL;
	}

	// get a unique id for this queue
	id = ar_atomic_fetch_add(&queue_id_counter, 1, AR_MEMORY_ORDER_RELAXED);

	AR_LOG_ARFW_INFO(
		AR_LOG_CREATE_QUEUE,
		"Create queue [arfw queue: 0x%x%s0x%x, generic layer id: %d, client: %p, elem size: %u, depth %u, segment size: %u, mirror: %d, pid: %d]",
		req->hlos_endpoint_id,
		req->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-",
		req->fw_endpoint_id, id, client, req->element_size, req->depth,
		req->queue_segment.size, req->queue_mirror, req->pid);

	if (debugfs_initialized()) {
		err = arfw_queue_debug_samplers_create(dev, client, req);
		if (err)
			return err;
	}

	int_queue = kzalloc(sizeof(*int_queue), GFP_KERNEL);
	if (!int_queue) {
		AR_LOG_ARFW_DEV_ERR(
			dev, AR_LOG_CREATE_QUEUE,
			"Can't allocate memory [arfw queue: 0x%x%s0x%x]",
			req->hlos_endpoint_id,
			req->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" :
									    "<-",
			req->fw_endpoint_id);
		return -ENOMEM;
	}

	err = lifetime_lock_construction_acquire(&client->lt_lock);
	if (err)
		goto free_int_queue;

	// initialize event fields
	spin_lock_init(&int_queue->locked_events.lock);
	mutex_init(&int_queue->serial_work_lock);
	INIT_LIST_HEAD(&int_queue->locked_events.events);
	init_waitqueue_head(&int_queue->event_wq);

	// initialize region fields
	spin_lock_init(&int_queue->locked_regions.lock);
	idr_init(&int_queue->locked_regions.region_idr);

	// save unique id for this queue
	int_queue->id = id;

	// inherit relevant fields from base_client
	int_queue->driver_ops = driver_ops;
	int_queue->dev = dev;
	int_queue->queue_device_info = *dev_info;
	int_queue->base_context = base_context;

	// client queue stores relevant information from create request
	int_queue->hlos_endpoint_id = req->hlos_endpoint_id;
	int_queue->fw_endpoint_id = req->fw_endpoint_id;
	int_queue->direction = req->queue_direction;
	int_queue->mirror = req->queue_mirror;

	// default queue shutdown reason is client (userspace) crash
	int_queue->shutdown_reason = AR_QUEUE_SHUTDOWN_CRASH;

	/**
	 * Assign client queue to client BEFORE firing off operations that will try to access
	 * arfw_client_queue, such as any callbacks in arfw_driver_ops.h
	 */
	client->queue = int_queue;

	// store debug information to track the state of the queue
	client->debug.pid = req->pid;
	rcu_read_lock();
	task = pid_task(find_vpid(req->pid), PIDTYPE_PID);
	if (task)
		strncpy(client->debug.comm, task->comm, TASK_COMM_LEN);
	else
		strncpy(client->debug.comm, "<unknown>", TASK_COMM_LEN);
	rcu_read_unlock();

	// initialize queue_meta fields based on request input
	err = ar_fw_queue_meta_init(
		req->depth, req->element_size,
		req->queue_direction == AR_QUEUE_HLOS_TO_FW ? SEND : RECEIVE,
		cache_line_size(), PAGE_SIZE, &int_queue->queue_meta);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_CREATE_QUEUE,
				      "Failed to init queue meta", "err: %d",
				      err);
		err = -EINVAL;
		goto unlock_ll_err;
	}

	if ((int_queue->queue_meta.total_mem_size -
	     int_queue->queue_meta.data_size) != req->queue_segment.size) {
		AR_LOG_ARFW_QUEUE_ERR(
			client->queue, AR_LOG_CREATE_QUEUE,
			"Unexpected queue segment sizes for the kernel memory allocation.",
			"expected: %llu actual: %d",
			int_queue->queue_meta.total_mem_size -
				int_queue->queue_meta.data_size,
			req->queue_segment.size);
		err = -EINVAL;
		goto unlock_ll_err;
	}

	// map the queue for usage by driver code
	err = arfw_map_queue(client, &req->queue_segment);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_CREATE_QUEUE,
				      "Failed to map queue.", "err: %d", err);
		err = -EFAULT;
		goto unlock_ll_err;
	}

	// initialize driver side creator data
	ar_fw_queue_creator_data_driver_init(&creator_data,
					     arfw_queue_handle_request, client);

	// Allocate queue data. This should be allocated before ar_fw_queue_create call to initialize
	// metadata.
	params.queue_direction = req->queue_direction;
	params.element_size = req->element_size;
	params.depth = req->depth;
	params.hlos_endpoint_id = req->hlos_endpoint_id;
	params.fw_endpoint_id = req->fw_endpoint_id;
	params.queue_mirror = req->queue_mirror;
	int_queue->queue_data =
		driver_ops->handle_queue_data_alloc(base_context, &params);
	if (!int_queue->queue_data) {
		err = -ENOMEM;
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_CREATE_QUEUE,
				      "Failed to allocate queue data.",
				      "err: %d", err);
		goto unmap_queue_segment;
	}

	AR_ASSERT(int_queue->queue_mapping.type == ARFW_CLIENT_REGION_DIRECT);
	err = ar_fw_queue_create(
		&int_queue->queue_meta,
		(uintptr_t)int_queue->queue_mapping.direct.base_ptr,
		ar_fw_queue_meta_size(&int_queue->queue_meta),
		(uintptr_t)int_queue->queue_data->ptr,
		int_queue->queue_data->size, &creator_data,
		&int_queue->arfw_queue);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_CREATE_QUEUE,
				      "Failed to create queue.", "err: %d",
				      err);
		err = -EINVAL;
		goto free_queue_data;
	}

	// if recv queue, reserve entire queue. This should move the read index of the queue to depth-1
	if (req->queue_direction == AR_QUEUE_FW_TO_HLOS) {
		read_index = reserve_int_queue(int_queue);
		if (read_index < 0) {
			err = read_index;
			goto free_queue_data;
		}

		// assert our assumption. If this trips, refactor handle_queue_create to pass in read_index
		AR_ASSERT(read_index == req->depth - 1);
	}

	// Fill in queue create params to send to HW specific impl
	params.queue_location = int_queue->queue_data->ptr;
	params.initial_read_index = read_index;

	// pass in arfw_client_queue to external functions
	handle = driver_ops->handle_queue_create(client, base_context, &params,
						 int_queue->queue_data,
						 &int_queue->queue_context);

	// in cases when a driver specific handle is assigned - we pass it back
	if (handle < 0) {
		err = handle;
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_CREATE_QUEUE,
				      "Failed to create HW driver queue.",
				      "err: %d", err);
		goto free_queue_data;
	}

	client->debug.creation_ts = ktime_get_real();

	// Mark the queue as ready outside construction lock
	lifetime_lock_construction_release(&client->lt_lock);
	driver_ops->handle_notify_queue_ready(client->queue->queue_context);
	AR_LOG_ARFW_QUEUE_INFO(client->queue, AR_LOG_CREATE_QUEUE,
			       "Create client queue OK", "id %d",
			       client->queue->id);

	if (debugfs_initialized())
		arfw_debug_queue_add(client);

	return handle;

free_queue_data:
	driver_ops->handle_queue_data_free(base_context, int_queue->queue_data);
	kref_put(&int_queue->queue_data->refcnt, arfw_mem_region_free);
unmap_queue_segment:
	arfw_unmap_queue(client);
unlock_ll_err:
	lifetime_lock_construction_release_err(&client->lt_lock);
free_int_queue:
	kfree(int_queue);
	// client->queue is set to int_queue, so NULL it once it is freed.
	client->queue = NULL;

	return err;
}

int arfw_queue_destroy(struct arfw_client_queue *client, void *base_context)
{
	int err, region_id;
	struct arfw_client_region *region;

	AR_ASSERT(client);
	AR_ASSERT(client->queue);
	AR_ASSERT(lifetime_lock_is_torn_down(&client->lt_lock));

	if (debugfs_initialized()) {
		// Since arfw queue debugfs relies on the queue data, it should be removed
		// first before starting the clean up process.
		arfw_debug_queue_remove(client);
	}

	err = client->queue->driver_ops->handle_queue_destroy(
		base_context, client->queue->queue_context);
	if (err) {
		// Normally this should not happen, but if we failed to notify the firmware
		// we just assume it's dead or not-functional and clean up resources
		// on our end in kernel.
		AR_LOG_ARFW_QUEUE_ERR(
			client->queue, AR_LOG_DESTROY_QUEUE,
			"Failed to destroy HW driver queue, cleanup locally",
			"err: %d", err);
	}

	// Hardware context is removed, we should not use it anymore.
	client->queue->queue_context = NULL;

	// Unpin and unmap all remaining regions even if referenced.
	// Firmware should've terminated all scheduled DMA before ack-ing queue deletion.
	// If not, it's probably dead anyways, let's clean it up.
	idr_for_each_entry(&client->queue->locked_regions.region_idr, region,
			   region_id) {
		AR_LOG_ARFW_QUEUE_DBG(
			client->queue, AR_LOG_UNREG_MEM,
			"Unregister remaining region complete (client release)",
			"id %d ref count %d dma addr %p", region->id,
			region->references,
			(void *)region->mapping.dma_region_addr);
		if (region->location == EXTERNAL) {
			arfw_dma_unmap_region(client, region->id,
					      &region->mapping);
			arfw_unmap_region(&region->mapping);
		}
		kfree(region);
	}

	idr_destroy(&client->queue->locked_regions.region_idr);
	ar_fw_queue_destroy(&client->queue->arfw_queue);

	client->queue->driver_ops->handle_queue_data_free(
		client->queue->base_context, client->queue->queue_data);

	arfw_unmap_queue(client);

	AR_LOG_ARFW_QUEUE_INFO(client->queue, AR_LOG_DESTROY_QUEUE,
			       "Destroy client queue OK", "id %d",
			       client->queue->id);

	return 0;
}

int arfw_queue_event_enqueue(struct arfw_client_queue *client,
			     struct arfw_client_event *event_in)
{
	int err = 0;
	unsigned long flags = 0;
	struct arfw_client_event *event;
	struct arfw_int_client_queue *client_queue;

	AR_ASSERT(client);
	AR_ASSERT(event_in);
	client_queue = client->queue;
	AR_ASSERT(client_queue);

	event = kzalloc(sizeof(struct arfw_client_event), GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	*event = *event_in;

	// acquire lifetime lock before work
	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		goto free_event;

	// secondary lock for events
	spin_lock_irqsave(&client_queue->locked_events.lock, flags);

	/**
	 * Add the event to the event list.
	 * Pend required events have higher priority, and so go in the front of the queue.
	 * Read complete events only appear once, so skip adding if one is already present
	 * All other events are appended to the end of the queue.
	 */
	if (event_in->type == AR_QUEUE_PEND_REQUIRED)
		list_add(&event->list_node,
			 &client_queue->locked_events.events);
	else if (event_in->type == AR_QUEUE_READ_COMPLETE &&
		 client_queue->locked_events.has_read_event)
		kfree(event);
	else
		list_add_tail(&event->list_node,
			      &client_queue->locked_events.events);

	if (event_in->type == AR_QUEUE_READ_COMPLETE)
		client_queue->locked_events.has_read_event = true;

	// unlock event lock and notify a waiter
	spin_unlock_irqrestore(&client_queue->locked_events.lock, flags);
	wake_up_interruptible(&client_queue->event_wq);

	// release lifetime lock
	lifetime_lock_release(&client->lt_lock);

	return 0;

free_event:
	kfree(event);

	return err;
}

int arfw_queue_event_dequeue(struct arfw_client_queue *client,
			     struct arfw_client_event_batch *events_out)
{
	int err, count = 0;
	unsigned long flags;
	struct arfw_client_event *event;
	struct arfw_int_client_queue *client_queue;

	AR_ASSERT(client);
	AR_ASSERT(events_out);
	client_queue = client->queue;
	AR_ASSERT(client_queue);

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		return err;

	spin_lock_irqsave(&client_queue->locked_events.lock, flags);

	while (!list_empty(&client_queue->locked_events.events) &&
	       count < AR_QUEUE_EVENT_BATCH_MAX) {
		event = list_first_entry(&client_queue->locked_events.events,
					 struct arfw_client_event, list_node);
		if (event && event->type == AR_QUEUE_READ_COMPLETE)
			client_queue->locked_events.has_read_event = false;
		list_del(&event->list_node);
		events_out->events[count++] = *event;
		kfree(event);
	}

	spin_unlock_irqrestore(&client_queue->locked_events.lock, flags);
	lifetime_lock_release(&client->lt_lock);

	events_out->size = count;
	return count ? count : -ENOMSG;
}

int arfw_queue_event_wait_dequeue(struct arfw_client_queue *client,
				  struct arfw_client_event_batch *events_out)
{
	int err;

	AR_ASSERT(client);
	AR_ASSERT(events_out);

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		return err;

	wait_event_interruptible(client->queue->event_wq,
				 (err = arfw_queue_event_dequeue(
					  client, events_out)) != -ENOMSG);

	lifetime_lock_release(&client->lt_lock);

	return err;
}

int arfw_queue_event_ready(struct arfw_client_queue *client)
{
	int err;
	unsigned long flags;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		return err;

	// do minimal work while lock is held
	spin_lock_irqsave(&client->queue->locked_events.lock, flags);
	if (!list_empty(&client->queue->locked_events.events))
		err = 1;

	spin_unlock_irqrestore(&client->queue->locked_events.lock, flags);
	lifetime_lock_release(&client->lt_lock);
	return err;
}

void arfw_queue_event_cleanup(struct arfw_client_queue *client)
{
	struct arfw_client_event *event;
	struct arfw_int_client_queue *queue;

	/* It is expected that clean up is called only from the release()
	 * handler. In this case the lifetime lock should be already torn
	 * down. Because of it, there is no need to acquire queue or events
	 * locks at this point.
	 * If queue drain is required outside of the release() call, then
	 * the proper locks are required.
	 */
	AR_ASSERT(lifetime_lock_is_torn_down(&client->lt_lock));

	AR_ASSERT(client);
	queue = client->queue;
	AR_ASSERT(queue);

	while (!list_empty(&queue->locked_events.events)) {
		event = list_first_entry(&queue->locked_events.events,
					 struct arfw_client_event, list_node);
		list_del(&event->list_node);
		kfree(event);
	}
}

int arfw_queue_register_region(struct arfw_client_queue *client,
			       struct ar_mem_region *region, bool managed)
{
	int err, id;
	unsigned long flags = 0;
	struct arfw_client_region *registered_region = NULL;
	struct arfw_int_client_queue *client_queue;
	size_t total_size, region_size = 0, segment_idx;

	AR_ASSERT(client);
	AR_ASSERT(client->queue);
	client_queue = client->queue;
	AR_ASSERT(region);

	registered_region = kzalloc(sizeof(*registered_region), GFP_KERNEL);
	if (!registered_region)
		return -ENOMEM;

	registered_region->location = EXTERNAL;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		goto free_registered_region;

	// Calculate the total size of the region without mapping/pinning pages.
	// Make sure each chunk is page aligned, since that is how we map/pin them as well.
	for (segment_idx = 0; segment_idx < region->segment_count;
	     segment_idx++)
		region_size +=
			ALIGN(region->segments[segment_idx].size, PAGE_SIZE);

	// We should be fine to check here since we serialize registration.
	// Concurrently we could go down in total size if some regions were inflight,
	// but released. But we care only about not going over the limit.
	spin_lock_irqsave(&client_queue->locked_regions.lock, flags);
	total_size = client_queue->locked_regions.total_size + region_size;
	if (total_size > queue_total_region_size_max)
		err = -ENOMEM;
	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);

	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(
			client_queue, AR_LOG_REG_MEM,
			"Region too big, overflows the total limit per queue",
			"err %d, limit 0x%zx, total 0x%zx", err,
			queue_total_region_size_max, total_size);
		goto unlock_ll;
	}

	// We need to pre-populate some fields for mapping to succeed.
	// This has to happen before we attemt mapping each region.
	registered_region->mapping.hw_dev = client_queue->hw_dev;
	registered_region->mapping.dma_direction =
		client_queue->queue_meta.queue_direction == SEND ?
			      DMA_TO_DEVICE :
			      DMA_FROM_DEVICE;

	if (!managed) {
		// Optimistically attempt to use DMA BUF, if lookup fails (fast)
		// we assume it's a region with direct mapping. Only do this if
		// the region/buffer was not internally managed.
		registered_region->mapping.type = ARFW_CLIENT_REGION_DMABUF;
		err = arfw_map_region(region, &registered_region->mapping,
				      /* map_kernel = */ false);
	}
	if (managed || err) {
		// Second time's the charm... Otherwise just fail here...
		// Also the default for internal managed regions as we only support ashmem.
		registered_region->mapping.type = ARFW_CLIENT_REGION_DIRECT;
		err = arfw_map_region(region, &registered_region->mapping,
				      /* map_kernel = */ false);
	}
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(client_queue, AR_LOG_REG_MEM,
				      "Failed to map region", "err %d", err);
		goto unlock_ll;
	}

	// these memory regions are fully dma mapped
	registered_region->mapping.dma_region_offset = 0;
	registered_region->mapping.dma_region_size =
		registered_region->mapping.region_size;

	// regions will be ref counted for dma usage
	registered_region->released = false;
	registered_region->references = 0;

	idr_preload(GFP_KERNEL);
	spin_lock_irqsave(&client_queue->locked_regions.lock, flags);

	// allocate region id and associate it with registered region
	id = idr_alloc_cyclic(&client_queue->locked_regions.region_idr,
			      registered_region, ARFW_REGION_ID_MIN,
			      ARFW_REGION_ID_MAX, GFP_NOWAIT);
	if (id < 0) {
		AR_LOG_ARFW_QUEUE_ERR(client_queue, AR_LOG_REG_MEM,
				      "Failed to allocate region", "err %d",
				      id);
		err = id;
		spin_unlock_irqrestore(&client_queue->locked_regions.lock,
				       flags);
		idr_preload_end();
		goto unmap_region;
	}
	client_queue->locked_regions.num_regions++;
	registered_region->id = (uint16_t)id;

	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);
	idr_preload_end();

	err = arfw_dma_map_region(client, registered_region->id,
				  &registered_region->mapping);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(client_queue, AR_LOG_REG_MEM,
				      "Failed to dma map region", "err %d",
				      err);
		goto remove_region;
	}

	spin_lock_irqsave(&client_queue->locked_regions.lock, flags);
	client_queue->locked_regions.total_size +=
		registered_region->mapping.region_size;
	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);

	lifetime_lock_release(&client->lt_lock);
	AR_LOG_ARFW_QUEUE_DBG(
		client->queue, AR_LOG_REG_MEM, "Registered region successfully",
		"id %d dma addr %p size %zu", id,
		(void *)registered_region->mapping.dma_region_addr,
		registered_region->mapping.region_size);

	return id;

remove_region:
	spin_lock_irqsave(&client_queue->locked_regions.lock, flags);
	idr_remove(&client_queue->locked_regions.region_idr,
		   registered_region->id);
	client_queue->locked_regions.num_regions--;
	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);
unmap_region:
	arfw_unmap_region(&registered_region->mapping);
unlock_ll:
	lifetime_lock_release(&client->lt_lock);
free_registered_region:
	kfree(registered_region);
	return err;
}

int arfw_queue_register_aperture_region(struct arfw_client_queue *client,
					void *va, dma_addr_t dma_addr,
					size_t size)
{
	int err, id;
	unsigned long flags = 0;
	struct arfw_client_region *registered_region = NULL;
	struct arfw_int_client_queue *client_queue = client->queue;

	AR_ASSERT(client_queue);
	AR_ASSERT(va);
	AR_ASSERT(size);

	registered_region = kzalloc(sizeof(*registered_region), GFP_KERNEL);
	if (!registered_region)
		return -ENOMEM;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		goto free_registered_region;

	/**
	 * TODO(T208123666, T208123723): Fix me.
	 * The following is a hack since the generic layer needs to know about DMA addresses and manages
	 * DMA map + unmap details. Once we refactor (T208123666), we can used a shared impl and add
	 * aperture buffer tracking for debugging. (T208123723)
	 */
	registered_region->location = APERTURE;
	registered_region->mapping.type = ARFW_CLIENT_REGION_DIRECT;
	registered_region->mapping.direct.base_ptr = va;
	registered_region->mapping.region_size = size;
	registered_region->mapping.dma_direction =
		client_queue->queue_meta.queue_direction == SEND ?
			      DMA_TO_DEVICE :
			      DMA_FROM_DEVICE;

	// these memory regions are already fully dma mapped
	registered_region->mapping.dma_region_addr = dma_addr;
	registered_region->mapping.dma_region_offset = 0;
	registered_region->mapping.dma_region_size = size;

	// regions will be ref counted for dma usage
	registered_region->released = false;
	registered_region->references = 0;

	idr_preload(GFP_KERNEL);
	spin_lock_irqsave(&client_queue->locked_regions.lock, flags);

	// allocate region id and associate it with registered region
	id = idr_alloc_cyclic(&client_queue->locked_regions.region_idr,
			      registered_region, ARFW_REGION_ID_MIN,
			      ARFW_REGION_ID_MAX, GFP_NOWAIT);
	if (id < 0) {
		AR_LOG_ARFW_QUEUE_ERR(client_queue, AR_LOG_REG_MEM,
				      "Failed to allocate aperture region id",
				      "err %d", id);
		err = id;
		goto unlock_region;
	}

	client_queue->locked_regions.num_regions++;
	registered_region->id = (uint16_t)id;

	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);
	idr_preload_end();

	lifetime_lock_release(&client->lt_lock);

	AR_LOG_ARFW_QUEUE_DBG(
		client->queue, AR_LOG_REG_MEM,
		"Registered aperture region successfully",
		"id %d kern addr %p dma addr %p size %zu", id,
		registered_region->mapping.direct.base_ptr,
		(void *)registered_region->mapping.dma_region_addr,
		registered_region->mapping.region_size);

	return id;

unlock_region:
	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);
	idr_preload_end();
	lifetime_lock_release(&client->lt_lock);
free_registered_region:
	kfree(registered_region);
	return err;
}

int arfw_queue_unregister_region(struct arfw_client_queue *client,
				 int region_id)
{
	int err;
	unsigned long flags = 0;
	struct arfw_client_region *registered_region = NULL;
	struct arfw_int_client_queue *client_queue = client->queue;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		return err;

	spin_lock_irqsave(&client_queue->locked_regions.lock, flags);
	registered_region =
		idr_find(&client_queue->locked_regions.region_idr, region_id);
	if (registered_region == NULL) {
		err = -ENOENT;
		goto unlock_region;
	}

	// if the region is currently active (potential dma) mark it and leave
	if (registered_region->references > 0) {
		registered_region->released = true;
		AR_LOG_ARFW_QUEUE_DBG(
			client->queue, AR_LOG_UNREG_MEM,
			"Unregister region deferred, region still active (potential dma)",
			"id %d ref count %d", registered_region->id,
			registered_region->references);
		goto unlock_region;
	}

	idr_remove(&client_queue->locked_regions.region_idr, region_id);
	client_queue->locked_regions.num_regions--;
	if (registered_region->location == EXTERNAL)
		client_queue->locked_regions.total_size -=
			registered_region->mapping.region_size;
	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);

	AR_LOG_ARFW_QUEUE_DBG(
		client->queue, AR_LOG_UNREG_MEM, "Unregister region complete",
		"id %d dma addr %p", registered_region->id,
		(void *)registered_region->mapping.dma_region_addr);

	if (registered_region->location == EXTERNAL) {
		arfw_dma_unmap_region(client, registered_region->id,
				      &registered_region->mapping);
		arfw_unmap_region(&registered_region->mapping);
	}

	kfree(registered_region);

	goto unlock_ll;

unlock_region:
	spin_unlock_irqrestore(&client_queue->locked_regions.lock, flags);
unlock_ll:
	lifetime_lock_release(&client->lt_lock);

	return err;
}

int arfw_queue_ref_region(struct arfw_client_queue *client, uint16_t region_id)
{
	int err = 0;
	struct arfw_client_region *region = NULL;
	struct arfw_int_client_queue *queue = client->queue;

	region = idr_find(&queue->locked_regions.region_idr, region_id);
	if (region == NULL) {
		err = -ENOENT;
		goto exit;
	}

	region->references++;
	AR_LOG_ARFW_QUEUE_DBG(queue, AR_LOG_REF_MEM,
			      "Reference region complete",
			      "id %d, new ref count %d", region->id,
			      region->references);

exit:
	return err;
}

int arfw_queue_deref_region(struct arfw_client_queue *client,
			    uint16_t region_id)
{
	int err = 0;
	unsigned long flags = 0;
	bool cleanup_work_required = false;
	struct arfw_client_region *region = NULL;
	struct arfw_int_client_queue *queue = client->queue;

	spin_lock_irqsave(&queue->locked_regions.lock, flags);
	region = idr_find(&queue->locked_regions.region_idr, region_id);
	if (region == NULL) {
		err = -ENOENT;
		goto unlock;
	}

	if (region->references > 0) {
		region->references--;
		AR_LOG_ARFW_QUEUE_DBG(queue, AR_LOG_DEREF_MEM,
				      "Dereference region complete",
				      "id %d, new ref count %d", region->id,
				      region->references);
	}

	// clean it up if the region was previously released and not active
	if (region->released && region->references == 0) {
		idr_remove(&queue->locked_regions.region_idr, region_id);
		queue->locked_regions.num_regions--;
		if (region->location == EXTERNAL)
			queue->locked_regions.total_size -=
				region->mapping.region_size;

		cleanup_work_required = true;
	}

unlock:
	spin_unlock_irqrestore(&queue->locked_regions.lock, flags);

	if (cleanup_work_required) {
		AR_LOG_ARFW_QUEUE_DBG(
			queue, AR_LOG_DEREF_MEM,
			"Unregister region complete (previously released, no refs)",
			"id %d dma addr %p", region->id,
			(void *)region->mapping.dma_region_addr);
		if (region->location == EXTERNAL) {
			arfw_dma_unmap_region(client, region->id,
					      &region->mapping);
			arfw_unmap_region(&region->mapping);
		}
		kfree(region);
	}

	return err;
}
