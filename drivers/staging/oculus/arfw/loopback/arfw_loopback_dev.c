// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_loopback_dev.c
 *
 * @brief Implementation of the arfw-loopback layer
 *
 * @details
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include "arfw_loopback_dev.h"
#include "arfw_loopback_int.h"
#include "arfw_loopback_ctl.h"

#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <ar_device.h>
#include <arfw_log.h>
#include <ar_utils.h>
#include <ar_future.h>

#include <ar_fw_message.h>

#define ARFW_LOOPBACK_TRANSPORT_HEADER_SIZE 88
#define ARFW_LOOPBACK_MAX_SEND_RINGS 24
#define ARFW_LOOPBACK_MAX_RECEIVE_RINGS 24
// Needs to be at least 62 for IntralinkApAvoTest.BandwidthStressTest
#define ARFW_LOOPBACK_MAX_PEND_BUFFS 64

#define ARFW_LOOPBACK_QUEUE_HANDLE_MIN 1
#define ARFW_LOOPBACK_QUEUE_HANDLE_MAX UINT16_MAX

#define ARFW_TRACKING_REPLY_BIT (1 << 6)

static struct arfw_loopback_ctl_queue_context *arfw_loopback_dev_find_queue(
	struct arfw_loopback_ctl_device *dev, ar_endpoint_id_t hlos_endpoint_id,
	ar_endpoint_id_t fw_endpoint_id,
	enum ar_queue_direction queue_direction, bool queue_mirror)
{
	struct arfw_loopback_ctl_queue_context *queue = NULL;
	unsigned long flags;
	int handle = 0;

	spin_lock_irqsave(&dev->locked_queues.lock, flags);

	idr_for_each_entry(&dev->locked_queues.queues, queue, handle)
		if (queue->info.hlos_endpoint == hlos_endpoint_id &&
		    queue->info.fw_endpoint == fw_endpoint_id &&
		    queue->info.direction == queue_direction &&
		    queue->info.mirror == queue_mirror)
			break;

	spin_unlock_irqrestore(&dev->locked_queues.lock, flags);

	if (!queue || !handle)
		return NULL;

	return queue;
}

static struct arfw_loopback_ctl_queue_context *
arfw_loopback_dev_find_mirror_queue(struct arfw_loopback_ctl_device *dev,
				    ar_endpoint_id_t hlos_endpoint_id,
				    ar_endpoint_id_t fw_endpoint_id,
				    enum ar_queue_direction queue_direction,
				    bool queue_mirror)
{
	enum ar_queue_direction direction;

	// Mirror the direction to find the queue: for send find recv and other way around.
	direction = (queue_direction == AR_QUEUE_HLOS_TO_FW) ?
				  AR_QUEUE_FW_TO_HLOS :
				  AR_QUEUE_HLOS_TO_FW;

	return arfw_loopback_dev_find_queue(dev, hlos_endpoint_id,
					    fw_endpoint_id, direction,
					    !queue_mirror);
}

