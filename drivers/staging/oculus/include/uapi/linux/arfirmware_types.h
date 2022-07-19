/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfirmware_types.h
 *
 * @brief Types and definitions used by ar firmware code, shared between user
 *        and kernel space.
 *
 * @details
 *
 *******************************************************************************/

#pragma once

#ifndef __KERNEL__
#include <stdint.h>
#else
#include <linux/types.h>
#endif

/**
 * Endpoints in ar firmware IPC are used to identify the sender and
 * receiver of a message. A src+dst pair uniquely identifies a session
 */
typedef uint16_t ar_endpoint_id_t;

/**
 * The message ID is used in ar firmware IPC to separate message types,
 * both at the IPC layer (ack/nack/session create/etc) as well as potentially at
 * the application layer to determine the expected payload format.
 */
typedef uint16_t ar_msg_id_t;

/**
 * The sequence ID is used in ar firmware IPC to match replies with
 * messages
 */
typedef uint16_t ar_sequence_id_t;

/**
 * The tracking ID is used in firmware IPC to match replies with the
 * original messages
 */
typedef uint8_t ar_tracking_id_t;

/// Which direction a queue sends data.
enum ar_queue_direction {
	AR_QUEUE_HLOS_TO_FW,
	AR_QUEUE_FW_TO_HLOS,
};

/// Maximum number of discrete segments in a memory region
#define AR_REGION_MAX_SEGMENTS 4

/**
 * Describes a memory segment, a contiguous virtual address range.
 */
struct ar_mem_segment {
	void *ptr;
	uint32_t size;
};

/**
 * Describes an external memory region, composed of up to AR_REGION_MAX_SEGMENTS
 * memory segments.
 */
struct ar_mem_region {
	uint16_t segment_count;
	struct ar_mem_segment segments[AR_REGION_MAX_SEGMENTS];
};

/**
 * Describes an external payload.
 */
struct ar_payload {
	/// External mem region id associated with this address range
	uint32_t mem_region_id;
	/// offset into region where the payload starts
	uint32_t offset;
	/// size of the payload
	uint32_t size;
};
