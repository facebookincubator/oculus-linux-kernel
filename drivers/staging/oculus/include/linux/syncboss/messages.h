/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_PROTOCOL_H
#define _SYNCBOSS_PROTOCOL_H

#include <linux/kernel.h>
#include <linux/syncboss/messages.h>
#include <uapi/linux/sched/types.h>
#include <uapi/linux/syncboss.h>

/*
 * The structs in this file are sent over-the-wire via SPI
 * to the MCU. Their format cannot be changed willy-nilly.
 * Keep this byte-packed, and in sync with the FW image.
 */

/* The header at the start of all syncboss_transactions */
struct transaction_header {
	/* Should always be SPI_DATA_MAGIC_NUM */
	u32 magic_num;
	/* Checksum for basic data integrity checks */
	u8 checksum;
} __packed;

/*
 * The bulk of syncboss_transaction's are made up of one or more syncboss_data
 * packets.  The syncboss_data structure is just a generic container for
 * various types of data (IMU, controller, control, etc.)
 */
struct syncboss_data {
	/* The data type.  This is used to interpret the data */
	u8 type;
	/* The sequence id of the request/response (or 0 if N/A) */
	u8 sequence_id;
	/* The length of the data buffer */
	u8 data_len;
	u8 data[];
} __packed;

#define NUM_PACKET_TYPES 256 /* Packet type is a u8. */

#define MAX_TRANSACTION_DATA_LENGTH \
	(SYNCBOSS_MAX_TRANSACTION_LENGTH - sizeof(struct transaction_header))

/* Union of all transaction message formats */
union transaction_data {
	u8 type;
	struct syncboss_data syncboss_data;
	u8 raw_data[MAX_TRANSACTION_DATA_LENGTH];
};

/*
 * The overall structure of the SPI transaction data,
 * in both directions.
 */
struct syncboss_transaction {
	struct transaction_header header;
	union transaction_data data;
} __packed;

/* The version of the header used by the kernel driver */
#define SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION SYNCBOSS_DRIVER_HEADER_VERSION_V1

#endif
