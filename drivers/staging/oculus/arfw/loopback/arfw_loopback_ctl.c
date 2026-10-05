// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_loopback_ctl.c
 *
 * @brief Implementation for the arfw-loopback ctl character device operations
 *
 * @details
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include <linux/arfw_types.h>
#include <linux/anon_inodes.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/kobject.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <ar_device.h>
#include <arfw_log.h>
#include <ar_common.h>
#include <arfw_shim.h>

#include "arfw_loopback_ctl.h"

struct arfw_loopback_ctl_fd_context {
	// idr allocated handles (in user), 0 reserved as a null value (not bound)
	uint32_t queue_handle;
	// idr allocated ids (in arfw), 0 reserved as a null value (not bound)
	uint16_t region_id;
	struct arfw_loopback_ctl_device *ctl_dev;
};

static struct arfw_loopback_ctl_fd_context *get_fd_context(struct file *file)
{
	AR_ASSERT(file);
	AR_ASSERT(file->private_data);
	return (struct arfw_loopback_ctl_fd_context *)file->private_data;
}

static void
arfw_loopback_ctl_device_destroy(struct arfw_loopback_ctl_device *ctl_dev)
{
	dev_t dev;

	AR_ASSERT(ctl_dev);

	dev = ctl_dev->cdev.dev;
	cdev_del(&ctl_dev->cdev);
	device_unregister(&ctl_dev->dev);
	unregister_chrdev_region(dev, 1);
	kobject_put(&ctl_dev->kobj);
}

static void arfw_loopback_ctl_device_cleanup(struct kref *references)
{
	struct arfw_loopback_ctl_device *ctl_dev;

	AR_ASSERT(references);
	ctl_dev = container_of(references, struct arfw_loopback_ctl_device,
			       references);
	arfw_loopback_ctl_device_destroy(ctl_dev);
}

static struct arfw_loopback_ctl_fd_context *
arfw_loopback_ctl_context_alloc(struct arfw_loopback_ctl_device *ctl_dev)
{
	struct arfw_loopback_ctl_fd_context *fd_context;

	AR_ASSERT(ctl_dev);

	fd_context = kzalloc(sizeof(struct arfw_loopback_ctl_fd_context),
			     GFP_KERNEL);
	if (!fd_context)
		return NULL;
	fd_context->ctl_dev = ctl_dev;

	return fd_context;
}

static int arfw_loopback_ctl_open(struct inode *inode, struct file *file)
{
	(void)inode;
	(void)file;
	AR_LOG_USER_ERR(
		AR_LOG_CTRL,
		"The fd for the control device, should be got only through REGISTER ioctl");

	return -ENOTSUPP;
}

static int arfw_loopback_ctl_queue_lookup_and_lock(
	struct arfw_loopback_ctl_device *ctl_dev, uint32_t handle,
	struct arfw_loopback_ctl_queue_context **queue_out)
{
	int err = 0;
	unsigned long flags;
	struct arfw_loopback_ctl_queue_context *queue;

	AR_ASSERT(ctl_dev);
	AR_ASSERT(handle);
	AR_ASSERT(queue_out);

	spin_lock_irqsave(&ctl_dev->locked_queues.lock, flags);

	queue = idr_find(&ctl_dev->locked_queues.queues, handle);
	if (queue == NULL) {
		AR_LOG_USER_ERR(AR_LOG_CLIENT_LOCK,
				"Failed to find queue=%d for this device",
				handle);
		err = -EINVAL;
		goto unlock;
	}

	err = lifetime_lock_acquire(&queue->lock);
	if (err) {
		AR_LOG_USER_QUEUE_ERR(
			queue, AR_LOG_CLIENT_LOCK,
			"Failed to lock queue=%d for this device, err=%d",
			handle, err);
		goto unlock;
	}

	*queue_out = queue;

unlock:
	spin_unlock_irqrestore(&ctl_dev->locked_queues.lock, flags);

	return err;
}

static int arfw_loopback_ctl_ioctl_queue_info(
	struct arfw_loopback_ctl_fd_context *fd_context,
	struct arfw_loopback_queue_info_req __user *user_req)
{
	int err;
	struct arfw_loopback_ctl_queue_context *queue;
	struct arfw_loopback_queue_info_req req;

	AR_ASSERT(fd_context);
	AR_ASSERT(fd_context->queue_handle);

	err = arfw_loopback_ctl_queue_lookup_and_lock(
		fd_context->ctl_dev, fd_context->queue_handle, &queue);
	if (err)
		goto exit;

	AR_ASSERT(queue->info.mirror);
	req.handle = queue->info.handle;
	req.direction = queue->info.direction;
	req.element_size = queue->info.element_size;
	req.depth = queue->info.depth;
	req.hlos_endpoint_id = queue->info.hlos_endpoint;
	req.fw_endpoint_id = queue->info.fw_endpoint;

	lifetime_lock_release(&queue->lock);

	if (copy_to_user(user_req, &req,
			 sizeof(struct arfw_loopback_queue_info_req)))
		err = -EFAULT;

exit:
	return err;
}

static int arfw_loopback_ctl_ioctl_request_pend(
	struct arfw_loopback_ctl_fd_context *fd_context,
	struct arfw_loopback_pend_req __user *user_req)
{
	int err = 0;
	struct arfw_loopback_ctl_queue_context *queue;
	struct arfw_loopback_pend_req req;

	AR_ASSERT(fd_context);
	AR_ASSERT(user_req);

	if (copy_from_user(&req, user_req,
			   sizeof(struct arfw_loopback_pend_req)))
		return -EFAULT;

