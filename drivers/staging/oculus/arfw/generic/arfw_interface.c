// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file arfw_io_interface.c
 *
 * @brief Implementation of AR Firmware IO interface.
 *
 *******************************************************************************/

#include <linux/arfw_io_interface.h>
#include <linux/arfw_types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/limits.h>
#include <linux/version.h>
#include <linux/debugfs.h>

#include <ar_common.h>
#include <ar_fw.h>
#include <ar_fw_message.h>
#include <arfw_log.h>
#include <arfw_ops.h>
#include <arfw_shim.h>
#include <lifetime_lock.h>

#include "arfw_int.h"
#include "arfw_int_map.h"
#include "arfw_int_queue.h"
#include "arfw_debug.h"
#include "arfw_loopback_map_fd.h"
#include "arfw_mem_util.h"
#include "arfw_sysfs.h"

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);

#define ARFW_SHIM_ID "arfw"

/**
 * Because SG-12 uses a different kernel than N14, we need to provide an impl
 * for close_fd. For now, just assume if close_fd doesn't exist, we are on
 * SG-12 and can use __close_fd.
 */
#ifndef close_fd
static int close_fd(unsigned fd)
{
	struct files_struct *files = current->files;
	return __close_fd(files, fd);
}
#endif

/// AR Class device fields
#define AR_CLASS_NAME "arfirmware"
int ar_dev_major;
static struct class *device_class;

struct ar_class_devices {
	struct mutex lock;
	struct list_head device_list;
};

/// list of ar firmware class registered devices
struct ar_class_devices registered_devices;

/// tracking of all queues created via the driver
static LIST_HEAD(arfw_registered_queues_list);
static DEFINE_MUTEX(arfw_registered_queues_lock);

static struct arfw_fd_context *get_fd_context(struct file *file)
{
	AR_ASSERT(file);
	AR_ASSERT(file->private_data);
	return (struct arfw_fd_context *)file->private_data;
}

static void arfw_ktype_release(struct kobject *kobj)
{
	struct arfw_char_device *device =
		container_of(kobj, struct arfw_char_device, kobj);

	AR_LOG_ARFW_INFO(AR_LOG_DEV_UNREG, "Deleted device [id: %s]",
			 device->id);

	kfree(device->id);
	kfree(device);
}

static struct kobj_type arfw_ktype = {
	.release = &arfw_ktype_release,
};

// Destroy a struct arfw_char_device, releasing the cdev and device resources
static void arfw_char_device_destroy(struct arfw_char_device *device)
{
	dev_t dev;

	dev = device->cdev.dev;
	cdev_del(&device->cdev);

	// remove sysfs nodes for this device
	remove_arfw_device_sysfs_nodes(device);

	// WARNING: this releases any devm_kzalloc'd memory
	device_destroy(device_class, dev);
	unregister_chrdev_region(dev, 1);

	kobject_put(&device->kobj);
}

static void arfw_char_device_cleanup(void)
{
	struct arfw_char_device *device = NULL, *temp = NULL;

	mutex_lock(&registered_devices.lock);

	list_for_each_entry_safe(device, temp, &registered_devices.device_list,
				 list) {
		if (device->references == 0 && device->released) {
			AR_LOG_ARFW_INFO(
				AR_LOG_DEV_UNREG,
				"Cleaning up a released device %s (offloaded)",
				device->id);
			list_del(&device->list);
			arfw_char_device_destroy(device);
		}
	}

	mutex_unlock(&registered_devices.lock);
}

enum arfw_offload_event_type { ARFW_OFFLOAD_DEVICE_CLEANUP };

struct arfw_offload_event {
	struct work_struct work;
	enum arfw_offload_event_type type;
};

static void arfw_handle_offload_event(struct work_struct *work)
{
	struct arfw_offload_event *event;

	AR_ASSERT(work);

	event = container_of(work, struct arfw_offload_event, work);
	switch (event->type) {
	case ARFW_OFFLOAD_DEVICE_CLEANUP:
		arfw_char_device_cleanup();
		break;
	default:
		AR_ASSERT(false);
	}

	kfree(event);
}

static void arfw_schedule_device_cleanup(struct arfw_char_device *device)
{
	struct arfw_offload_event *event;

	AR_ASSERT(device);
	AR_ASSERT(mutex_is_locked(&registered_devices.lock));

	event = kzalloc(sizeof(struct arfw_offload_event), GFP_KERNEL);
	if (event) {
		event->type = ARFW_OFFLOAD_DEVICE_CLEANUP;
		INIT_WORK(&event->work, arfw_handle_offload_event);
		schedule_work(&event->work);
	} else {
		AR_LOG_ARFW_ERR(
			AR_LOG_DEV_UNREG,
			"Failed to schedule a device cleanup for id %s (offloaded)",
			device->id);
	}
}

/**
 * Check if a device with device_id has been previously registered. Parameter
 * device_id must be a valid string. registered_devices.lock should be held
 * before calling this function.
 */
static struct arfw_char_device *arfw_dev_lookup(const char *device_id)
{
	struct arfw_char_device *ar_device = NULL;

	AR_ASSERT(device_id);
	AR_ASSERT(mutex_is_locked(&registered_devices.lock));

	list_for_each_entry(ar_device, &registered_devices.device_list, list) {
		// if either of these are not a valid string, it is probably better to crash.
		if (strcmp(device_id, ar_device->id) == 0)
			return ar_device;
	}

	return NULL;
}

static int check_arfw_driver_ops(const struct arfw_driver_ops *ops)
{
	if (!ops || !ops->handle_send_queue || !ops->handle_rcv_queue_consume ||
	    !ops->handle_queue_create || !ops->handle_queue_destroy ||
	    !ops->handle_get_device_information ||
	    !ops->handle_receive_payload_pend ||
	    !ops->handle_notify_queue_ready)
		return -EINVAL;

	return 0;
}

/**
 * Locks the client for serial work.
 */
static int lock_client_for_work(struct arfw_client_queue *client)
{
	int err;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err) {
		if (likely(client->queue != NULL)) {
			AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_CLIENT_LOCK,
					      "Failed to acquire lifetime lock",
					      "err %d", err);
		} else {
			AR_LOG_ARFW_ERR(
				AR_LOG_CLIENT_LOCK,
				"Failed to acquire lifetime lock. [err %d]",
				err);
		}
		return err;
	}

	mutex_lock(&client->queue->serial_work_lock);
	return 0;
}

/// get the arfw client queue, locked for serial work
static struct arfw_client_queue *
get_client_locked_for_work(struct arfw_fd_context *fd_context)
{
	int err;
	struct arfw_client_queue *client;

	client = &fd_context->client;

	err = lock_client_for_work(client);
	if (err)
		return ERR_PTR(err);

	return client;
}

/**
 * Unlocks the client after serial work is done.
 */
static void unlock_client_for_work(struct arfw_client_queue *client)
{
	mutex_unlock(&client->queue->serial_work_lock);
	lifetime_lock_release(&client->lt_lock);
}

/**
 * Handle user file open request.
 *
 * @retval 0 No error.
 *
 * @retval -ENOMEM    Failed to allocate file context.
 * @retval -EINTR     Failed to lock device, interrupted.
 * @retval -ENODEV    Failed to locate device, not registered.
 * @retval -ESHUTDOWN Device is in shutdown mode, cannot be used.
 */
static int arfw_open(struct inode *inode, struct file *file)
{
	int err;
	struct arfw_char_device *device = NULL;
	struct arfw_fd_context *fd_context = NULL;

	fd_context = kzalloc(sizeof(struct arfw_fd_context), GFP_KERNEL);
	if (!fd_context) {
		err = -ENOMEM;
		goto exit;
	}

	err = mutex_lock_interruptible(&registered_devices.lock);
	if (err)
		goto free_context;

	device = container_of(inode->i_cdev, struct arfw_char_device, cdev);

	if (arfw_dev_lookup(device->id) != device) {
		err = -ENODEV;
		goto unlock_devices;
	} else if (device->released) {
		err = -ESHUTDOWN;
		goto unlock_devices;
	}

	// inherit relevant fields from arfw char device
	fd_context->driver_ops = device->driver_ops;
	fd_context->base_context = device->base_context;
	fd_context->dev = device->hw_dev;

	// Initialize client queue lifetime lock.
	lifetime_lock_init(&fd_context->client.lt_lock);
	mutex_init(&fd_context->device_info_locked.lock);
	file->private_data = fd_context;

	device->references++;

	mutex_lock(&arfw_registered_queues_lock);
	list_add(&fd_context->client.list, &arfw_registered_queues_list);
	mutex_unlock(&arfw_registered_queues_lock);

unlock_devices:
	mutex_unlock(&registered_devices.lock);

free_context:
	if (err)
		kfree(fd_context);

exit:
	return err;
}

static void arfw_client_queue_destroy(arfw_client_queue_t client);

