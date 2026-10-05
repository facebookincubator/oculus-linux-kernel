/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_loopback_int.h
 *
 * @brief Internal definitions for arfw-loopback module
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFIRMWARE_LOOPBACK_INT_H
#define ARFIRMWARE_LOOPBACK_INT_H

#include <linux/cdev.h>
#include <linux/idr.h>
#include <linux/kref.h>
#include <linux/spinlock.h>

#define ARFW_LOOPBACK_DEV_PATH_MAXLEN 128

#define ARFW_LOOPBACK_CLASS_NAME "arfw_loopback"
extern struct class *arfw_loopback_device_class;

struct arfw_loopback_ctl_device {
	/**
	 * Device handle for ctl cdev.
	 * Needs to be first in the struct because we are having the arfw layer track
	 * these objects for us, and we need to be able to cast the struct device
	 * back to a arfw_loopback_ctl_device.
	 * See: arfw_device_unregister.
	 */
	struct device dev;
	// ctl cdev
	struct cdev cdev;
	// device id
	char *id;
	// callbacks into the client
	const struct arfw_client_ops *client_ops;
	// map from queue id to queue context
	struct {
		spinlock_t lock;
		struct idr queues;
	} locked_queues;
	// used to delay the device deletion until all fds closed
	struct kref references;
	// used to sync the device deletion
	struct kobject kobj;
	// used to track all the CTL devices created
	struct list_head list;
};

#endif // !ARFIRMWARE_LOOPBACK_INT_H