bool arfw_loopback_dev_handle_send_queue_request(
	void *queue_context, const struct arfw_io_request *req, bool *consumed)
{
	int err;
	ar_firmware_message_header_t *ar_ipc_header;
	const ar_firmware_msg_sg_elem_t *sg_elem;
	ar_firmware_msg_sg_elem_t sg_temp;
	ar_firmware_msg_sg_recv_t *user_sg_elem;
	arfw_client_queue_t mirror_arfw_queue;
	struct arfw_io_request out_req = *req;
	struct arfw_loopback_ctl_queue_context *queue = queue_context;
	struct arfw_loopback_ctl_queue_context *mirror_queue;
	int buffer_num;

	AR_ASSERT(req);
	AR_ASSERT(queue);

	// To trust the queue context structure the lock should be acquired.
	err = lifetime_lock_acquire(&queue->lock);
	if (err) {
		AR_LOG_USER_QUEUE_ERR(
			queue, AR_LOG_WRITE,
			"Can't lock arfw-loopback send queue [err: %d]", err);
		return false;
	}
	mirror_queue = queue->mirror_queue;
	AR_ASSERT(mirror_queue);
	err = lifetime_lock_acquire(&mirror_queue->lock);
	if (err) {
		AR_LOG_USER_QUEUE_ERR(
			mirror_queue, AR_LOG_WRITE,
			"Can't lock arfw-loopback send queue mirror [err: %d]",
			err);
		goto fail_unlock_queue;
	}

	mirror_arfw_queue = mirror_queue->arfw_queue;
	AR_ASSERT(mirror_arfw_queue);

	ar_ipc_header = out_req.data;

	if (!data_location_external_to_queue(ar_ipc_header->data_location))
		goto produce;

	sg_elem = arfw_sg_list_of_msg(ar_ipc_header,
				      ARFW_LOOPBACK_INLINE_DATA_OFFSET);
	user_sg_elem = arfw_sg_list_of_msg(ar_ipc_header,
					   ARFW_LOOPBACK_INLINE_DATA_OFFSET);
	buffer_num = arfw_sg_buf_count_of_msg(ar_ipc_header);

	AR_ASSERT(
		ar_ipc_header->data_size <=
		(queue->info.element_size - ARFW_LOOPBACK_INLINE_DATA_OFFSET));

	// convert from ar_firmware_msg_sg_elem_t to ar_firmware_msg_sg_recv_t
	while (buffer_num) {
		sg_temp = *sg_elem;

		user_sg_elem->roundtrip.id = sg_temp.roundtrip.id;
		user_sg_elem->size = sg_temp.size;

		// mirror queues send pend ids, so mark pend released
		if (queue->info.mirror) {
			err = queue->dev->client_ops
				      ->handle_client_payload_pend_released(
					      mirror_arfw_queue,
					      sg_temp.roundtrip.id);
			if (err)
				AR_LOG_USER_QUEUE_ERR(
					queue, AR_LOG_WRITE,
					"Can't release pend [index: %d, pend_id %u, mem_region_id %u err: %d]",
					out_req.index,
					sg_temp.roundtrip.pend_id,
					sg_temp.roundtrip.region_id, err);
		}

		buffer_num--;
		sg_elem++;
		user_sg_elem++;
	}

produce:
	err = queue->dev->client_ops->handle_client_queue_produce_request(
		mirror_arfw_queue, &out_req);
	if (err) {
		AR_LOG_USER_QUEUE_ERR(queue, AR_LOG_WRITE,
				      "Can't produce [index: %d, err: %d]",
				      out_req.index, err);
		goto fail_unlock_both;
	}

	AR_LOG_USER_QUEUE_DBG(
		queue, AR_LOG_WRITE,
		"Send Ring entry [handle %d, index %d, msg_id: 0x%x, seq: %u, tracking: %d]",
		queue->info.handle, out_req.index, ar_ipc_header->msg_id,
		ar_ipc_header->sequence_id, ar_ipc_header->tracking_id);

	*consumed = false;
	lifetime_lock_release(&mirror_queue->lock);
	lifetime_lock_release(&queue->lock);

	return true;

fail_unlock_both:
	lifetime_lock_release(&mirror_queue->lock);
fail_unlock_queue:
	lifetime_lock_release(&queue->lock);
	return false;
}

