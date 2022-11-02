/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file babel.h
 *
 * @brief Interface definiton for linux kernel babel interface.
 *        Note: This is not a "normal" babel interface, rather it can
 *              be used with a user space implementation to create one
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_FIRMWARE_BABEL_H
#define AR_FIRMWARE_BABEL_H

#include <linux/arfirmware_types.h>

#ifndef __KERNEL__
#include <sys/ioctl.h>
#else
#include <linux/ioctl.h>
#endif

/**
 * The ring id is used to track internal ring buffers between pre-set endpoints.
 * The userspace app is responsible for keeping track of which ring buffer
 * corresponds to an endpoint.
 */
typedef uint32_t babel_ring_id_t;

/**
 * This structure is used at the front of each data ring entry (send and
 * receive) for ar firmware queues. The structure itself will not be transferred
 * as part of the protocol, but instead serves as communication between library
 * and driver implementing the interface.
 *
 * This buffer maybe be written without a payload in the event that a message comes
 * in that is larger than the read size of char device. In this case, use the data_size
 * field to determine the full length of the message and read the node again using the
 * larger size. The driver will hold the mesage until a large enough read has been done.
 */
typedef struct __packed {
	/// The ring index to send/receive the message on
	babel_ring_id_t ring_id;
	/// XROS service ID
	janus_endpoint_id_t xros_endpoint_id;
	/// Firmware service ID
	janus_endpoint_id_t fw_endpoint_id;
	/**
	 * The message ID. This may be part of the session negotiation or specific to
	 * the app, informing it of the expected payload.
	 */
	janus_msg_id_t msg_id;
	/// Tracking ID, used to match replies to messages
	janus_tracking_id_t tracking_id;
	/// Sequence ID, used to match replies to requests
	janus_sequence_id_t sequence_id;
	/// Payload size being transferred.
	uint32_t data_size;
} babel_header_t;

/**
 * This struct is sent as part of the open ioctl
 *
 * The user app is responsible for providing all the fields besides
 * the ring_id which will be propulated by the driver.
 */
struct babel_open_info {
	// Janus endpoint ids
	janus_endpoint_id_t xros_endpoint_id;
	janus_endpoint_id_t fw_endpoint_id;
	// The payload size of the message (not including the linux_babel_hearder_t)
	__u32 msg_size;
	// The depth of the ringbuffer used by the kernel layer
	__u32 msg_count;
	// Buffer size for indirect messages
	__u32 buf_size;
	// Buffer pool size
	__u32 buf_count;
	// Indirect send buffer size
	__u32 send_buf_size;
	// The ring_id is populated by the driver an will be provided in the message header
	babel_ring_id_t ring_id;
};


#define BABEL_OPEN_IO _IOWR(0xFB, 0x01, struct babel_open_info)
#define BABEL_CLOSE_IO _IOW(0xFB, 0x02, babel_ring_id_t)

#endif // AR_FIRMWARE_BABEL_H
