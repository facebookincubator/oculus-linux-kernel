/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_loopback_ctl.h
 *
 * @brief Definitions for the ar-loopback ctl character device operations
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFIRMWARE_LOOPBACK_CTL_H
#define ARFIRMWARE_LOOPBACK_CTL_H

#include "arfw_loopback_int.h"

#include <linux/arfw_loopback_ctl.h>
#include <linux/list.h>
#include <linux/mm_types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <ar_future.h>
#include <arfw_ops.h>
#include <lifetime_lock.h>

struct arfw_loopback_ctl_queue_mapping {
	// Kernel virtual memory queue data section base pointer.
	void *data_base;
	// Kernel virtual memory queue data section size.
	size_t data_size;
};

struct arfw_loopback_ctl_queue_info {
	// This stores idr allocated handles, 0 reserved as a NULL value.
	uint32_t handle;
	enum ar_queue_direction direction;
	uint32_t element_size;
	uint16_t depth;
	ar_endpoint_id_t hlos_endpoint;
	ar_endpoint_id_t fw_endpoint;
	bool mirror;
};

struct arfw_loopback_ctl_queue_context {
	struct lifetime_lock lock;
	struct arfw_loopback_ctl_queue_info info;
	struct arfw_loopback_ctl_queue_mapping mapping;
	arfw_client_queue_t *arfw_queue;
	struct arfw_loopback_ctl_queue_context *mirror_queue;
	struct arfw_loopback_ctl_device *dev;
	struct arfw_queue_mem_region *queue_data;
};

/**
 * Creates a loopback ctl char device
 *
 * @param[in] ctl_dev the ar loopback ctl device
 * @param[in] name the name to use
 */
int arfw_loopback_ctl_create(struct arfw_loopback_ctl_device *ctl_dev,
			     const char *name);

/**
 * Decref a loopback ctl device and perform a clean up when the 0 reference is reached.
 *
 * @param[in] ctl_dev the ar loopback ctl device
 */
int arfw_loopback_ctl_device_put(struct arfw_loopback_ctl_device *ctl_dev);

/**
 * Get fd for the ctl device. Return positive fd num in case of success, otherwise
 * error number.
 *
 * @param[in] ctl_dev the ar loopback ctl device
 */
int arfw_loopback_ctl_fd(struct arfw_loopback_ctl_device *ctl_dev);

#endif // !ARFIRMWARE_LOOPBACK_CTL_H