void arfw_loopback_dev_handle_rcv_queue_consume(arfw_client_queue_t arfw_queue,
						void *queue_context)
{
	int ret = 0;
	struct arfw_io_request out_req = {};
	struct arfw_loopback_ctl_queue_context *queue = queue_context;
	struct arfw_loopback_ctl_queue_context *mirror_queue;
	uint32_t index = UINT_MAX;

	AR_ASSERT(queue);

	// To trust the queue context structure the lock should be acquired.
	ret = lifetime_lock_acquire(&queue->lock);
	if (ret) {
		AR_LOG_USER_QUEUE_ERR(
			queue, AR_LOG_READ,
			"Can't lock arfw-loopback recv queue [err: %d]", ret);
		return;
	}
	mirror_queue = queue->mirror_queue;
	AR_ASSERT(mirror_queue);
	ret = lifetime_lock_acquire(&mirror_queue->lock);
	if (ret) {
		AR_LOG_USER_QUEUE_ERR(
			mirror_queue, AR_LOG_READ,
			"Can't lock arfw-loopback recv queue mirror [err: %d]",
			ret);
		lifetime_lock_release(&queue->lock);
		return;
	}

	AR_LOG_USER_QUEUE_DBG(queue, AR_LOG_READ,
			      "Handle rcv consume [handle: %d]",
			      queue->info.handle);

	while (ret == 0) {
		ret = queue->dev->client_ops
			      ->handle_client_queue_reserve_request(
				      queue->arfw_queue, &out_req);
		if (ret)
			break;

		AR_LOG_USER_QUEUE_DBG(queue, AR_LOG_READ,
				      "Consume io req [handle: %d, idx: %d]",
				      queue->info.handle, out_req.index);

		index = out_req.index;

		// make current slot ready to produce into again
		ret = queue->dev->client_ops->handle_client_queue_index_consumed(
			mirror_queue->arfw_queue, index);
		if (ret) {
			AR_LOG_USER_QUEUE_ERR(
				queue, AR_LOG_READ,
				"Can't mark consumed [index: %d, err: %d]",
				index, ret);
		}
	}

	if (index != UINT_MAX) {
		AR_LOG_USER_QUEUE_DBG(queue, AR_LOG_READ,
				      "Update rd_idx [new fw rd_idx: %u]",
				      index);
	} else {
		AR_LOG_USER_QUEUE_DBG(queue, AR_LOG_READ,
				      "No slots to reserve");
	}

	lifetime_lock_release(&mirror_queue->lock);
	lifetime_lock_release(&queue->lock);
}

struct arfw_queue_mem_region *arfw_loopback_dev_handle_queue_data_alloc(
	void *base_context,
	const struct arfw_client_queue_create_params *queue_params)
{
	struct arfw_loopback_ctl_device *dev =
		(struct arfw_loopback_ctl_device *)base_context;
	struct arfw_queue_mem_region *queue_data;
	struct arfw_loopback_ctl_queue_context *origin_queue = NULL;

	AR_ASSERT(dev);
	AR_ASSERT(queue_params);

	queue_data = kzalloc(sizeof(*queue_data), GFP_KERNEL);
	if (!queue_data) {
		AR_LOG_USER_DEV_ERR(&dev->dev, AR_LOG_CREATE_QUEUE,
				    "Can't allocate queue data memory");
		goto fail_alloc;
	}
	queue_data->context = NULL;
	if (queue_params->queue_mirror) {
		origin_queue = arfw_loopback_dev_find_mirror_queue(
			dev, queue_params->hlos_endpoint_id,
			queue_params->fw_endpoint_id,
			queue_params->queue_direction,
			queue_params->queue_mirror);
		if (!origin_queue) {
			AR_LOG_USER_DEV_ERR(
				&dev->dev, AR_LOG_CREATE_QUEUE,
				"No original queue entry found, err = %d.",
				-ENOENT);
			goto fail_data_alloc;
		}
		queue_data = origin_queue->queue_data;
	} else {
		queue_data->size = AR_ROUNDUP(
			queue_params->depth * queue_params->element_size,
			PAGE_SIZE);
		queue_data->ptr = vmalloc_user(queue_data->size);
	}

	if (!queue_data->ptr) {
		AR_LOG_USER_DEV_ERR(
			&dev->dev, AR_LOG_CREATE_QUEUE,
			"Can't allocate data for the queue, err = %d", -ENOMEM);
		goto fail_data_alloc;
	}
	if (queue_params->queue_mirror)
		kref_get(&queue_data->refcnt);
	else
		kref_init(&queue_data->refcnt);

	kref_get(&dev->references);
	get_device(&dev->dev);

	return queue_data;

fail_data_alloc:
	kfree(queue_data);
fail_alloc:
	return NULL;
}

void arfw_loopback_dev_handle_queue_data_free(
	void *base_context, struct arfw_queue_mem_region *queue_data)
{
	struct arfw_loopback_ctl_device *dev =
		(struct arfw_loopback_ctl_device *)base_context;

	AR_ASSERT(dev);
	AR_ASSERT(queue_data);

	put_device(&dev->dev);
	arfw_loopback_ctl_device_put(dev);
	// queue_data and queue_data->ptr should be freed outside the device specific
	// context. The generic layer decide when to clean up it, based on the last
	// reference in the user space.
}

