/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __LINUX_USB_PDFU_H
#define __LINUX_USB_PDFU_H

#define PD_MAX_EXT_MSG_LEN	260

/*
 * PDFU header
 * -----------
 * Byte 0 :: ProtocolVersion
 * Byte 1 :: MessageType
 */
#define PDFU_HEADER(rev, type)	\
	(((rev) & 0xff) |			\
	((((type) & 0xff) << 8)))

struct pdfu_header {
	u8 protocol_version;
	u8 msg_type;
} __packed;

#define PDFU_REV10	0x01

/* Firmware Update Message Request Types */
enum pdfu_req_msg_type {
	/* 0x00 - 0x7F Only used by Responses */
	/* 0x80 Reserved */
	REQ_GET_FW_ID = 0x81,
	REQ_PDFU_INITIATE = 0x82,
	REQ_PDFU_DATA = 0x83,
	REQ_PDFU_DATA_NR = 0x84,
	REQ_PDFU_VALIDATE = 0x85,
	REQ_PDFU_ABORT = 0x86,
	/* 0x88 - 0xFE Reserved */
	REQ_VENDOR_SPECIFIC = 0xFF,
};

/* Firmware Update Message Response Types */
enum pdfu_resp_msg_type {
	/* 0x00 Reserved */
	RESP_GET_FW_ID = 0x01,
	RESP_PDFU_INITIATE = 0x02,
	RESP_PDFU_DATA = 0x03,
	/* 0x04 Reserved */
	RESP_PDFU_VALIDATE = 0x05,
	/* 0x06 Reserved */
	RESP_PDFU_DATA_PAUSE = 0x07,
	/* 0x08 - 0x7E Reserved */
	RESP_VENDOR_SPECIFIC = 0x7F,
	/* 0x80 - 0xFF Only used by Requests */
};

/* TODO(T181477506) Use a union for payload types) */
/* PDFU Messages */
struct pdfu_message {
	struct pdfu_header header;
	u8 payload[PD_MAX_EXT_MSG_LEN - sizeof(struct pdfu_header)];
} __packed;

struct get_fw_id_response_payload {
	u8 status;
	u16 vid;
	u16 pid;
	u8 hw_version;
	u8 si_version;
	u16 fw_version1;
	u16 fw_version2;
	u16 fw_version3;
	u16 fw_version4;
	u8 image_bank;
	u8 flags1;
	u8 flags2;
	u8 flags3;
	u8 flags4;
} __packed;

#define PDFU_DATA_BLOCK_MAX_SIZE	256
#define PDFU_WAIT_TIME_DONE			255 /* in context of PDFU_DATA */
#define PDFU_WAIT_TIME_ERROR		255 /* in all other contexts */

struct pdfu_data_request_payload {
	u16 data_block_index;
	u8 data_block[PDFU_DATA_BLOCK_MAX_SIZE];
} __packed;

struct pdfu_initiate_request_payload {
	u16 fw_version1;
	u16 fw_version2;
	u16 fw_version3;
	u16 fw_version4;
} __packed;

struct pdfu_initiate_response_payload {
	u8 status;
	u8 wait_time;
	u8 max_image_size[3];
} __packed;

struct pdfu_data_response_payload {
	u8 status;
	u8 wait_time;
	u8 num_data_nr;
	u16 data_block_num;
} __packed;

struct pdfu_validate_response_payload {
	u8 status;
	u8 wait_time;
	u8 flags;
} __packed;

/* PDFU timers and counters */
#define PDFU_T_NEXT_REQUEST_SENT		27 /* in ms */
#define PDFU_T_RESPONSE_RECVD_UNCHUNKED	60 /* in ms */
#define PDFU_T_RESPONSE_RECVD_CHUNKED	30 /* in ms, per exchanged chunk */

#define PDFU_N_ENUMERATE_RESEND		10
#define PDFU_N_RECONFIGURE_RESEND	3
#define PDFU_N_VALIDATE_RESEND		3

#endif /* __LINUX_USB_PDFU_H  */
