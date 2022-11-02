/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2008-2009, 2012-2014, 2016, 2018 The Linux Foundation.
 * All rights reserved.
 */

#ifndef DIAGCHAR_HDLC
#define DIAGCHAR_HDLC

enum diag_send_state_enum_type {
	DIAG_STATE_START,
	DIAG_STATE_BUSY,
	DIAG_STATE_CRC1,
	DIAG_STATE_CRC2,
	DIAG_STATE_TERM,
	DIAG_STATE_COMPLETE
};

struct diag_send_desc_type {
	const void *pkt;
	const void *last;	/* Address of last byte to send. */
	enum diag_send_state_enum_type state;
	/* True if this fragment terminates the packet */
	unsigned char terminate;
};

struct diag_hdlc_dest_type {
	void *dest;
	void *dest_last;
	/* Below: internal use only */
	uint16_t crc;
};

struct diag_hdlc_decode_type {
	uint8_t *src_ptr;
	unsigned int src_idx;
	unsigned int src_size;
	uint8_t *dest_ptr;
	unsigned int dest_idx;
	unsigned int dest_size;
	int escaping;

};

void diag_hdlc_encode(struct diag_send_desc_type *src_desc,
		      struct diag_hdlc_dest_type *enc);

int diag_hdlc_decode(struct diag_hdlc_decode_type *hdlc);

int crc_check(uint8_t *buf, uint16_t len);

#define ESC_CHAR     0x7D
#define ESC_MASK     0x20

#define HDLC_INCOMPLETE		0
#define HDLC_COMPLETE		1

#define HDLC_FOOTER_LEN		3
#endif
