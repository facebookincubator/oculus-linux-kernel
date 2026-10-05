// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_loopbackc
 *
 * @brief User-level rerouting layer driver for AR FW IPC
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include <linux/arfw_loopback_ctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <ar_common.h>
#include <ar_device.h>
#include <arfw_log.h>

#include <arfw_shim.h>
#include <arfw_mem_util.h>

#include "arfw_loopback_ctl.h"
#include "arfw_loopback_dev.h"
#include "arfw_loopback_int.h"

MODULE_LICENSE("GPL");

struct arfw_loopback_device {
	struct cdev cdev;
	struct device *dev;
	struct kobject kobj;

	// Track all the CTL devices created and used in the system.
	struct list_head ctl_dev_list;
	spinlock_t ctl_dev_lock;
};

struct class *arfw_loopback_device_class;

static struct arfw_loopback_device *arfw_loopback_dev;

static const struct arfw_driver_ops arfw_loopback_dev_ops = {
	.handle_send_queue = arfw_loopback_dev_handle_send_queue_request,
	.handle_rcv_queue_consume = arfw_loopback_dev_handle_rcv_queue_consume,
	.handle_queue_data_alloc = arfw_loopback_dev_handle_queue_data_alloc,
	.handle_queue_data_free = arfw_loopback_dev_handle_queue_data_free,
	.handle_queue_create = arfw_loopback_dev_handle_queue_create_request,
	.handle_queue_destroy = arfw_loopback_dev_handle_queue_destroy_request,
	.handle_get_device_information =
		arfw_loopback_dev_handle_get_device_information,
	.handle_receive_payload_pend =
		arfw_loopback_dev_handle_receive_payload_pend,
	.handle_notify_queue_ready =
		arfw_loopback_dev_handle_notify_queue_ready,
	.handle_queue_data_mmap = arfw_loopback_dev_handle_queue_data_mmap,
};

static void arfw_loopback_ktype_release(struct kobject *kobj)
{
	struct arfw_loopback_device *dev =
		container_of(kobj, struct arfw_loopback_device, kobj);

	AR_LOG_USER_DEV_INFO(arfw_loopback_dev->dev, AR_LOG_DEV_UNREG,
			     "Deleted arfw-loopback device");

	kfree(dev);
}

static struct kobj_type arfw_loopback_ktype = {
	.release = &arfw_loopback_ktype_release,
};

static void arfw_loopback_ctl_ktype_release(struct kobject *kobj)
{
	unsigned long flags;
	struct arfw_loopback_ctl_device *ctl_dev =
		container_of(kobj, struct arfw_loopback_ctl_device, kobj);

	AR_LOG_USER_DEV_INFO(arfw_loopback_dev->dev, AR_LOG_DEV_UNREG,
			     "Deleted arfw-loopback-ctl device [id: %s]",
			     ctl_dev->id);

	spin_lock_irqsave(&arfw_loopback_dev->ctl_dev_lock, flags);
	list_del(&ctl_dev->list);
	spin_unlock_irqrestore(&arfw_loopback_dev->ctl_dev_lock, flags);

	kfree(ctl_dev->id);
	kfree(ctl_dev);
}

static struct kobj_type arfw_loopback_ctl_ktype = {
	.release = &arfw_loopback_ctl_ktype_release,
};

/**
 * Look up for the device with the same id in the system. If the device is
 * found, then reference will be get for this device.
 *
 * @retval The pointer to the existing device in case of success, otherwise
 * return NULL.
 */
static struct arfw_loopback_ctl_device *
arfw_loopback_ctl_lookup_and_get(const char *id)
{
	unsigned long flags;
	struct arfw_loopback_ctl_device *ctl = NULL;

	spin_lock_irqsave(&arfw_loopback_dev->ctl_dev_lock, flags);
	list_for_each_entry(ctl, &arfw_loopback_dev->ctl_dev_list, list) {
		if (strcmp(id, ctl->id) == 0) {
			// If the refcounter is 0, then device is already scheduled
			// for clean up. In this case this device couldn't be used.
			if (!kref_get_unless_zero(&ctl->references))
				break;

			// Device found.
			spin_unlock_irqrestore(&arfw_loopback_dev->ctl_dev_lock,
					       flags);
			return ctl;
		}
	}
	spin_unlock_irqrestore(&arfw_loopback_dev->ctl_dev_lock, flags);

	return NULL;
}

/**
 * Allocate new arfw_loopback CTL device.
 *
 * @retval The pointer to the newly created device, otherwise return NULL.
 */
static struct arfw_loopback_ctl_device *arfw_loopback_ctl_alloc(const char *id)
{
	int err;
	unsigned long flags;
	struct arfw_loopback_ctl_device *ctl_dev;
	char name[ARFW_LOOPBACK_DEV_PATH_MAXLEN + 1] = { 0 };

