/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#ifndef __STP_INTERFACE_ABI_H__
#define __STP_INTERFACE_ABI_H__

#define STP_INTERFACE_I2C_MSG_MAGIC (0x012C)
#define PAYLOAD_TYPE_ERRVAL (0x0001)
#define PAYLOAD_TYPE_DATALEN (0x0020)

struct stp_interface_msg_header {
	u8 csum;
	__le16 magic;
	u8 seqnum;
	__le16 payload_len;
	__le16 reserved1;
} __packed;

struct stp_interface_msg_payload {
	__le16 flags;
	__le16 dev_addr;
	__le16 reg_addr;
	u8 mode_read;
	union {
		__le32 datalen;
		__le32 errval;
	} u;
	u8 data[0];
} __packed;

#endif