int arfw_loopback_dev_handle_queue_create_request(
	arfw_client_queue_t arfw_queue, void *base_context,
	const struct arfw_client_queue_create_params *queue_params,
	struct arfw_queue_mem_region *queue_data, void **queue_context)
{
	struct arfw_loopback_ctl_device *dev =
		(struct arfw_loopback_ctl_device *)base_context;
	struct arfw_loopback_ctl_queue_context *queue, *origin_queue = NULL;
	unsigned long flags;
	int ret = 0, handle = 0;

	AR_ASSERT(queue_params);
	AR_ASSERT(queue_context);
	AR_ASSERT(dev);

	kref_get(&dev->references);

	// Check that the queue with the exactly same params doesn't exist.
	origin_queue = arfw_loopback_dev_find_queue(
		dev, queue_params->hlos_endpoint_id,
		queue_params->fw_endpoint_id, queue_params->queue_direction,
		queue_params->queue_mirror);
	if (origin_queue) {
		AR_LOG_USER_QUEUE_ERR(
			origin_queue, AR_LOG_CREATE_QUEUE,
			"This queue alread existed in the system. Remove it first.");
		ret = -EEXIST;
		goto free_references;
	}

	queue = devm_kzalloc(&dev->dev,
			     sizeof(struct arfw_loopback_ctl_queue_context),
			     GFP_KERNEL);
	if (!queue) {
		AR_LOG_USER_DEV_ERR(&dev->dev, AR_LOG_CREATE_QUEUE,
				    "Can't allocate queue memory");
		ret = -ENOMEM;
		goto free_references;
	}

	idr_preload(GFP_KERNEL);
	spin_lock_irqsave(&dev->locked_queues.lock, flags);
	handle = idr_alloc(&dev->locked_queues.queues, queue,
			   ARFW_LOOPBACK_QUEUE_HANDLE_MIN,
			   ARFW_LOOPBACK_QUEUE_HANDLE_MAX, GFP_NOWAIT);
	if (handle < 0) {
		AR_LOG_USER_DEV_ERR(&dev->dev, AR_LOG_CREATE_QUEUE,
				    "Failed to allocate queue idr [err: %d]",
				    handle);
		ret = handle;
	}
	spin_unlock_irqrestore(&dev->locked_queues.lock, flags);
	idr_preload_end();
	if (ret)
		goto free_queue;

	lifetime_lock_init(&queue->lock);
	ret = lifetime_lock_construction_acquire(&queue->lock);
	if (ret) {
		AR_LOG_USER_DEV_ERR(
			&dev->dev, AR_LOG_CREATE_QUEUE,
			"Couldn't construct the lifetime lock for the queue [err: %d]",
			ret);
		goto free_handle;
	}

	// Populating all original queue guts here.
	queue->info.handle = handle;
	queue->info.direction = queue_params->queue_direction;
	queue->info.element_size = queue_params->element_size;
	queue->info.depth = queue_params->depth;
	queue->info.hlos_endpoint = queue_params->hlos_endpoint_id;
	queue->info.fw_endpoint = queue_params->fw_endpoint_id;
	queue->info.mirror = queue_params->queue_mirror;
	queue->dev = dev;
	queue->arfw_queue = arfw_queue;
	queue->mapping.data_base = queue_params->queue_location;
	queue->mapping.data_size = AR_ROUNDUP(
		queue_params->depth * queue_params->element_size, PAGE_SIZE);
	queue->queue_data = queue_data;

	if (queue_params->queue_mirror) {
		origin_queue = arfw_loopback_dev_find_mirror_queue(
			dev, queue_params->hlos_endpoint_id,
			queue_params->fw_endpoint_id,
			queue_params->queue_direction,
			queue_params->queue_mirror);

		if (!origin_queue) {
			AR_LOG_USER_QUEUE_ERR(queue, AR_LOG_CREATE_QUEUE,
					      "No original queue entry found.");
			ret = -ENOENT;
			goto free_handle;
		}

		/*
		 * It shouldn't be possible that the mirror_queue pointer of the original queue is
		 * already set and we pass initial check for the queue. It is expected that we will
		 * get the -EEXIST error at the beginning of the routine.
		 */
		AR_ASSERT(!origin_queue->mirror_queue);

		queue->mirror_queue = origin_queue;
		origin_queue->mirror_queue = queue;
	}

	// At this point it's safe to release the construction locks.
	lifetime_lock_construction_release(&queue->lock);