/**
 * Handle user file release request.
 *
 * @retval 0 No error.
 */
static int arfw_release(struct inode *inode, struct file *file)
{
	struct arfw_char_device *device;
	struct arfw_fd_context *fd_context;
	struct arfw_client_queue *client;

	AR_ASSERT(inode);
	AR_ASSERT(file);

	fd_context = get_fd_context(file);
	client = &fd_context->client;

	mutex_lock(&arfw_registered_queues_lock);
	list_del(&client->list);
	mutex_unlock(&arfw_registered_queues_lock);

	mutex_lock(&registered_devices.lock);

	device = container_of(inode->i_cdev, struct arfw_char_device, cdev);

	if (lifetime_lock_is_constructed(&client->lt_lock))
		arfw_client_queue_destroy((arfw_client_queue_t)client);

	// At this point, there are no fd left in user space. It is safe:
	//   - to remove the events enqueued for the read() syscall
	//   - to free the queue data memory, since it is not mapped in the user space
	if (client->queue) {
		arfw_queue_event_cleanup(client);

		kref_put(&client->queue->queue_data->refcnt,
			 arfw_mem_region_free);
	}

	AR_ASSERT(device->references > 0);
	device->references--;
	if (device->released && device->references == 0)
		arfw_schedule_device_cleanup(device);

	mutex_unlock(&registered_devices.lock);

	if (client->queue) {
		// We cannot clean this up in shutdown because
		// any ongoing polls need it to exist, so we do it here on release.
		kfree(client->queue);
		client->queue = NULL;
	}

	kfree(fd_context);
	file->private_data = NULL;

	return 0;
}

static int get_device_info_locked(struct arfw_fd_context *fd_context,
				  struct arfw_device_information_req *dev_info)
{
	int err;

	err = fd_context->driver_ops->handle_get_device_information(
		fd_context->base_context, dev_info);
	if (err)
		return err;

	fd_context->device_info_locked.info = *dev_info;
	fd_context->device_info_locked.is_cached = true;

	return 0;
}

/**
 * Handle user request to create a new queue with given parameters.
 *
 * @retval 0 No error.
 *
 * @retval -E...      Errors from the corresponding driver_ops implementation.
 * @retval -E...      Errors from the corresponding arfw_int_queue implementation.
 * @retval -EFAULT    Failed to copy request from the user.
 * @retval -EINTR     Failed to lock device, interrupted.
 * @retval -ESHUTDOWN Device is already in shutdown mode.
 */
static int
handle_ioctl_queue_create(struct file *file, struct arfw_fd_context *fd_context,
			  struct ar_queue_create_req __user *user_req)
{
	int err;
	struct ar_queue_create_req req;
	struct arfw_char_device *device;
	struct arfw_device_information_req dev_info;
	struct arfw_client_queue *dup_client = NULL;

	AR_ASSERT(file);
	AR_ASSERT(fd_context);
	AR_ASSERT(user_req);

	if (copy_from_user(&req, user_req, sizeof(struct ar_queue_create_req)))
		return -EFAULT;

	err = mutex_lock_interruptible(&registered_devices.lock);
	if (err)
		return err;

	device = container_of(file->f_path.dentry->d_inode->i_cdev,
			      struct arfw_char_device, cdev);
	if (device->released) {
		err = -ESHUTDOWN;
		goto unlock_devices;
	}

	err = mutex_lock_interruptible(&fd_context->device_info_locked.lock);
	if (err)
		goto unlock_devices;

	if (!fd_context->device_info_locked.is_cached)
		err = get_device_info_locked(fd_context, &dev_info);
	else
		dev_info = fd_context->device_info_locked.info;

	mutex_unlock(&fd_context->device_info_locked.lock);

	if (err)
		goto unlock_devices;

	err = arfw_queue_create(fd_context->driver_ops,
				fd_context->base_context, fd_context->dev,
				device->dev, &req, &dev_info,
				&fd_context->client);

	if (err == -EEXIST) {
		mutex_lock(&arfw_registered_queues_lock);
		list_for_each_entry(dup_client, &arfw_registered_queues_list,
				    list) {
			if (!dup_client->queue ||
			    dup_client == &fd_context->client) {
				continue;
			} else if (dup_client->queue->hlos_endpoint_id ==
					   req.hlos_endpoint_id &&
				   dup_client->queue->fw_endpoint_id ==
					   req.fw_endpoint_id &&
				   dup_client->queue->direction ==
					   req.queue_direction) {
				AR_LOG_ARFW_ERR(
					AR_LOG_CREATE_QUEUE,
					"Duplicated queue, currently held by another process: %s[%d]",
					dup_client->debug.comm,
					dup_client->debug.pid);
			}
		}
		mutex_unlock(&arfw_registered_queues_lock);
	} else if (!err) {
		fd_context->client.queue->hw_dev = device->hw_dev;
	}

unlock_devices:
	mutex_unlock(&registered_devices.lock);

	return err;
}

/**
 * Handle user request to query device information.
 *
 * @retval 0 No error.
 *
 * @retval -E...   Errors from the corresponding driver_ops implementation.
 * @retval -EFAULT Failed to copy response to the user.
 * @retval -EINTR  Failed to lock device, interrupted.
 */
static int handle_ioctl_get_device_information(
	struct arfw_fd_context *fd_context,
	struct ar_device_information_req __user *out)
{
	int err;
	struct arfw_device_information_req dev_info;
	struct ar_device_information_req user_info;

	err = mutex_lock_interruptible(&fd_context->device_info_locked.lock);
	if (err)
		return err;

	err = get_device_info_locked(fd_context, &dev_info);

	mutex_unlock(&fd_context->device_info_locked.lock);

	if (err)
		return err;

	user_info.transport_header_size = dev_info.transport_header_size;
	user_info.inline_data_offset = dev_info.inline_data_offset;
	user_info.send_ring_max = dev_info.send_ring_max;
	user_info.rcv_ring_max = dev_info.rcv_ring_max;
	user_info.require_contiguous_memory_for_queues =
		dev_info.require_contiguous_memory_for_queues;
	user_info.rcv_ring_pend_buff_count_max =
		dev_info.rcv_ring_pend_buff_count_max;

	if (copy_to_user(out, &user_info,
			 sizeof(struct ar_device_information_req)))
		return -EFAULT;

	return 0;
}

/**
 * Handle user request to query queue debug information.
 *
 * @retval 0 No error.
 *
 * @retval -ESHUTDOWN Failed to lock the queue, already in shutdown mode.
 * @retval -EFAULT    Failed to copy response to the user.
 */
static int handle_ioctl_queue_debug(struct arfw_fd_context *fd_context,
				    struct ar_queue_debug_req __user *out)
{
	int err;
	struct arfw_client_queue *client = &fd_context->client;
	struct ar_queue_debug_req debug_info = {};

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		goto exit;

	debug_info.msg_count = (uint64_t)ar_atomic64_load(
		&client->debug.msg_count, AR_MEMORY_ORDER_RELAXED);
	debug_info.consumer_idx = ar_queue_get_consumer_idx(
		&client->queue->arfw_queue.submit_queue);
	debug_info.producer_idx = ar_queue_get_producer_idx(
		&client->queue->arfw_queue.submit_queue);

	lifetime_lock_release(&client->lt_lock);

	if (copy_to_user(out, &debug_info, sizeof(struct ar_queue_debug_req)))
		err = -EFAULT;

exit:
	return err;
}

/**
 * Handle user request to query queue information.
 * This will return the information about the queue: endpoints, direction, etc.
 *
 * @retval 0 No error.
 *
 * @retval -ESHUTDOWN Failed to lock the queue, already in shutdown mode.
 * @retval -EFAULT    Failed to copy response to the user.
 * @retval -ENOENT    Queue is not created/initialized yet.
 */
static int handle_ioctl_queue_info(struct arfw_fd_context *fd_context,
				   struct ar_queue_info_req __user *out)
{
	int err;
	struct arfw_client_queue *client;
	struct arfw_int_client_queue *queue;
	struct ar_queue_info_req info = {};

	AR_ASSERT(fd_context);
	client = &fd_context->client;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		goto exit;

	queue = client->queue;
	if (!queue) {
		err = -ENOENT;
		goto unlock_and_exit;
	}

	info.id = queue->id;
	info.direction = queue->direction;
	info.hlos_endpoint_id = queue->hlos_endpoint_id;
	info.fw_endpoint_id = queue->fw_endpoint_id;
	info.depth = queue->queue_meta.depth;
	info.mirror = queue->mirror;

unlock_and_exit:
	lifetime_lock_release(&client->lt_lock);

	if (!err && copy_to_user(out, &info, sizeof(struct ar_queue_info_req)))
		err = -EFAULT;

exit:
	return err;
}

