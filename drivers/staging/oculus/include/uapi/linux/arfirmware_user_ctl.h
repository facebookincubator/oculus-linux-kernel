/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*******************************************************************************
 * @file arfirmware_user_ctl.h
 *
 * @brief Interface for managing user backends for the arfirmware arfwuser kernel module.
 *
 * @details
 *
 *******************************************************************************/

#pragma once

#include <linux/arfirmware_types.h>

#ifndef __KERNEL__
#include <sys/ioctl.h>
#else
#include <linux/ioctl.h>
#endif

#define AR_USER_DEV_ID_MAX 32

/**
 * Event types that can happen on ar-user-ctl device.
 */
enum ar_user_event_type {
	AR_USER_BUFFER_PEND,
};

/**
 * Event data for each buffer pend operation.
 */
struct __packed ar_user_pend_buffer_data {
	uint16_t region_id;
	uint16_t pend_id;
};

/**
 * Event data that can be optionally passed for certain event types.
 */
union ar_user_event_data {
	struct ar_user_pend_buffer_data pend_buffer;
};

/**
 * Event data structs that can happen on ar-user-ctl device.
 */
struct __packed ar_user_event {
	enum ar_user_event_type type;
	/// The queue id associated with this event.
	uint32_t queue_handle;
	/// Direction that this queue transfers data in.
	enum ar_queue_direction direction;
	/// The hlos id of the queue for firmware IPC
	ar_endpoint_id_t hlos_endpoint_id;
	/// The fw id of the queue for firmware IPC
	ar_endpoint_id_t fw_endpoint_id;
	union ar_user_event_data data;
};

#define AR_USER_EVENT_BATCH_MAX 20

struct __packed ar_user_event_batch {
	struct ar_user_event events[AR_USER_EVENT_BATCH_MAX];
	uint16_t size;
};

/**
 * Structure used to pass user device creation parameters
 */
struct __packed ar_user_register_req {
	/// The device will end up as /dev/AR-USER-<device_id>
	char device_id[AR_USER_DEV_ID_MAX + 1];
};

/**
 * Structure used to pass user device destroy parameters
 */
struct __packed ar_user_unregister_req {
	/// The device is /dev/AR-USER-<device_id>
	char device_id[AR_USER_DEV_ID_MAX + 1];
};

/**
 * Structure used to get information for a queue id.
 */
struct __packed ar_user_queue_info_req {
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

struct __packed ar_user_pend_req {
	uint32_t handle;
	uint32_t size;
};

/**
 * Magic number for the ar-user-ctl device ioctls
 */
#define AR_USER_CTL_MAGIC 0xc6
#define AR_USER_CTL_REGISTER _IOR(AR_USER_CTL_MAGIC, 0, struct ar_user_register_req*)
#define AR_USER_CTL_UNREGISTER _IOR(AR_USER_CTL_MAGIC, 1, struct ar_user_unregister_req*)
#define AR_USER_CTL_QUEUE_INFO _IOW(AR_USER_CTL_MAGIC, 4, struct ar_user_queue_info_req*)
#define AR_USER_CTL_REQUEST_PEND _IOW(AR_USER_CTL_MAGIC, 5, struct ar_user_pend_req*)