	err = snprintf(name, ARFW_LOOPBACK_DEV_PATH_MAXLEN,
		       ARFW_LOOPBACK_DEVICE_NAME_PREFIX
		       "%s" ARFW_LOOPBACK_CHAR_DEVICE_CTL_SUFFIX,
		       id);
	if (err < 0 || err >= ARFW_LOOPBACK_DEV_PATH_MAXLEN)
		return NULL;

	ctl_dev = kzalloc(sizeof(struct arfw_loopback_ctl_device), GFP_KERNEL);
	if (!ctl_dev)
		return NULL;

	spin_lock_init(&ctl_dev->locked_queues.lock);
	idr_init(&ctl_dev->locked_queues.queues);
	kref_init(&ctl_dev->references);

	ctl_dev->id = kstrdup(id, GFP_KERNEL);
	if (!ctl_dev->id)
		goto free_dev;

	kobject_init(&ctl_dev->kobj, &arfw_loopback_ctl_ktype);

	err = arfw_loopback_ctl_create(ctl_dev, name);
	if (err)
		goto free_id;

	spin_lock_irqsave(&arfw_loopback_dev->ctl_dev_lock, flags);
	list_add(&ctl_dev->list, &arfw_loopback_dev->ctl_dev_list);
	spin_unlock_irqrestore(&arfw_loopback_dev->ctl_dev_lock, flags);

	return ctl_dev;

free_id:
	kfree(ctl_dev->id);
free_dev:
	kfree(ctl_dev);
	return NULL;
}

/**
 * Register a new arfw-loopback device and provide file descriptor of the CTL device to the user
 * space.
 *
 * @retval fd number on success, negative value otherwise
 */
static int arfw_loopback_register(const char *device_id)
{
	int err = 0;
	struct arfw_loopback_ctl_device *ctl_dev;
	char dev_name[ARFW_LOOPBACK_DEV_PATH_MAXLEN + 1] = { 0 };
	int fd;

	err = arfw_mem_util_check_arfw_device_id(device_id,
						 ARFW_LOOPBACK_DEV_ID_MAX);
	if (err)
		goto exit;

	ctl_dev = arfw_loopback_ctl_lookup_and_get(device_id);
	// If device isn't in the system, then allocate a new one.
	if (!ctl_dev) {
		// There is a possibility that two arfw_loopback_register calls
		// reach this point with the same device_id, because of the
		// race in the system. That is okay, since only one
		// arfw_shim_cdev_register() will succeed. And the
		// one call which fail with registration will make a clean up.
		ctl_dev = arfw_loopback_ctl_alloc(device_id);
		if (!ctl_dev) {
			err = -ENOMEM;
			goto exit;
		}
	}

	err = snprintf(dev_name, ARFW_LOOPBACK_DEV_PATH_MAXLEN,
		       ARFW_LOOPBACK_DEVICE_NAME_PREFIX "%s", device_id);
	if (err < 0 || err >= ARFW_LOOPBACK_DEV_PATH_MAXLEN) {
		err = -EINVAL;
		goto destroy_ctl;
	}
	err = arfw_shim_cdev_register(&ctl_dev->dev, dev_name,
				      &arfw_loopback_dev_ops,
				      &ctl_dev->client_ops, ctl_dev);
	if (err)
		goto destroy_ctl;

	fd = arfw_loopback_ctl_fd(ctl_dev);
	if (fd < 0) {
		err = fd;
		goto destroy_ctl;
	}

	AR_LOG_USER_DEV_INFO(arfw_loopback_dev->dev, AR_LOG_DEV_REG,
			     "Registered device [id: %s]", device_id);

	return fd;

destroy_ctl:
	arfw_loopback_ctl_device_put(ctl_dev);
exit:
	AR_LOG_USER_DEV_ERR(arfw_loopback_dev->dev, AR_LOG_DEV_REG,
			    "Failed to register device [id: %s, err: %d]",
			    device_id, err);
	return err;
}

/**
 * Unregister an arfw-loopback device
 * @retval 0 on success, negative value otherwise
 */
static int arfw_loopback_unregister(char *device_id)
{
	int err;
	struct arfw_loopback_ctl_device *ctl_dev;
	char dev_name[ARFW_LOOPBACK_DEV_PATH_MAXLEN + 1] = { 0 };

	snprintf(dev_name, ARFW_LOOPBACK_DEV_PATH_MAXLEN,
		 ARFW_LOOPBACK_DEVICE_NAME_PREFIX "%s", device_id);
	err = arfw_shim_cdev_unregister(dev_name, (struct device **)&ctl_dev,
					false);
	if (err)
		goto exit;

	AR_LOG_USER_DEV_INFO(arfw_loopback_dev->dev, AR_LOG_DEV_UNREG,
			     "Unregistered device [id: %s]", device_id);

	return 0;

exit:
	AR_LOG_USER_DEV_ERR(arfw_loopback_dev->dev, AR_LOG_DEV_UNREG,
			    "Failed to unregister device [id: %s, err: %d]",
			    device_id, err);
	return err;
}

static int arfw_loopback_handle_ioctl_register(
	struct arfw_loopback_register_req __user *user_req)
{
	struct arfw_loopback_register_req req;