/**
 * Handles notifying the driver when an index has been consumed up to. The index is not
 * passed as a param as it lives inside the shared memory in the ar circular queue construct.
 *
 * NOTE:
 * Only valid for queue direction AR_QUEUE_FW_TO_HLOS.
 * Cannot hold the serial work lock, only the lifetime lock.
 *
 * @retval 0 No error.
 *
 * @retval -EINVAL Passed queue type is invalid, only recv queue allowed.
 */
static int handle_ioctl_consumed_index(struct arfw_fd_context *fd_context)
{
	int err;
	struct arfw_client_queue *client = &fd_context->client;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		return err;

	// marking index as consumed is only applicable on recv queues.
	if (client->queue->queue_meta.queue_direction != RECEIVE) {
		err = -EINVAL;
		goto unlock;
	}

	// notify the driver
	AR_LOG_ARFW_QUEUE_DBG0(client->queue, AR_LOG_INDEX_CONSUMED,
			       "Update queue consumed index");
	client->queue->driver_ops->handle_rcv_queue_consume(
		client, client->queue->queue_context);

unlock:
	lifetime_lock_release(&client->lt_lock);

	return err;
}

/**
 * Handle register region request from user.
 *
 * @param[in] fd_context Context for the ioctl file descriptor.
 * @param[in] user_req The user request pointer.
 *
 * @retval region id >= 0 On success, no error.
 *
 * @retval -E...   Errors from the corresponding arfw_int_queue implementation.
 * @retval -EFAULT Failed to copy request from user.
 */
static int
handle_ioctl_register_region(struct arfw_fd_context *fd_context,
			     struct ar_region_register_req __user *user_req)
{
	struct ar_region_register_req req;
	struct arfw_client_queue *client = &fd_context->client;

	if (copy_from_user(&req, user_req, sizeof(req)))
		return -EFAULT;

	return arfw_queue_register_region(client, &req.region, req.managed);
}

/**
 * Handle unregister region request from user.
 *
 * @param[in] fd_context Context for the ioctl file descriptor.
 * @param[in] region_id The region id.
 *
 * @retval 0 No error.
 *
 * @retval -E... Errors from the corresponding arfw_int_queue implementation.
 */
static int handle_ioctl_unregister_region(struct arfw_fd_context *fd_context,
					  int __user region_id)
{
	struct arfw_client_queue *client = &fd_context->client;

	return arfw_queue_unregister_region(client, region_id);
}

/**
 * Create the arfw (hw) pend req from the ar (user) pend request.
 *
 * IMPORTANT:
 * Assumes a lock on regions idr is held while this is executing.
 *
 * @param[in] client the client queue
 * @param[in] req the user pend request
 * @param[out] arfw_req the resulting arfw req
 *
 * @retval 0 on success
 */
static int create_pend_req(struct arfw_client_queue *client,
			   const struct ar_pend_payload_req *req,
			   struct arfw_payload_pend_req *arfw_req)
{
	int err, temp;
	unsigned long flags;
	struct arfw_client_region *region;

	AR_ASSERT(client);
	AR_ASSERT(req);
	AR_ASSERT(arfw_req);

	err = arfw_queue_ref_region(client, req->mem_region_id);
	if (err)
		return err;

	// check if region is valid
	spin_lock_irqsave(&client->queue->locked_regions.lock, flags);
	region = idr_find(&client->queue->locked_regions.region_idr,
			  req->mem_region_id);
	if (region == NULL) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_PEND,
				      "Bad region id for payload pend",
				      "region_id: %d", req->mem_region_id);
		err = -ENOENT;
		goto deref_region;
	}

	// validate offset + size
	for (temp = 0; temp < req->chunk_count; temp++) {
		const struct ar_payload_chunk *chunk =
			&req->payload_chunks[temp];

		// check overflow
		if (chunk->offset > U64_MAX - chunk->size) {
			AR_LOG_ARFW_QUEUE_ERR(
				client->queue, AR_LOG_PEND,
				"Chunk offset + size overflows",
				"region_id: %d, chunk_offset: %llu, size: %llu, region size: %zu",
				req->mem_region_id, chunk->offset, chunk->size,
				region->mapping.region_size);
			err = -ERANGE;
			goto deref_region;
		}
		if (chunk->offset + chunk->size > region->mapping.region_size) {
			AR_LOG_ARFW_QUEUE_ERR(
				client->queue, AR_LOG_PEND,
				"Chunk offset + size exceeds region size",
				"region_id: %d, chunk offset: %llu, size: %llu, region size %zu",
				req->mem_region_id, chunk->offset, chunk->size,
				region->mapping.region_size);
			err = -ERANGE;
			goto deref_region;
		}

		arfw_req->sg_list[temp].addr =
			region->mapping.dma_region_addr + chunk->offset;

		if (unlikely(chunk->size == 0)) {
			AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_PEND,
					      "Pending with chunk size 0",
					      "region_id: %d, offset: %llu",
					      req->mem_region_id,
					      chunk->offset);
			err = -ERANGE;
			goto deref_region;
		}

		arfw_req->sg_list[temp].len = chunk->size;
	}

	AR_ASSERT(region->id == req->mem_region_id);

	arfw_req->sg_list_len = req->chunk_count;
	arfw_req->roundtrip.region_id = region->id;
	arfw_req->roundtrip.pend_id = req->pend_id;

deref_region:
	spin_unlock_irqrestore(&client->queue->locked_regions.lock, flags);

	if (err && arfw_queue_deref_region(client, req->mem_region_id))
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_PEND,
				      "Failed to deref region on failed pend.",
				      "region_id: %d", req->mem_region_id);

	return err;
}

/**
 * Handle pend payload request from user.
 *
 * @retval 0 No error.
 *
 * @retval -E... Errors from the corresponding driver_ops implementation.
 * @retval -EFAULT Failed to copy request from user.
 * @retval -EINVAL Passed user request parameters are invalid.
 * @retval -ENOENT Passed memory region id does not exist.
 * @retval -ERANGE Passed chunk size/offset is invalid: zero, overflows or too big for region.
 */
static int
handle_ioctl_pend_payload(struct arfw_fd_context *fd_context,
			  struct ar_pend_payload_req __user *user_req)
{
	int err;
	struct ar_pend_payload_req req;
	struct arfw_payload_pend_req arfw_req;
	struct arfw_client_queue *client;

	AR_ASSERT(fd_context);
	AR_ASSERT(user_req);
	client = &fd_context->client;

	if (copy_from_user(&req, user_req, sizeof(req)))
		return -EFAULT;

	if (req.chunk_count > AR_PEND_MAX_CHUNKS)
		return -EINVAL;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		return err;

	err = create_pend_req(client, &req, &arfw_req);
	if (err)
		goto unlock_queue;

	err = client->queue->driver_ops->handle_receive_payload_pend(
		client->queue->queue_context, &arfw_req);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_PEND,
				      "Failed to pend with HW.", "err: %d",
				      err);
		if (arfw_queue_deref_region(client, req.mem_region_id))
			AR_LOG_ARFW_QUEUE_ERR(
				client->queue, AR_LOG_PEND,
				"Failed to deref region on failed pend.",
				"region_id: %d", req.mem_region_id);
	}

unlock_queue:
	lifetime_lock_release(&client->lt_lock);

	if (err) {
		ar_atomic64_fetch_add(&client->debug.pend_error_count, 1,
				      AR_MEMORY_ORDER_RELAXED);
		return err;
	}
	ar_atomic64_fetch_add(&client->debug.pend_active_count, 1,
			      AR_MEMORY_ORDER_RELAXED);
	ar_atomic64_fetch_add(&client->debug.pend_count, 1,
			      AR_MEMORY_ORDER_RELAXED);
	client->debug.pend_last_ts = ktime_get_real();

	AR_LOG_ARFW_QUEUE_DBG(client->queue, AR_LOG_PEND,
			      "Pended buffer successfully",
			      "region_id: %d, pend_id: %d, roundtrip_id: 0x%x",
			      arfw_req.roundtrip.region_id,
			      arfw_req.roundtrip.pend_id,
			      arfw_req.roundtrip.id);
	return arfw_req.roundtrip.pend_id;
}

/**
 * Handle user request to destroy queue.
 *
 * @retval 0 No error.
 */
static int handle_ioctl_queue_destroy(struct arfw_fd_context *fd_context)
{
	struct arfw_client_queue *client;

	AR_ASSERT(fd_context);
	client = &fd_context->client;
	AR_ASSERT(client);

	// Need to grab cdev lock here to prevent device driver from
	// racing with the client ops here in shutdown flows.
	mutex_lock(&registered_devices.lock);

	if (lifetime_lock_is_constructed(&client->lt_lock)) {
		client->queue->shutdown_reason = AR_QUEUE_SHUTDOWN_NORMAL;
		arfw_client_queue_destroy(client);
	}

	mutex_unlock(&registered_devices.lock);

	return 0;
}

