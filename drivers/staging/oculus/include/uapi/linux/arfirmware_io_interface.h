/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfirmware_io_interface.h
 *
 * @brief Interface definiton for linux kernel arfirmware character devices.
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

/**
 * Structure used to pass queue creation parameters
 */
struct __packed ar_queue_create_req {
	/// Direction that this queue transfers data in.
	enum ar_queue_direction queue_direction;
	/// The buffer size of queue elements
	uint32_t element_size;
	/// The count of elements in the queue
	uint16_t depth;
	/// The hlos id of the queue for firmware IPC
	ar_endpoint_id_t hlos_endpoint_id;
	/// The fw id of the queue for firmware IPC
	ar_endpoint_id_t fw_endpoint_id;
	/// Where the queue resides.
	struct ar_mem_segment queue_segment;
};

/**
 * Structure containing device information
 */
struct __packed ar_device_information_req {
	uint16_t transport_header_size;
	uint16_t inline_data_offset;
	uint16_t send_ring_max;
	uint16_t rcv_ring_max;
	bool require_contiguous_memory_for_queues;
};

/**
 * Structure for registering a region
 */
struct __packed ar_region_register_req {
	struct ar_mem_region region;
};

/// Maximum number of chunks in a pend req
#define AR_PEND_MAX_CHUNKS 4

/**
 * Structure for pending a payload
 */
struct __packed ar_pend_payload_req {
	/// External mem region id associated with this address range
	uint32_t mem_region_id;
	/// payload chunk count
	uint16_t chunk_count;
	/// payload chunks
	struct ar_payload_chunk payload_chunks[AR_PEND_MAX_CHUNKS];
};

/**
 * Queue event/status struct
 */
struct __packed ar_queue_event {
	enum ar_queue_event_type type;
	union {
		uint32_t pend_size;
		void *payload_context;
	};
};

/// Magic number for ARFW device ioctls
#define ARFW_CHDEV_MAGIC 0xc5
#define ARFW_QUEUE_CREATE _IOR(ARFW_CHDEV_MAGIC, 0, struct ar_queue_create_req*)
#define ARFW_REGISTER_REGION _IOR(ARFW_CHDEV_MAGIC, 1, struct ar_region_register_req*)
#define ARFW_UNREGISTER_REGION _IOR(ARFW_CHDEV_MAGIC, 2, long)
#define ARFW_PEND_PAYLOAD _IOR(ARFW_CHDEV_MAGIC, 3, struct ar_pend_payload_req*)
#define ARFW_CONSUMED_INDEX _IO(ARFW_CHDEV_MAGIC, 4)
#define ARFW_DEV_INFO _IOW(ARFW_CHDEV_MAGIC, 5, struct ar_device_information_req*)