	*queue_context = queue;

	AR_LOG_USER_QUEUE_DBG(queue, AR_LOG_CREATE_QUEUE,
			      "Handle Create Data Queue OK [handle: %d]",
			      handle);

	return handle;

free_handle:
	spin_lock_irqsave(&dev->locked_queues.lock, flags);
	idr_remove(&dev->locked_queues.queues, handle);
	spin_unlock_irqrestore(&dev->locked_queues.lock, flags);
free_queue:
	devm_kfree(&dev->dev, queue);
free_references:
	arfw_loopback_ctl_device_put(dev);
	return ret;
}

int arfw_loopback_dev_handle_queue_destroy_request(void *base_context,
						   void *queue_context)
{
	int err;
	unsigned long flags;
	struct arfw_loopback_ctl_device *dev =
		(struct arfw_loopback_ctl_device *)base_context;
	struct arfw_loopback_ctl_queue_context *queue =
		(struct arfw_loopback_ctl_queue_context *)queue_context;
	struct arfw_loopback_ctl_queue_context *mirror_queue;

	AR_ASSERT(dev);
	AR_ASSERT(queue);

	err = lifetime_lock_destruction_acquire(&queue->lock);
	if (err) {
		if (err != -ESHUTDOWN)
			AR_LOG_USER_QUEUE_ERR(
				queue, AR_LOG_DESTROY_QUEUE,
				"Failed to lock queue for shutdown [err: %d]",
				err);
		goto exit;
	}
	lifetime_lock_destruction_release(&queue->lock);

	mirror_queue = queue->mirror_queue;
	queue->mirror_queue = NULL;
	if (mirror_queue &&
	    !lifetime_lock_is_shutting_down(&mirror_queue->lock)) {
		dev->client_ops->handle_client_queue_shutdown(
			mirror_queue->arfw_queue,
			AR_QUEUE_SHUTDOWN_MIRROR_STATE);
		dev->client_ops->handle_client_queue_destroy(
			mirror_queue->arfw_queue);
	}

	spin_lock_irqsave(&dev->locked_queues.lock, flags);
	idr_remove(&dev->locked_queues.queues, queue->info.handle);
	spin_unlock_irqrestore(&dev->locked_queues.lock, flags);

	devm_kfree(&dev->dev, queue);
	arfw_loopback_ctl_device_put(dev);

	AR_LOG_USER_QUEUE_DBG(queue, AR_LOG_DESTROY_QUEUE,
			      "Handle Destroy Data Queue OK");

exit:
	return err;
}

int arfw_loopback_dev_handle_get_device_information(
	void *base_context,
	struct arfw_device_information_req *device_information)
{
	AR_ASSERT(device_information);

	device_information->transport_header_size =
		ARFW_LOOPBACK_TRANSPORT_HEADER_SIZE;
	device_information->inline_data_offset =
		ARFW_LOOPBACK_INLINE_DATA_OFFSET;
	device_information->require_contiguous_memory_for_queues = false;
	device_information->send_ring_max = ARFW_LOOPBACK_MAX_SEND_RINGS;
	device_information->rcv_ring_max = ARFW_LOOPBACK_MAX_RECEIVE_RINGS;
	device_information->rcv_ring_pend_buff_count_max =
		ARFW_LOOPBACK_MAX_PEND_BUFFS;

	return 0;
}

int arfw_loopback_dev_handle_receive_payload_pend(
	void *queue_context, const struct arfw_payload_pend_req *req)
{
	// Implementation moved to user-level
	return 0;
}

void arfw_loopback_dev_handle_notify_queue_ready(void *queue_context)
{
}

int arfw_loopback_dev_handle_queue_data_mmap(
	void *base_context, struct arfw_queue_mem_region *queue_data,
	struct vm_area_struct *vma)
{
	int ret;
	struct arfw_loopback_ctl_device *dev = base_context;

	AR_ASSERT(base_context);
	AR_ASSERT(queue_data);

	ret = remap_vmalloc_range(vma, queue_data->ptr, 0);
	if (ret) {
		AR_LOG_USER_DEV_ERR(
			&dev->dev, AR_LOG_MMAP,
			"Can't mmap queue data to the user space, error = %d",
			ret);
		return ret;
	}

	return 0;
}