static void
arfw_aperture_map_fd_release(struct arfw_loopback_map_fd *map_fd_data)
{
	int err;
	struct arfw_client_queue *client;
	uint16_t region_id;
	struct arfw_aperture_fd_context *context;

	AR_ASSERT(map_fd_data);
	context = map_fd_data->context;
	AR_ASSERT(context);
	client = context->client;
	AR_ASSERT(client);

	region_id = context->region_id;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err) {
		AR_ASSERT(context->driver_ops);
		AR_ASSERT(context->base_context);
		// if queue was closed first, we pass the aperture free to the hw layer, but with a null ptr for queue context.
		context->driver_ops->handle_aperture_free(context->base_context,
							  NULL,
							  map_fd_data->vmaddr,
							  map_fd_data->size);
		return;
	}

	AR_ASSERT(client->queue);
	AR_ASSERT(client->queue->driver_ops);
	AR_ASSERT(client->queue->driver_ops->handle_aperture_free);

	/**
	 * Aperture buffers are unregistered by the kernel instead of the client. We do this
	 * because the buffers need to be unregistered before they are free'd, and we don't
	 * wan't to double-unregister the buffer.
	 *
	 * External buffers don't exhibit this issue because external buffers are not owned
	 * by the arfw kernel module, and queue memory is managed by the kernel (user space)
	 * cannot unmap queue memory and manage queue memory allocations.
	 *
	 * It is also likely that we will require managing the underlying dev refcount
	 * to ensure that aperture buffer lifecycle is valid. (T208592194)
	 */
	err = arfw_queue_unregister_region(client, region_id);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_APERTURE_FREE,
				      "Error during aperture unregister.",
				      "region_id %u, err: %d", region_id, err);
		goto exit;
	}

	client->queue->driver_ops->handle_aperture_free(
		client->queue->base_context, client->queue->queue_context,
		map_fd_data->vmaddr, map_fd_data->size);

exit:
	lifetime_lock_release(&client->lt_lock);

	kfree(context);
}

static int arfw_aperture_fd_mmap(struct arfw_loopback_map_fd *map_fd_data,
				 struct vm_area_struct *vma)
{
	int err;
	struct arfw_aperture_fd_context *context;
	struct arfw_client_queue *client;
	size_t size;

	AR_ASSERT(map_fd_data);
	AR_ASSERT(vma);
	context = map_fd_data->context;
	AR_ASSERT(context);
	client = context->client;

	AR_ASSERT(client);
	AR_ASSERT(client->queue);
	AR_ASSERT(client->queue->driver_ops);
	AR_ASSERT(client->queue->driver_ops->handle_aperture_mmap);

	AR_ASSERT(vma->vm_end > vma->vm_start);
	size = vma->vm_end - vma->vm_start;
	if (size > map_fd_data->size) {
		AR_LOG_ARFW_QUEUE_ERR(
			client->queue, AR_LOG_APERTURE_MMAP,
			"Error during aperture fd mmap - requested size larger than mapping size.",
			"requested size: %zu, mapping size: %zu", size,
			map_fd_data->size);
		return -EINVAL;
	}

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(
			client->queue, AR_LOG_APERTURE_MMAP,
			"Error during aperture fd mmap - unable to acquire lifetime lock.",
			"err: %d", err);
		return err;
	}

	err = client->queue->driver_ops->handle_aperture_mmap(
		client->queue->base_context, client->queue->queue_context,
		map_fd_data->vmaddr, map_fd_data->size, vma);

	lifetime_lock_release(&client->lt_lock);

	return err;
}

static struct arfw_loopback_map_fd_ops mmap_fd_ops = {
	.mmap = arfw_aperture_fd_mmap,
	.release = arfw_aperture_map_fd_release,
};

static int
handle_ioctl_aperture_alloc(struct arfw_fd_context *fd_context,
			    struct ar_aperture_alloc_req __user *user_req)
{
	int err, fd = -1;
	struct ar_aperture_alloc_req req;
	struct arfw_client_queue *client;
	int region_id;
	void *aperture_addr;
	dma_addr_t aperture_dma_addr;
	struct arfw_aperture_fd_context *mmap_fd_context;

	AR_ASSERT(fd_context);
	client = &fd_context->client;

	if (copy_from_user(&req, user_req, sizeof(req)))
		return -EFAULT;

	mmap_fd_context =
		kzalloc(sizeof(struct arfw_aperture_fd_context), GFP_KERNEL);
	if (mmap_fd_context == NULL)
		return -ENOMEM;

	err = lifetime_lock_acquire(&client->lt_lock);
	if (err)
		goto free_mmap_fd_context;

	if (client->queue->driver_ops->handle_aperture_alloc == NULL) {
		err = -ENOTSUPP;
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_APERTURE_ALLOC,
				      "Aperture allocation not supported.", "");
		goto unlock_ll;
	}

	if (req.size % ARFW_APERTURE_ALLOC_CHUNK_SIZE) {
		err = -EINVAL;
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_APERTURE_ALLOC,
				      "Invalid aperture alloc alignment.",
				      "size: %d", req.size);
		goto unlock_ll;
	}

	err = client->queue->driver_ops->handle_aperture_alloc(
		client->queue->base_context, client->queue->queue_context,
		req.size, &aperture_addr, &aperture_dma_addr);
	if (err) {
		AR_LOG_ARFW_QUEUE_ERR(
			client->queue, AR_LOG_APERTURE_ALLOC,
			"HW impl failed aperture buffer allocation.",
			"size: %d", req.size);
		goto unlock_ll;
	}

	// create a memory region for this allocation
	region_id = arfw_queue_register_aperture_region(
		client, aperture_addr, aperture_dma_addr, req.size);
	if (region_id < 0) {
		err = region_id;
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_APERTURE_ALLOC,
				      "Failed to register aperture region.",
				      "err %d", err);
		goto aperture_free;
	}

	mmap_fd_context->client = client;
	mmap_fd_context->driver_ops = client->queue->driver_ops;
	mmap_fd_context->base_context = client->queue->base_context;
	mmap_fd_context->region_id = region_id;

	// create a mmap fd.
	fd = arfw_loopback_map_fd_create("arfw_aperture_mapping",
					 mmap_fd_context, aperture_addr,
					 req.size, &mmap_fd_ops);
	if (fd < 0) {
		err = fd;
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_APERTURE_ALLOC,
				      "Failed to allocate aperture map fd.",
				      "err %d", err);
		goto unregister_region;
	}

	req.mem_region_id = region_id;
	// copy out memory region id
	if (copy_to_user(user_req, &req, sizeof(req))) {
		close_fd(fd);
		lifetime_lock_release(&client->lt_lock);
		// close_fd will call cleanup on fd, which will call unregister_region and free the memory
		return -EFAULT;
	}

	lifetime_lock_release(&client->lt_lock);

	AR_LOG_ARFW_QUEUE_DBG(client->queue, AR_LOG_APERTURE_ALLOC,
			      "Allocated aperture",
			      "region_id: %d, size: %d, fd: %d", region_id,
			      req.size, fd);
	return fd;

unregister_region:
	arfw_queue_unregister_region(client, region_id);
aperture_free:
	client->queue->driver_ops->handle_aperture_free(
		client->queue->base_context, client->queue->queue_context,
		aperture_addr, req.size);
unlock_ll:
	lifetime_lock_release(&client->lt_lock);
free_mmap_fd_context:
	kfree(mmap_fd_context);
	return err;
}