	if (copy_from_user(&req, user_req,
			   sizeof(struct arfw_loopback_register_req)))
		return -EFAULT;

	return arfw_loopback_register(req.device_id);
}

static int arfw_loopback_handle_ioctl_unregister(
	struct arfw_loopback_unregister_req __user *user_req)
{
	struct arfw_loopback_unregister_req req;

	if (copy_from_user(&req, user_req,
			   sizeof(struct arfw_loopback_unregister_req)))
		return -EFAULT;

	return arfw_loopback_unregister(req.device_id);
}

static long arfw_loopback_ioctl(struct file *file, unsigned int ioctl_num,
				unsigned long ioctl_param)
{
	long ret = 0;

	switch (ioctl_num) {
	case ARFW_LOOPBACK_CTL_REGISTER:
		ret = arfw_loopback_handle_ioctl_register((
			struct arfw_loopback_register_req __user *)ioctl_param);
		break;
	case ARFW_LOOPBACK_CTL_UNREGISTER:
		ret = arfw_loopback_handle_ioctl_unregister(
			(struct arfw_loopback_unregister_req __user *)
				ioctl_param);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations arfw_loopback_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = arfw_loopback_ioctl,
};

static int __init arfw_loopback_init(void)
{
	int err = 0;
	dev_t dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	arfw_loopback_device_class = class_create(ARFW_LOOPBACK_CLASS_NAME);
#else
	arfw_loopback_device_class =
		class_create(THIS_MODULE, ARFW_LOOPBACK_CLASS_NAME);
#endif
	if (IS_ERR(arfw_loopback_device_class)) {
		AR_LOG_USER_ERR(AR_LOG_INIT, "Failed to register device class");
		return PTR_ERR(arfw_loopback_device_class);
	}

	arfw_loopback_dev =
		kzalloc(sizeof(struct arfw_loopback_device), GFP_KERNEL);
	if (!arfw_loopback_dev) {
		err = -ENOMEM;
		goto free_class;
	}

	INIT_LIST_HEAD(&arfw_loopback_dev->ctl_dev_list);
	spin_lock_init(&arfw_loopback_dev->ctl_dev_lock);

	kobject_init(&arfw_loopback_dev->kobj, &arfw_loopback_ktype);

	err = alloc_chrdev_region(&dev, 0, 1, ARFW_LOOPBACK_CLASS_NAME);
	if (err) {
		AR_LOG_USER_ERR(AR_LOG_DEV_REG,
				"Failed to allocate chrdev region [id: %s]",
				ARFW_LOOPBACK_DEV_NAME);
		goto free_arfw_loopback_device;
	}

	cdev_init(&arfw_loopback_dev->cdev, &arfw_loopback_fops);
	arfw_loopback_dev->cdev.owner = THIS_MODULE;
	arfw_loopback_dev->cdev.kobj.parent = &arfw_loopback_dev->kobj;

	arfw_loopback_dev->dev = device_create(arfw_loopback_device_class, NULL,
					       dev, NULL, "%s",
					       ARFW_LOOPBACK_DEV_NAME);
	if (IS_ERR(arfw_loopback_dev->dev)) {
		AR_LOG_USER_ERR(AR_LOG_DEV_REG,
				"Failed to create the device [id: %s]",
				ARFW_LOOPBACK_DEV_NAME);
		err = PTR_ERR(arfw_loopback_dev->dev);
		goto free_chdev_region;
	}

	err = cdev_add(&arfw_loopback_dev->cdev, dev, 1);
	if (err) {
		AR_LOG_USER_DEV_ERR(arfw_loopback_dev->dev, AR_LOG_DEV_REG,
				    "Failed to add cdev");
		goto free_device;
	}

	AR_LOG_USER_INFO(AR_LOG_INIT, "arfw-loopback module init");

	return 0;

free_device:
	device_destroy(arfw_loopback_device_class, dev);

free_chdev_region:
	unregister_chrdev_region(dev, 1);

free_arfw_loopback_device:
	kfree(arfw_loopback_dev);
	arfw_loopback_dev = NULL;

free_class:
	class_destroy(arfw_loopback_device_class);
	arfw_loopback_device_class = NULL;

	return err;
}

static void __exit arfw_loopback_exit(void)
{
	dev_t dev;

	AR_LOG_USER_INFO(AR_LOG_SHUTDOWN, "arfw-loopback module exit");

	if (arfw_loopback_dev) {
		dev = arfw_loopback_dev->cdev.dev;
		device_destroy(arfw_loopback_device_class, dev);
		cdev_del(&arfw_loopback_dev->cdev);
		unregister_chrdev_region(dev, 1);
		kfree(arfw_loopback_dev);
		arfw_loopback_dev = NULL;
	}

	if (arfw_loopback_device_class)
		class_destroy(arfw_loopback_device_class);
}

module_init(arfw_loopback_init);
module_exit(arfw_loopback_exit);
