/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *
 * COBS+CRC32 framing for UART transport.
 */

#ifndef ARFW_UART_FRAMING_H
#define ARFW_UART_FRAMING_H

#include <linux/types.h>

#define ARFW_UART_FRAME_DELIMITER 0x00

/*
 * CRC32 header (4 bytes serialized) + COBS overhead on header (1 byte) +
 * COBS overhead on payload (~0.4%) + frame delimiter (1 byte).
 * For a payload of size N, worst-case encoded size is:
 *   cobs_worst(4) + cobs_worst(N) + 1
 * = 5 + N + ceil(N/254) + 1
 */
#define ARFW_UART_HEADER_SIZE 4
#define ARFW_UART_FRAME_OVERHEAD 6

/**
 * arfw_uart_cobs_worst_case - compute worst-case COBS encoded size
 * @input_size: size of the data to encode
 *
 * Returns the maximum number of bytes the COBS encoding could produce.
 */
static inline size_t arfw_uart_cobs_worst_case(size_t input_size)
{
	if (input_size == 0)
		return 1;
	return input_size + (input_size + 254 - 1) / 254;
}

/**
 * arfw_uart_frame_max_encoded_size - maximum encoded frame size for a payload
 * @payload_size: size of the raw payload
 *
 * Returns the worst-case total frame size on the wire.
 */
static inline size_t arfw_uart_frame_max_encoded_size(size_t payload_size)
{
	return arfw_uart_cobs_worst_case(ARFW_UART_HEADER_SIZE) +
	       arfw_uart_cobs_worst_case(payload_size) + 1;
}

/**
 * arfw_uart_frame_encode - COBS+CRC32 encode a payload into a wire frame
 * @in:      raw payload
 * @in_len:  payload length
 * @out:     output buffer (must be at least arfw_uart_frame_max_encoded_size bytes)
 * @out_max: size of output buffer
 * @out_len: (out) actual encoded frame length including delimiter
 *
 * Returns 0 on success, -ENOSPC if the output buffer is too small.
 */
int arfw_uart_frame_encode(const u8 *in, size_t in_len, u8 *out, size_t out_max,
			   size_t *out_len);

/**
 * arfw_uart_frame_decode - decode one COBS+CRC32 frame from a delimited buffer
 * @in:      encoded frame data (everything between two 0x00 delimiters, exclusive)
 * @in_len:  length of the encoded frame data
 * @out:     output buffer for the decoded payload
 * @out_max: size of output buffer
 * @out_len: (out) actual decoded payload length
 *
 * Returns 0 on success, -EINVAL on framing/CRC error, -ENOSPC if output too small.
 */
int arfw_uart_frame_decode(const u8 *in, size_t in_len, u8 *out, size_t out_max,
			   size_t *out_len);

#endif /* ARFW_UART_FRAMING_H */