static long arfw_ioctl(struct file *file, unsigned int ioctl_num,
		       unsigned long ioctl_param)
{
	long ret;
	struct arfw_fd_context *fd_context = get_fd_context(file);

	switch (ioctl_num) {
	case ARFW_QUEUE_CREATE:
		ret = handle_ioctl_queue_create(
			file, fd_context,
			(struct ar_queue_create_req __user *)ioctl_param);
		break;
	case ARFW_DEV_INFO:
		ret = handle_ioctl_get_device_information(
			fd_context,
			(struct ar_device_information_req __user *)ioctl_param);
		break;
	case ARFW_CONSUMED_INDEX:
		ret = handle_ioctl_consumed_index(fd_context);
		break;
	case ARFW_REGISTER_REGION:
		ret = handle_ioctl_register_region(
			fd_context,
			(struct ar_region_register_req __user *)ioctl_param);
		break;
	case ARFW_UNREGISTER_REGION:
		ret = handle_ioctl_unregister_region(fd_context, ioctl_param);
		break;
	case ARFW_PEND_PAYLOAD:
		ret = handle_ioctl_pend_payload(
			fd_context,
			(struct ar_pend_payload_req __user *)ioctl_param);
		break;
	case ARFW_QUEUE_DEBUG:
		ret = handle_ioctl_queue_debug(
			fd_context,
			(struct ar_queue_debug_req __user *)ioctl_param);
		break;
	case ARFW_QUEUE_INFO:
		ret = handle_ioctl_queue_info(
			fd_context,
			(struct ar_queue_info_req __user *)ioctl_param);
		break;
	case ARFW_QUEUE_DESTROY:
		ret = handle_ioctl_queue_destroy(fd_context);
		break;
	case ARFW_APERTURE_ALLOC:
		ret = handle_ioctl_aperture_alloc(
			fd_context,
			(struct ar_aperture_alloc_req __user *)ioctl_param);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/**
 * Handle user writes to an open file.
 *
 * NOTE:
 * Acquires the queue work lock for the duration of queue polling,
 * and releases it after polling is done. Polling will invoke underlying
 * driver implementation code, which must assume work lock is held.
 *
 * @retval total >= 0 No error.
 *
 * @retval -ESHUTDOWN Failed to lock the queue, already in shutdown mode.
 * @retval -EINVAL    Passed queue type is wrong, expecyed send queues only.
 */
static ssize_t arfw_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *offset)
{
	int err = 0;
	ssize_t total = 0;
	struct arfw_client_queue *client;

	(void)buf;
	(void)offset;

	client = get_client_locked_for_work(get_fd_context(file));

	if (IS_ERR(client))
		return PTR_ERR(client);

	// can only write on send queues
	if (client->queue->queue_meta.queue_direction != SEND) {
		err = -EINVAL;
		goto unlock_queue;
	}

	AR_LOG_ARFW_QUEUE_DBG(client->queue, AR_LOG_WRITE,
			      "Polling to drain write queue",
			      "user req count: %zu", count);

	if (debugfs_initialized())
		ar_binned_sampler_add_sample(
			client->debug.capacity_sampler,
			ar_fw_queue_get_consumable_count(&client->queue->arfw_queue));

	// drain queue as much as possible
	while (true) {
		size_t consumed;

		consumed = ar_fw_queue_poll(&client->queue->arfw_queue, count,
					    NULL);
		total += consumed;

		if (!consumed)
			break;
	}

	AR_LOG_ARFW_QUEUE_DBG(client->queue, AR_LOG_WRITE,
			      "Drained write queue", "req count: %zd", total);
	ar_atomic64_fetch_add(&client->debug.msg_count, total,
			      AR_MEMORY_ORDER_RELAXED);
	client->debug.msg_activity_last_ts = ktime_get_real();

unlock_queue:
	unlock_client_for_work(client);

	return err ? err : total;
}

// translate a arfw_client_event to a ar_queue_event
static void translate_arfw_event(struct ar_queue_event *uapi_event,
				 const struct arfw_client_event *client_event)
{
	AR_ASSERT(uapi_event);
	AR_ASSERT(client_event);

	uapi_event->type = client_event->type;

	switch (client_event->type) {
	case AR_QUEUE_PEND_REQUIRED:
		uapi_event->pend_size = client_event->pend_size;
		return;
	case AR_QUEUE_PAYLOAD_CONSUMED:
		uapi_event->payload_context = client_event->payload_context;
		return;
	case AR_QUEUE_READ_COMPLETE:
		return;
	case AR_QUEUE_SHUTDOWN:
		uapi_event->shutdown_reason = client_event->shutdown_reason;
		return;
	default:
		AR_ASSERT(false);
	}
}

/**
 * Handle user reads from an open file.
 *
 * @retval size > 0   Size of bytes read, no error.
 *
 * @retval -EINVAL      Event size requested by user is unexpected.
 * @retval -ESHUTDOWN   Queue is in shutdown mode, cannot be used.
 * @retval -EWOULDBLOCK Non-blocking request on an empty event queue.
 * @retval -EINTR       Failed to read event, interrupted.
 * @retval -EFAULT      Failed to copy response to user.
 */
static ssize_t arfw_read(struct file *file, char __user *buf, size_t count,
			 loff_t *offset)
{
	int err, event_idx;
	uint16_t consumed_count;
	size_t padding_size;
	struct arfw_client_queue *client;
	struct arfw_client_event_batch queue_events;
	struct ar_queue_event_batch user_events;
	struct arfw_fd_context *fd_context;

	fd_context = get_fd_context(file);
	client = &fd_context->client;

	// we expect reads to take a struct ar_queue_event
	if (count != sizeof(struct ar_queue_event_batch))
		return -EINVAL;

	// only read on constructed queues, handled by lifetime_lock_acquire
	err = lifetime_lock_acquire(&client->lt_lock);

	// shutting down queues should always return the same event
	if (err == -ESHUTDOWN) {
		queue_events.events[0].type = AR_QUEUE_SHUTDOWN;
		queue_events.events[0].shutdown_reason =
			client->queue->shutdown_reason;
		queue_events.size = 1;
		goto send_event;
	}

	if (err)
		return err;

	// handle nonblocking case.
	if (file->f_flags & O_NONBLOCK) {
		err = arfw_queue_event_dequeue(client, &queue_events);
		if (err == -ENOMSG)
			err = -EWOULDBLOCK;
	} else {
		// handle blocking case
		AR_LOG_ARFW_QUEUE_DBG0(client->queue, AR_LOG_READ,
				       "Blocking for arfw event");
		err = arfw_queue_event_wait_dequeue(client, &queue_events);
	}

	lifetime_lock_release(&client->lt_lock);

	if (err < 0)
		return err;

send_event:
	user_events.size = queue_events.size;
	for (event_idx = 0; event_idx < queue_events.size; event_idx++) {
		AR_LOG_ARFW_QUEUE_DBG(client->queue, AR_LOG_READ,
				      "Consumed event", "type %d",
				      queue_events.events[event_idx].type);
		translate_arfw_event(&user_events.events[event_idx],
				     &queue_events.events[event_idx]);
		if (queue_events.events[event_idx].type ==
		    AR_QUEUE_READ_COMPLETE) {
			consumed_count = ar_fw_queue_get_consumable_count(
				&client->queue->arfw_queue);
			if (debugfs_initialized())
				ar_binned_sampler_add_sample(
					client->debug.capacity_sampler, consumed_count);
			ar_atomic64_fetch_add(&client->debug.consumed_count,
					      consumed_count,
					      AR_MEMORY_ORDER_RELAXED);
		}
	}

	// We do not need to copy events if they were not populated.
	// We can just report to userspace that we did, but really only do the bare minimum.
	// Userspace will work with the size variable that we set anyways.
	padding_size = (AR_QUEUE_EVENT_BATCH_MAX - user_events.size) *
		       sizeof(*user_events.events);
	if (copy_to_user(buf, &user_events, sizeof(user_events) - padding_size))
		return -EFAULT;

	return sizeof(user_events);
}

static unsigned int arfw_poll(struct file *file, poll_table *wait)
{
	int ret;
	struct arfw_fd_context *fd_context = NULL;
	struct arfw_client_queue *client = NULL;

	fd_context = get_fd_context(file);
	client = &fd_context->client;

	ret = lifetime_lock_acquire(&client->lt_lock);

	// let shutting down queues communicate the event back to the client
	if (ret == -ESHUTDOWN)
		return POLLIN | POLLRDNORM;
	else if (ret)
		return POLLERR;

	poll_wait(file, &client->queue->event_wq, wait);

	ret = arfw_queue_event_ready(client);
	lifetime_lock_release(&client->lt_lock);

	if (ret < 0)
		return POLLERR;
	else if (ret > 0)
		return POLLIN | POLLRDNORM;

	return ret;
}

/**
 * Handle user mmap request.
 *
 * @retval 0 No error.
 *
 * @retval -ESHUTDOWN Failed to lock the queue, already in shutdown mode.
 * @retval -EINVAL    Passed queue type or size are wrong.
 * @retval -ENOENT    Data map is not implemented by the device driver.
 */
static int arfw_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	size_t data_size;
	struct arfw_client_queue *client;
	const struct arfw_driver_ops *driver_ops;

	AR_ASSERT(file);
	AR_ASSERT(vma);

	client = get_client_locked_for_work(get_fd_context(file));
	if (IS_ERR(client)) {
		ret = PTR_ERR(client);
		AR_LOG_ARFW_ERR(
			AR_LOG_MMAP,
			"can't get the queue handler for the requested fd, err = %d",
			ret);
		goto ret_value;
	}

	AR_ASSERT(client->queue);
	driver_ops = client->queue->driver_ops;
	AR_ASSERT(driver_ops);

	if (!driver_ops->handle_queue_data_mmap) {
		ret = -ENOENT;
		AR_LOG_ARFW_QUEUE_ERR(
			client->queue, AR_LOG_MMAP,
			"mmap() for the data part of the queue isn't supported",
			"err = %d", ret);
		goto unlock_client;
	}

	data_size = client->queue->queue_data->size;
	if (data_size != (vma->vm_end - vma->vm_start)) {
		ret = -EINVAL;
		AR_LOG_ARFW_QUEUE_ERR(
			client->queue, AR_LOG_MMAP,
			"wrong size is used, the user could mmap only the full queue data region",
			"expected = %lu, requested = %lu, err = %d", data_size,
			(vma->vm_end - vma->vm_start), ret);
		goto unlock_client;
	}

	ret = driver_ops->handle_queue_data_mmap(
		client->queue->base_context, client->queue->queue_data, vma);

unlock_client:
	unlock_client_for_work(client);
ret_value:
	return ret;
}

