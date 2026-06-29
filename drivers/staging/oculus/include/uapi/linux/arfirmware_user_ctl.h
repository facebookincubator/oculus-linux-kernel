/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*******************************************************************************
 * @file arfw_user_ctl.h
 *
 * @brief Interface for managing user backends for the arfirmware arfwuser kernel module.
 *
 * @details
 *
 *******************************************************************************/

#pragma once

#include <linux/arfw_types.h>

#ifndef __KERNEL__
#include <sys/ioctl.h>
#else
#include <linux/ioctl.h>
#endif

#define ARFW_LOOPBACK_DEV_ID_MAX 32

/**
 * Event types that can happen on arfw-loopback-ctl device.
 */
enum arfw_loopback_event_type {
	ARFW_LOOPBACK_BUFFER_PEND,
};

/**
 * Event data for each buffer pend operation.
 */
struct __packed arfw_loopback_pend_buffer_data {
	uint16_t region_id;
	uint16_t pend_id;
};

/**
 * Event data that can be optionally passed for certain event types.
 */
union arfw_loopback_event_data {
	struct arfw_loopback_pend_buffer_data pend_buffer;
};

/**
 * Event data structs that can happen on arfw-loopback-ctl device.
 */
struct __packed arfw_loopback_event {
	enum arfw_loopback_event_type type;
	/// The queue id associated with this event.
	uint32_t queue_handle;
	/// Direction that this queue transfers data in.
	enum ar_queue_direction direction;
	/// The hlos id of the queue for firmware IPC
	ar_endpoint_id_t hlos_endpoint_id;
	/// The fw id of the queue for firmware IPC
	ar_endpoint_id_t fw_endpoint_id;
	union arfw_loopback_event_data data;
};

#define ARFW_LOOPBACK_EVENT_BATCH_MAX 20

struct __packed arfw_loopback_event_batch {
	struct arfw_loopback_event events[ARFW_LOOPBACK_EVENT_BATCH_MAX];
	uint16_t size;
};

/**
 * Structure used to pass user device creation parameters
 */
struct __packed arfw_loopback_register_req {
	/// The device will end up as /dev/ARFW-LOOPBACK-<device_id>
	char device_id[ARFW_LOOPBACK_DEV_ID_MAX + 1];
};

/**
 * Structure used to pass user device destroy parameters
 */
struct __packed arfw_loopback_unregister_req {
	/// The device is /dev/ARFW-LOOPBACK-<device_id>
	char device_id[ARFW_LOOPBACK_DEV_ID_MAX + 1];
};

/**
 * Structure used to get information for a queue id.
 */
struct __packed arfw_loopback_queue_info_req {
	/// queue id to get information for.
	uint32_t handle;
	/// Direction that this queue transfers data in.
	enum ar_queue_direction direction;
	/// The buffer size of queue elements
	uint32_t element_size;
	/// The count of elements in the queue
	uint16_t depth;
	/// The hlos id of the queue for firmware IPC
	ar_endpoint_id_t hlos_endpoint_id;
	/// The fw id of the queue for firmware IPC
	ar_endpoint_id_t fw_endpoint_id;
};

struct __packed arfw_loopback_pend_req {
	uint32_t handle;
	uint32_t size;
};

/**
 * Magic number for the arfw-loopback-ctl device ioctls
 */
#define ARFW_LOOPBACK_CTL_MAGIC 0xc6
#define ARFW_LOOPBACK_CTL_REGISTER \
	_IOR(ARFW_LOOPBACK_CTL_MAGIC, 0, struct arfw_loopback_register_req *)
#define ARFW_LOOPBACK_CTL_UNREGISTER \
	_IOR(ARFW_LOOPBACK_CTL_MAGIC, 1, struct arfw_loopback_unregister_req *)
#define ARFW_LOOPBACK_CTL_QUEUE_INFO \
	_IOW(ARFW_LOOPBACK_CTL_MAGIC, 4, struct arfw_loopback_queue_info_req *)
#define ARFW_LOOPBACK_CTL_REQUEST_PEND \
	_IOW(ARFW_LOOPBACK_CTL_MAGIC, 5, struct arfw_loopback_pend_req *)