	err = arfw_loopback_ctl_queue_lookup_and_lock(fd_context->ctl_dev,
						      req.handle, &queue);
	if (err)
		goto exit;

	err = fd_context->ctl_dev->client_ops
		      ->handle_client_payload_pend_required(queue->arfw_queue,
							    req.size);

	lifetime_lock_release(&queue->lock);

exit:
	return err;
}

static long arfw_loopback_ctl_ioctl(struct file *file, unsigned int num,
				    unsigned long param)
{
	int err;
	struct arfw_loopback_ctl_fd_context *fd_context = get_fd_context(file);

	switch (num) {
	case ARFW_LOOPBACK_CTL_QUEUE_INFO:
		err = arfw_loopback_ctl_ioctl_queue_info(
			fd_context,
			(struct arfw_loopback_queue_info_req __user *)param);
		break;
	case ARFW_LOOPBACK_CTL_REQUEST_PEND:
		err = arfw_loopback_ctl_ioctl_request_pend(
			fd_context,
			(struct arfw_loopback_pend_req __user *)param);
		break;
	default:
		err = -EINVAL;
	}

	return err;
}

static int arfw_loopback_ctl_release(struct inode *inode, struct file *file)
{
	struct arfw_loopback_ctl_device *ctl_dev;
	struct arfw_loopback_ctl_fd_context *fd_context;
	char dev_name[ARFW_LOOPBACK_DEV_PATH_MAXLEN + 1] = { 0 };

	AR_ASSERT(inode);
	AR_ASSERT(file);

	fd_context = get_fd_context(file);
	ctl_dev = fd_context->ctl_dev;
	AR_ASSERT(ctl_dev);

	/* User ctl fd is a lifetime of the CTL device. Since the last one
	 * fd closed, the device could be successfully unregistered.
	 */
	snprintf(dev_name, ARFW_LOOPBACK_DEV_PATH_MAXLEN,
		 ARFW_LOOPBACK_DEVICE_NAME_PREFIX "%s", ctl_dev->id);
	arfw_shim_cdev_unregister(dev_name, (struct device **)&ctl_dev, false);

	arfw_loopback_ctl_device_put(ctl_dev);
	kfree(fd_context);
	file->private_data = NULL;

	return 0;
}

static const struct file_operations arfw_loopback_ctl_fops = {
	.open = arfw_loopback_ctl_open,
	.unlocked_ioctl = arfw_loopback_ctl_ioctl,
	.release = arfw_loopback_ctl_release,
};

static void arfw_loopback_ctl_dev_create_release(struct device *dev)
{
	// Since dev is embedded in a struct arfw_loopback_ctl_device, there is nothing to do
	pr_debug("release device (nothing to do): '%s': %s\n", dev_name(dev),
		 __func__);
}

int arfw_loopback_ctl_create(struct arfw_loopback_ctl_device *ctl_dev,
			     const char *name)
{
	int err;
	dev_t dev;

	AR_ASSERT(ctl_dev);
	AR_ASSERT(name);

	err = alloc_chrdev_region(&dev, 0, 1, ARFW_LOOPBACK_CLASS_NAME);
	if (err) {
		AR_LOG_USER_ERR(AR_LOG_DEV_REG,
				"Failed to allocate chrdev region [name: %s]",
				name);
		goto exit;
	}

	// init the cdev member
	cdev_init(&ctl_dev->cdev, &arfw_loopback_ctl_fops);
	ctl_dev->cdev.owner = THIS_MODULE;
	ctl_dev->cdev.kobj.parent = &ctl_dev->kobj;

	// create the /dev ctl_dev
	device_initialize(&ctl_dev->dev);
	ctl_dev->dev.devt = dev;
	ctl_dev->dev.class = arfw_loopback_device_class;
	ctl_dev->dev.release = arfw_loopback_ctl_dev_create_release;

	err = dev_set_name(&ctl_dev->dev, "%s", name);
	if (err) {
		AR_LOG_USER_ERR(AR_LOG_DEV_REG,
				"Failed to set device name [name: %s, err: %d]",
				name, err);
		goto free_chdev_region;
	}

	err = device_add(&ctl_dev->dev);
	if (err) {
		AR_LOG_USER_ERR(AR_LOG_DEV_REG,
				"Failed to add device [name: %s, err: %d]",
				name, err);
		goto free_chdev_region;
	}

	err = cdev_add(&ctl_dev->cdev, dev, 1);
	if (err) {
		AR_LOG_USER_ERR(
			AR_LOG_DEV_REG,
			"Failed to add character device [name: %s, err: %d]",
			name, err);
		goto put_dev;
	}

	AR_LOG_USER_INFO(AR_LOG_DEV_REG, "Created ctl cdev [name: %s]", name);

	goto exit;

put_dev:
	put_device(&ctl_dev->dev);
free_chdev_region:
	unregister_chrdev_region(dev, 1);
exit:
	return err;
}

int arfw_loopback_ctl_device_put(struct arfw_loopback_ctl_device *ctl_dev)
{
	AR_ASSERT(ctl_dev);
	kref_put(&ctl_dev->references, arfw_loopback_ctl_device_cleanup);
	return 0;
}

int arfw_loopback_ctl_fd(struct arfw_loopback_ctl_device *ctl_dev)
{
	int fd;
	struct arfw_loopback_ctl_fd_context *fd_context;

	AR_ASSERT(ctl_dev);
	fd_context = arfw_loopback_ctl_context_alloc(ctl_dev);
	if (!fd_context)
		return -ENOMEM;

	fd = anon_inode_getfd(dev_name(&ctl_dev->dev), &arfw_loopback_ctl_fops,
			      fd_context, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		kfree(fd_context);

	return fd;
}