/// initialize file_operations
static const struct file_operations arfw_chdev_fops = {
	.owner = THIS_MODULE,
	.open = arfw_open,
	.release = arfw_release,
	.unlocked_ioctl = arfw_ioctl,
	.read = arfw_read,
	.write = arfw_write,
	.poll = arfw_poll,
	.mmap = arfw_mmap,
};

/**
 * Create an ARFW character device.
 *
 * @param[in] hw_dev The device associated with the hw ops this device may perform.
 * @param[in] device_id a valid device_id (see arfw_mem_util_check_arfw_device_id)
 * @param[in] driver_ops a valid driver_ops (see check_arfw_driver_ops)
 * @param[in] base_context an opaque pointer to the underlying hw device
 * @param[out] out_device where the created device will be stored
 */
static int arfw_char_device_create(struct device *hw_dev, const char *device_id,
				   const struct arfw_driver_ops *driver_ops,
				   void *base_context,
				   struct arfw_char_device **out_device)
{
	int err;
	dev_t dev;
	struct arfw_char_device *device;
	char *device_id_copy;

	device = kzalloc(sizeof(struct arfw_char_device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;

	// copy device_id string
	device_id_copy = kstrdup(device_id, GFP_KERNEL);
	if (!device_id_copy) {
		err = -ENOMEM;
		goto free_arfw_char_device;
	}
	device->id = device_id_copy;

	err = alloc_chrdev_region(&dev, 0, 1, AR_CLASS_NAME);
	if (err) {
		AR_LOG_ARFW_ERR(
			AR_LOG_DEV_REG,
			"Failed to allocate chrdev region for device id %s",
			device_id_copy);
		goto free_arfw_char_device;
	}

	// assign driver ops to invoke later
	device->driver_ops = driver_ops;
	// assign the context of the underlying hw device
	device->base_context = base_context;
	// init the cdev member
	cdev_init(&device->cdev, &arfw_chdev_fops);
	device->cdev.owner = THIS_MODULE;
	kobject_init(&device->kobj, &arfw_ktype);
	device->cdev.kobj.parent = &device->kobj;

	// create the /dev device
	device->dev = device_create(device_class, NULL, dev, device, "%s",
				    device_id_copy);
	if (IS_ERR(device->dev)) {
		AR_LOG_ARFW_ERR(AR_LOG_DEV_REG,
				"Failed to create device for arfw device id %s",
				device_id_copy);
		err = PTR_ERR(device->dev);
		goto free_chdev_region;
	}

	// init sysfs attributes
	err = add_arfw_device_sysfs_nodes(device);
	if (err)
		AR_LOG_ARFW_DEV_WARN(
			device->dev, AR_LOG_DEV_REG,
			"Failed to add sysfs nodes for arfw device id %s",
			device_id_copy);

	err = cdev_add(&device->cdev, dev, 1);
	if (err) {
		AR_LOG_ARFW_DEV_ERR(device->dev, AR_LOG_DEV_REG,
				    "Failed to add cdev");
		goto remove_sysfs_nodes;
	}

	device->hw_dev = hw_dev;
	*out_device = device;

	return err;

remove_sysfs_nodes:
	remove_arfw_device_sysfs_nodes(device);
	device_destroy(device_class, dev);
free_chdev_region:
	unregister_chrdev_region(dev, 1);
free_arfw_char_device:
	kfree(device->id);
	kfree(device);

	return err;
}

/**
 * Unregister a previously registered ar_firmware device. To be called by driver
 * implementations prior to device teardown.
 *
 * @param[in] device_id Unique device id string
 * @param[out] hw_device The underlying hw device.
 * @param[in] module_remove If true, then unregister is called from the module remove operation.
 *
 * @return 0 on success
 * @return -ENOENT if the device was not previously registered
 */
static int arfw_device_unregister(const char *device_id,
				  struct device **hw_device, bool module_remove)
{
	int err = 0;
	struct arfw_char_device *device;

	AR_ASSERT(device_id);

	mutex_lock(&registered_devices.lock);

	device = arfw_dev_lookup(device_id);
	if (device == NULL) {
		err = -ENODEV;
		goto unlock_devices;
	}

	if (hw_device)
		*hw_device = device->hw_dev;

	if (device->released) {
		err = -ESHUTDOWN;
		goto unlock_devices;
	}

	device->released = true;
	// Unregister is called during module removal process. Reset the
	// driver_ops field, so other parts of code could check the state.
	// The check is made at least in the arfw_release() function.
	// The module is still left in the system, if:
	//   - arfw_loopback: unregister CTL device
	//   - ar_pci: suspend device
	// In this case (module_remove == false) and the callbacks are
	// still usable by the system.
	if (module_remove)
		device->driver_ops = NULL;
	if (device->references == 0)
		arfw_schedule_device_cleanup(device);

	if (debugfs_initialized())
		arfw_debug_device_remove(device_id);

unlock_devices:
	mutex_unlock(&registered_devices.lock);

	// Log only successful unregister message.
	// For some devices (for example arfw-loopback) it is possible to call the
	// unregister call twice: unregistered with the ioctl and by fd release.
	// Just propagate the error to the caller, so it could decide to print
	// out the error message.
	if (!err)
		AR_LOG_ARFW_INFO(AR_LOG_DEV_UNREG,
				 "Successfully unregistered %.*s",
				 ARFW_DEVICE_ID_MAX_LEN, device_id);

	return err;
}

/**
 * Mark a previous IO as consumed, allowing the slot in the queue to be reused.
 *
 * NOTE:
 * Only valid for queue direction AR_QUEUE_HLOS_TO_FW (SEND).
 * Acquires & releases the queue serial work lock
 *
 * @param[in] queue handle for the current queue
 * @param[in] index The index from req in
 * arfw_handle_send_queue_request_t (index of the slot in the circular queue)
 *
 * @retval 0 on success
 * @retval -ESHUTDOWN The client is already torn down.
 * @retval -EINTR The call was interrupted.
 * @retval -EINVAL The index is wrong.
 */
static int arfw_client_queue_index_consumed(arfw_client_queue_t client,
					    uint32_t index)
{
	int err = 0;
	ar_firmware_message_header_t *msg_header = NULL;
	struct arfw_client_queue *arfw_client =
		(struct arfw_client_queue *)client;

	AR_ASSERT(arfw_client);
	AR_ASSERT(arfw_client->queue);
	AR_ASSERT(arfw_client->queue->queue_meta.queue_direction == SEND);

	err = lock_client_for_work(arfw_client);
	if (err)
		goto exit;

	if (AR_UNLIKELY(arfw_client->queue->queue_meta.depth <= index)) {
		/**
		 * This can happen if we pass wrong index into this function, which
		 * is a valid case (occasionally) since we might race with the device driver
		 * and lose device registers before we stop irqs - hence getting gibberish index values.
		 */
		AR_LOG_ARFW_QUEUE_ERR(arfw_client->queue, AR_LOG_INDEX_CONSUMED,
				      "Index consume failed",
				      "index %u >= depth", index);
		err = -EINVAL;
		goto unlock_client;
	}

	// inspect slot and notify user layer if there was a payload context that needs to be reclaimed
	msg_header =
		(ar_firmware_message_header_t
			 *)(ar_fw_queue_base_address_get(
				    &arfw_client->queue->arfw_queue) +
			    index * arfw_client->queue->queue_meta.element_size);

	if (AR_UNLIKELY(!ar_fw_queue_mark_slot_ready(
		    &arfw_client->queue->arfw_queue, index))) {
		/**
		 * Same thing as above, we might have an index that is not correct, but passed the depth
		 * check. Need to ensure we succeeded here.
		 */

		// It could be the case, that FW passed the wrong index to AP.
		AR_LOG_ARFW_QUEUE_ERR(arfw_client->queue, AR_LOG_INDEX_CONSUMED,
				      "Index consume failed",
				      "index %u, not marked ready", index);
		if (debugfs_initialized())
			arfw_debug_dump_queue(arfw_client);
		err = -EINVAL;
		goto unlock_client;
	}

	AR_LOG_ARFW_QUEUE_DBG(arfw_client->queue, AR_LOG_INDEX_CONSUMED,
			      "Index consumed", "index %u", index);
	ar_atomic64_fetch_add(&arfw_client->debug.consumed_count, 1,
			      AR_MEMORY_ORDER_RELAXED);

	// send mirror queues do not need the payload-consumed event
	// since they use previously pended buffers
	if (!arfw_client->queue->mirror &&
	    data_location_external_to_queue(msg_header->data_location))
		err = arfw_queue_external_payload_consumed(arfw_client,
							   msg_header);

unlock_client:
	unlock_client_for_work(arfw_client);

exit:
	return err;
}

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
 * @param[in] client handle for the current queue
 * @param[out] req The io request in the queue. (to be filled out and passed
 * to arfw_client_queue_produce_request)
 *
 * @retval 0 on success
 */
static int arfw_client_queue_reserve_request(arfw_client_queue_t client,
					     struct arfw_io_request *req)
{
	int err = 0;
	struct ar_fw_io_request ar_fw_request;
	struct arfw_client_queue *arfw_client =
		(struct arfw_client_queue *)client;
	ar_firmware_message_header_t *header = NULL;

	AR_ASSERT(arfw_client);
	AR_ASSERT(arfw_client->queue->queue_meta.queue_direction == RECEIVE);

	err = ar_fw_queue_slot_reserve(&arfw_client->queue->arfw_queue,
				       &ar_fw_request);
	if (err) {
		err = -ENOSPC;
	} else {
		req->data = ar_fw_request.data;
		req->data_size = ar_fw_request.data_size;
		req->index = ar_fw_request.index;

		if (debugfs_initialized()) {
			header = (ar_firmware_message_header_t *)req->data;
			/**
			 * When slots are processed by the user, they will fill out latency info in the timestamp slots.
			 * This overwrites the timestamp values with the time taken values, with system processing time
			 * stored in kernel_creation_timestamp_us and user procesing time stored in
			 * user_creation_timestamp_us. Otherwise, they should be zero'd out. (we also zero these
			 * out after we are done reading them)
			 */
			if (likely(header->kernel_creation_timestamp_us)) {
				ar_binned_sampler_add_sample(
					arfw_client->debug.latency_sampler,
					header->kernel_creation_timestamp_us);

				if (unlikely(header->kernel_creation_timestamp_us >
					     THRESHOLD_TIME))
					arfw_client->debug.latency_threshold_last_ts =
						ktime_get_real_seconds();
			}

			if (likely(header->user_creation_timestamp_us)) {
				ar_binned_sampler_add_sample(
					arfw_client->debug.user_latency_sampler,
					header->user_creation_timestamp_us);

				if (unlikely(header->user_creation_timestamp_us >
					     THRESHOLD_TIME))
					arfw_client->debug
						.user_latency_threshold_last_ts =
						ktime_get_real_seconds();
			}

			header->user_creation_timestamp_us = 0;
			header->kernel_creation_timestamp_us = 0;
		}
	}

	return err;
}

/**
 * Driver entry point to produce data to the queue.
 *
 * NOTE:
 * Only valid for queue direction AR_QUEUE_FW_TO_HLOS (RECEIVE)
 * Acquires & releases the queue serial work lock
 *
 * @param[in] queue handle for the current queue
 * @param[in] req The io request data structure, previously acquired via
 * arfw_client_queue_reserve_request
 *
 * @retval 0 on success
 */
static int arfw_client_queue_produce_request(arfw_client_queue_t client,
					     struct arfw_io_request *req)
{
	int err;
	struct ar_fw_io_request ar_fw_request;
	struct arfw_client_event read_event = { .type = AR_QUEUE_READ_COMPLETE };
	struct arfw_client_queue *arfw_client =
		(struct arfw_client_queue *)client;
	ar_firmware_message_header_t *msg_header;

	AR_ASSERT(arfw_client);

	ar_fw_request.data = req->data;
	ar_fw_request.data_size = req->data_size;
	ar_fw_request.index = req->index;

	err = lock_client_for_work(arfw_client);
	if (err) {
		ar_atomic64_fetch_add(&arfw_client->debug.drop_count, 1,
				      AR_MEMORY_ORDER_RELAXED);
		return err;
	}

	AR_ASSERT(arfw_client->queue->queue_meta.queue_direction == RECEIVE);

	msg_header = req->data;
	AR_ASSERT(msg_header);
	msg_header->kernel_creation_timestamp_us = ktime_to_us(ktime_get());
	msg_header->user_creation_timestamp_us =
		msg_header->kernel_creation_timestamp_us;

	if (AR_UNLIKELY(!ar_fw_queue_slot_produce(
		    &arfw_client->queue->arfw_queue, &ar_fw_request))) {
		AR_LOG_ARFW_QUEUE_ERR(arfw_client->queue, AR_LOG_QUEUE_PRODUCE,
				      "Queue produce failed", "index %u",
				      req->index);
		ar_atomic64_fetch_add(&arfw_client->debug.drop_count, 1,
				      AR_MEMORY_ORDER_RELAXED);
		err = -EIO;
	}

	unlock_client_for_work(arfw_client);

	if (err)
		return err;

	ar_atomic64_fetch_add(&arfw_client->debug.msg_count, 1,
			      AR_MEMORY_ORDER_RELAXED);
	arfw_client->debug.msg_activity_last_ts = ktime_get_real();

	if (msg_header->data_location == ARFW_BUFFER_LOC_EXTERNAL)
		ar_atomic64_fetch_add(&arfw_client->debug.external_count, 1,
				      AR_MEMORY_ORDER_RELAXED);
	else
		ar_atomic64_fetch_add(&arfw_client->debug.inline_count, 1,
				      AR_MEMORY_ORDER_RELAXED);

	return arfw_queue_event_enqueue(arfw_client, &read_event);
}

/**
 * Notify the client that an external payload needs to be pended to deliver a
 * message.
 *
 * NOTE:
 * Only valid on AR_QUEUE_FW_TO_HLOS queues.
 * Does not acquire queue serial work lock.
 *
 * @param[in] queue handle for the current queue
 * @param[in] required_size The minimum size the consumer must allocated
 *
 * @retval 0 On success.
 */
static int arfw_payload_pend_required(arfw_client_queue_t client,
				      uint32_t required_size)
{
	struct arfw_client_event pend_required_event = {
		.type = AR_QUEUE_PEND_REQUIRED
	};
	struct arfw_client_queue *arfw_client =
		(struct arfw_client_queue *)client;

	AR_ASSERT(arfw_client);

	pend_required_event.pend_size = required_size;
	ar_atomic64_fetch_add(&arfw_client->debug.pend_req_count, 1,
			      AR_MEMORY_ORDER_RELAXED);

	return arfw_queue_event_enqueue(arfw_client, &pend_required_event);
}

/**
 * Notify the client that the pended buffer was received and it's safe to
 * release the buffer.
 *
 * NOTE:
 * Does not acquire queue serial work lock.
 *
 * @param[in] queue Handle for the current queue
 * @param[in] id Packed region_id and pend_id
 *
 * @retval 0 On success.
 */
static int arfw_payload_pend_released(arfw_client_queue_t client, uint32_t id)
{
	int err;
	roundtrip_id_t roundtrip_id = { .id = id };
	struct arfw_client_queue *arfw_client =
		(struct arfw_client_queue *)client;

	AR_ASSERT(arfw_client);

	err = lifetime_lock_acquire(&arfw_client->lt_lock);
	if (err) {
		AR_LOG_ARFW_ERR(AR_LOG_CLIENT_LOCK,
				"Failed to acquire lifetime lock, err = %d",
				err);
		goto exit;
	}

	AR_ASSERT(arfw_client->queue);
	ar_atomic64_fetch_add(&arfw_client->debug.pend_active_count, -1,
			      AR_MEMORY_ORDER_RELAXED);

	err = arfw_queue_deref_region(arfw_client, roundtrip_id.region_id);
	if (err)
		AR_LOG_ARFW_QUEUE_ERR(arfw_client->queue, AR_LOG_CLIENT_LOCK,
				      "Failed to deref region.", "region_id %d",
				      roundtrip_id.region_id);

	lifetime_lock_release(&arfw_client->lt_lock);

exit:
	return err;
}

/**
 * Driver API to trigger shutdown of a client queue.
 * This will only tell the clients we are shutting down.
 *
 * NOTE:
 * Does not acquire queue serial work lock.
 * Marks the the lock as shutting down.
 *
 * @param[in] queue handle for the current queue
 *
 * @retval 0 On success.
 */
static int arfw_client_queue_shutdown(arfw_client_queue_t client,
				      enum ar_queue_shutdown_reason reason)
{
	int err;
	struct arfw_client_event shutdown_event = {};
	struct arfw_client_queue *arfw_client =
		(struct arfw_client_queue *)client;

	AR_ASSERT(arfw_client);
	AR_ASSERT(arfw_client->queue);

	shutdown_event.type = AR_QUEUE_SHUTDOWN;
	shutdown_event.shutdown_reason = reason;
	arfw_client->queue->shutdown_reason = reason;

	err = arfw_queue_event_enqueue(arfw_client, &shutdown_event);
	if (err)
		AR_LOG_ARFW_QUEUE_ERR(arfw_client->queue, AR_LOG_DESTROY_QUEUE,
				      "Failed to notify queue shutdown.",
				      "err %d", err);

	lifetime_lock_trigger_shutdown(&arfw_client->lt_lock);

	return err;
}

/**
 * Driver API to trigger destruction of a client queue. This will result in an
 * async callback to the handle_client_destroy operation.
 *
 * NOTE:
 * Acquires queue lock for destruction.
 *
 * @param[in] queue handle for the current queue
 */
static void arfw_client_queue_destroy(arfw_client_queue_t client)
{
	int err;
	struct arfw_client_queue *arfw_client =
		(struct arfw_client_queue *)client;
	struct arfw_io_request req;

	AR_ASSERT(arfw_client);

	if (!arfw_client->queue)
		return;

	err = lifetime_lock_destruction_acquire(&arfw_client->lt_lock);
	if (err) {
		if (err != -ESHUTDOWN)
			AR_LOG_ARFW_QUEUE_ERR(
				arfw_client->queue, AR_LOG_DESTROY_QUEUE,
				"Failed to lock queue for shutdown", "err %d",
				err);
		return;
	}

	// consume remaining queue entries (required to fill in debug information userspace left us)
	// pcie already does this, but ar user does not.
	if (arfw_client->queue->queue_meta.queue_direction == RECEIVE)
		while (arfw_client_queue_reserve_request(client, &req) == 0)
			;

	lifetime_lock_destruction_release(&arfw_client->lt_lock);

	err = arfw_queue_destroy(arfw_client, arfw_client->queue->base_context);
	if (err)
		AR_LOG_ARFW_QUEUE_ERR(arfw_client->queue, AR_LOG_DESTROY_QUEUE,
				      "Failed to destroy queue on shutdown",
				      "err %d", err);
}

static void arfw_client_lock(void)
{
	mutex_lock(&registered_devices.lock);
}

static void arfw_client_unlock(void)
{
	mutex_unlock(&registered_devices.lock);
}

static const struct arfw_client_ops ar_client_ops = {
	.handle_client_queue_index_consumed = arfw_client_queue_index_consumed,
	.handle_client_payload_pend_required = arfw_payload_pend_required,
	.handle_client_payload_pend_released = arfw_payload_pend_released,
	.handle_client_queue_reserve_request =
		arfw_client_queue_reserve_request,
	.handle_client_queue_produce_request =
		arfw_client_queue_produce_request,
	.handle_client_queue_shutdown = arfw_client_queue_shutdown,
	.handle_client_queue_destroy = arfw_client_queue_destroy,
	.handle_client_lock = arfw_client_lock,
	.handle_client_unlock = arfw_client_unlock,
};

/**
 * Register an ar firmware device with the device manager. The struct device
 * is destroyed when the device is unregistered.
 *
 * @param[in] hw_dev The device associated with the hw ops this device may perform.
 * @param[in] device_id Unique identifier for the device. The character device will be created with this string.
 * @param[in] driver_ops The operations the driver is required to implement.
 * @param[out] client_ops The operations the arfw interface provides to the driver to interact with a client queue.
 * @param[in] base_context Context to be passed back in allt he callback operations.
 *
 * @retval 0 On success
 * @retval AR_ERR_INVALID_ARGS A device with the specified ID is already registered.
 * @retval AR_ERR_INVALID_ARGS device_id string too long.
 * @retval AR_ERR_INVALID_ARGS device id does not match [a-zA-Z0-9-], or driver_ops is invalid.
 *	 registered.
 */
static int arfw_device_register(struct device *dev, const char *device_id,
				const struct arfw_driver_ops *driver_ops,
				const struct arfw_client_ops **client_ops,
				void *base_context)
{
	int err = 0;
	bool new_device_registration = false;
	struct arfw_char_device *device = NULL;

	AR_LOG_ARFW_INFO(AR_LOG_DEV_REG, "Registering %.*s",
			 ARFW_DEVICE_ID_MAX_LEN, device_id);

	err = arfw_mem_util_check_arfw_device_id(device_id,
						 ARFW_DEVICE_ID_MAX_LEN);
	if (err) {
		AR_LOG_ARFW_ERR(AR_LOG_DEV_REG,
				"Invalid device id %.*s, err %d",
				ARFW_DEVICE_ID_MAX_LEN, device_id, err);
		goto exit;
	}

	err = check_arfw_driver_ops(driver_ops);
	if (err) {
		AR_LOG_ARFW_ERR(AR_LOG_DEV_REG,
				"Invalid driver ops for %.*s, err %d",
				ARFW_DEVICE_ID_MAX_LEN, device_id, err);
		goto exit;
	}

	err = mutex_lock_interruptible(&registered_devices.lock);
	if (err) {
		AR_LOG_ARFW_ERR(AR_LOG_DEV_REG,
				"Failed to lock registered devices, err %d",
				err);
		goto exit;
	}

	device = arfw_dev_lookup(device_id);
	if (device == NULL) {
		new_device_registration = true;
		err = arfw_char_device_create(dev, device_id, driver_ops,
					      base_context, &device);
	} else if (device->released) {
		device->released = false;
		device->driver_ops = driver_ops;
		device->base_context = base_context;
		device->hw_dev = dev;
	} else {
		AR_LOG_ARFW_ERR(AR_LOG_DEV_REG,
				"Device %.*s already registered",
				ARFW_DEVICE_ID_MAX_LEN, device_id);
		err = -EEXIST;
	}

	if (err)
		goto unlock_registered_devices;

	*client_ops = &ar_client_ops;
	if (new_device_registration)
		list_add(&device->list, &registered_devices.device_list);

	if (debugfs_initialized())
		arfw_debug_device_add(device_id);

	AR_LOG_ARFW_INFO(AR_LOG_DEV_REG, "Successfully registered %s",
			 device_id);

unlock_registered_devices:
	mutex_unlock(&registered_devices.lock);

exit:
	if (err)
		AR_LOG_ARFW_ERR(AR_LOG_DEV_REG,
				"Failed to register %.*s err %d",
				ARFW_DEVICE_ID_MAX_LEN, device_id, err);

	return err;
}

static const struct arfw_shim_cdev arfw_shim_cdev_ops = {
	.cdev_register = &arfw_device_register,
	.cdev_unregister = &arfw_device_unregister,
};

static int __init arfw_class_init(void)
{
	int ret = 0;

	mutex_init(&registered_devices.lock);
	INIT_LIST_HEAD(&registered_devices.device_list);

	AR_LOG_ARFW_INFO(AR_LOG_INIT, "Initializing arfw interface");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	device_class = class_create(AR_CLASS_NAME);
#else
	device_class = class_create(THIS_MODULE, AR_CLASS_NAME);
#endif
	if (IS_ERR(device_class)) {
		AR_LOG_ARFW_ERR(AR_LOG_INIT, "Failed to register device class");
		return PTR_ERR(device_class);
	}

	ret = arfw_shim_cdev_add(ARFW_SHIM_ID, &arfw_shim_cdev_ops);
	if (ret) {
		AR_LOG_ARFW_ERR(AR_LOG_INIT,
				"Failed to register in shim, err %d", ret);
		goto shim_add_failure;
	} else
		AR_LOG_ARFW_INFO(AR_LOG_INIT, "Initialized arfw interface");

	// Don't fail in case of the debugfs error.
	if (arfw_debug_init())
		AR_LOG_ARFW_ERR(AR_LOG_INIT,
				"Failed to initialize debugfs for the queues");

	return ret;

shim_add_failure:
	class_destroy(device_class);
	return ret;
}

static void __exit arfw_class_exit(void)
{
	int ret = 0;

	AR_LOG_ARFW_INFO(AR_LOG_SHUTDOWN, "Exiting arfw interface");

	arfw_debug_destroy();

	if (device_class) {
		class_destroy(device_class);
		device_class = NULL;
	}

	ret = arfw_shim_cdev_remove(ARFW_SHIM_ID);
	if (ret)
		AR_LOG_ARFW_ERR(AR_LOG_SHUTDOWN,
				"Failed to unregister in shim, err %d", ret);

	AR_LOG_ARFW_INFO(AR_LOG_SHUTDOWN, "Exited arfw interface");
}

void dbg_dump_queue_info(struct arfw_client_queue *client, const char *action)
{
	char buf[1024];
	char *ptr, *old_ptr;

	AR_ASSERT(ar_queue_dump(&client->queue->arfw_queue.submit_queue, buf,
				sizeof(buf)) == 0);

	ptr = buf;
	while (ptr) {
		old_ptr = ptr;
		ptr = strchr(ptr, '\n');

		if (ptr != NULL) {
			*ptr = '\0';
			ptr++;
		}

		AR_LOG_ARFW_QUEUE_DBG(client->queue, action, "Queue debug dump",
				      "%s", old_ptr);
	}
}

module_init(arfw_class_init);
module_exit(arfw_class_exit);
