// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *
 * COBS+CRC32 framing for UART transport. This is a kernel-space port of the
 * super_serial library — the wire format must be bit-for-bit identical to
 * ensure interoperability with the peer.
 *
 * Wire format (same as super_serial.c):
 *   [COBS(4-byte CRC32 header)][COBS(payload)][0x00 delimiter]
 *
 * The CRC32 is super_serial's non-standard variant (initial value 0, no final
 * XOR) — NOT the standard CRC32, so the kernel's crc32_le() does not match. The
 * lookup table below is copied verbatim from super_serial.c and kept in sync
 * with it.
 */

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>

#include "arfw_uart_framing.h"

/*
 * Pre-computed CRC32 values for a single byte, copied verbatim from
 * super_serial.c. This is super_serial's non-standard CRC32 (initial value 0,
 * no final XOR). Do NOT replace with crc32_le() — the wire bytes would diverge.
 * Source of truth (keep in sync):
 *   fbsource arvr/firmware/wearables/libs/super_serial/super_serial.c
 */
static const u32 arfw_uart_crc32_table[256] = {
	0xd202ef8d, 0xa505df1b, 0x3c0c8ea1, 0x4b0bbe37, 0xd56f2b94, 0xa2681b02,
	0x3b614ab8, 0x4c667a2e, 0xdcd967bf, 0xabde5729, 0x32d70693, 0x45d03605,
	0xdbb4a3a6, 0xacb39330, 0x35bac28a, 0x42bdf21c, 0xcfb5ffe9, 0xb8b2cf7f,
	0x21bb9ec5, 0x56bcae53, 0xc8d83bf0, 0xbfdf0b66, 0x26d65adc, 0x51d16a4a,
	0xc16e77db, 0xb669474d, 0x2f6016f7, 0x58672661, 0xc603b3c2, 0xb1048354,
	0x280dd2ee, 0x5f0ae278, 0xe96ccf45, 0x9e6bffd3, 0x0762ae69, 0x70659eff,
	0xee010b5c, 0x99063bca, 0x000f6a70, 0x77085ae6, 0xe7b74777, 0x90b077e1,
	0x09b9265b, 0x7ebe16cd, 0xe0da836e, 0x97ddb3f8, 0x0ed4e242, 0x79d3d2d4,
	0xf4dbdf21, 0x83dcefb7, 0x1ad5be0d, 0x6dd28e9b, 0xf3b61b38, 0x84b12bae,
	0x1db87a14, 0x6abf4a82, 0xfa005713, 0x8d076785, 0x140e363f, 0x630906a9,
	0xfd6d930a, 0x8a6aa39c, 0x1363f226, 0x6464c2b0, 0xa4deae1d, 0xd3d99e8b,
	0x4ad0cf31, 0x3dd7ffa7, 0xa3b36a04, 0xd4b45a92, 0x4dbd0b28, 0x3aba3bbe,
	0xaa05262f, 0xdd0216b9, 0x440b4703, 0x330c7795, 0xad68e236, 0xda6fd2a0,
	0x4366831a, 0x3461b38c, 0xb969be79, 0xce6e8eef, 0x5767df55, 0x2060efc3,
	0xbe047a60, 0xc9034af6, 0x500a1b4c, 0x270d2bda, 0xb7b2364b, 0xc0b506dd,
	0x59bc5767, 0x2ebb67f1, 0xb0dff252, 0xc7d8c2c4, 0x5ed1937e, 0x29d6a3e8,
	0x9fb08ed5, 0xe8b7be43, 0x71beeff9, 0x06b9df6f, 0x98dd4acc, 0xefda7a5a,
	0x76d32be0, 0x01d41b76, 0x916b06e7, 0xe66c3671, 0x7f6567cb, 0x0862575d,
	0x9606c2fe, 0xe101f268, 0x7808a3d2, 0x0f0f9344, 0x82079eb1, 0xf500ae27,
	0x6c09ff9d, 0x1b0ecf0b, 0x856a5aa8, 0xf26d6a3e, 0x6b643b84, 0x1c630b12,
	0x8cdc1683, 0xfbdb2615, 0x62d277af, 0x15d54739, 0x8bb1d29a, 0xfcb6e20c,
	0x65bfb3b6, 0x12b88320, 0x3fba6cad, 0x48bd5c3b, 0xd1b40d81, 0xa6b33d17,
	0x38d7a8b4, 0x4fd09822, 0xd6d9c998, 0xa1def90e, 0x3161e49f, 0x4666d409,
	0xdf6f85b3, 0xa868b525, 0x360c2086, 0x410b1010, 0xd80241aa, 0xaf05713c,
	0x220d7cc9, 0x550a4c5f, 0xcc031de5, 0xbb042d73, 0x2560b8d0, 0x52678846,
	0xcb6ed9fc, 0xbc69e96a, 0x2cd6f4fb, 0x5bd1c46d, 0xc2d895d7, 0xb5dfa541,
	0x2bbb30e2, 0x5cbc0074, 0xc5b551ce, 0xb2b26158, 0x04d44c65, 0x73d37cf3,
	0xeada2d49, 0x9ddd1ddf, 0x03b9887c, 0x74beb8ea, 0xedb7e950, 0x9ab0d9c6,
	0x0a0fc457, 0x7d08f4c1, 0xe401a57b, 0x930695ed, 0x0d62004e, 0x7a6530d8,
	0xe36c6162, 0x946b51f4, 0x19635c01, 0x6e646c97, 0xf76d3d2d, 0x806a0dbb,
	0x1e0e9818, 0x6909a88e, 0xf000f934, 0x8707c9a2, 0x17b8d433, 0x60bfe4a5,
	0xf9b6b51f, 0x8eb18589, 0x10d5102a, 0x67d220bc, 0xfedb7106, 0x89dc4190,
	0x49662d3d, 0x3e611dab, 0xa7684c11, 0xd06f7c87, 0x4e0be924, 0x390cd9b2,
	0xa0058808, 0xd702b89e, 0x47bda50f, 0x30ba9599, 0xa9b3c423, 0xdeb4f4b5,
	0x40d06116, 0x37d75180, 0xaede003a, 0xd9d930ac, 0x54d13d59, 0x23d60dcf,
	0xbadf5c75, 0xcdd86ce3, 0x53bcf940, 0x24bbc9d6, 0xbdb2986c, 0xcab5a8fa,
	0x5a0ab56b, 0x2d0d85fd, 0xb404d447, 0xc303e4d1, 0x5d677172, 0x2a6041e4,
	0xb369105e, 0xc46e20c8, 0x72080df5, 0x050f3d63, 0x9c066cd9, 0xeb015c4f,
	0x7565c9ec, 0x0262f97a, 0x9b6ba8c0, 0xec6c9856, 0x7cd385c7, 0x0bd4b551,
	0x92dde4eb, 0xe5dad47d, 0x7bbe41de, 0x0cb97148, 0x95b020f2, 0xe2b71064,
	0x6fbf1d91, 0x18b82d07, 0x81b17cbd, 0xf6b64c2b, 0x68d2d988, 0x1fd5e91e,
	0x86dcb8a4, 0xf1db8832, 0x616495a3, 0x1663a535, 0x8f6af48f, 0xf86dc419,
	0x660951ba, 0x110e612c, 0x88073096, 0xff000000,
};

static u32 arfw_uart_crc32(const u8 *data, size_t len)
{
	u32 crc = 0;
	size_t i;

	for (i = 0; i < len; i++)
		crc = arfw_uart_crc32_table[(u8)crc ^ data[i]] ^ (crc >> 8);

	return crc;
}

/*
 * Serialize CRC32 to 4 bytes in little-endian order, matching the nanocereal
 * serialization that super_serial uses for the header.
 */
static void arfw_uart_serialize_header(u32 crc, u8 *out)
{
	out[0] = (u8)(crc);
	out[1] = (u8)(crc >> 8);
	out[2] = (u8)(crc >> 16);
	out[3] = (u8)(crc >> 24);
}

static u32 arfw_uart_deserialize_header(const u8 *in)
{
	return (u32)in[0] | ((u32)in[1] << 8) | ((u32)in[2] << 16) |
	       ((u32)in[3] << 24);
}

/*
 * COBS encode a contiguous buffer. Returns the number of bytes written to
 * output, or 0 on failure.
 */
static size_t arfw_uart_cobs_encode(const u8 *in, size_t in_len, u8 *out,
				    size_t out_max)
{
	size_t worst = arfw_uart_cobs_worst_case(in_len);
	const u8 *out_start = out;
	const u8 *out_end = out + out_max;
	u8 *block_size_ptr;
	u8 block_size;
	size_t i;

	if (out_max < worst)
		return 0;

	block_size_ptr = out++;
	block_size = 1;

	for (i = 0; i < in_len; i++) {
		if (in[i] == 0) {
			*block_size_ptr = block_size;
			block_size = 1;
			block_size_ptr = out++;
		} else {
			if (out >= out_end)
				return 0;
			*out++ = in[i];
			block_size++;
			if (block_size == 0xFF) {
				*block_size_ptr = block_size;
				block_size = 1;
				block_size_ptr = out++;
			}
		}
	}
	*block_size_ptr = block_size;

	/*
	 * Encoded length is (out - out_start): the final block-size byte is stored via
	 * block_size_ptr without advancing `out`. Do NOT subtract 1 like super_serial's
	 * cobs_encode does -- that only offsets its finish_block() advance, which this
	 * port omits; subtracting under-reports each COBS section and corrupts the frame.
	 */
	return (size_t)(out - out_start);
}

/*
 * COBS decode a contiguous buffer. Returns the number of input bytes consumed,
 * or 0 on error. bytes_written is set to the number of decoded bytes.
 *
 * Note: COBS decode appends an artificial trailing zero to each block with
 * size < 0xFF. The caller must subtract 1 from bytes_written to get the
 * actual payload size (matching super_serial convention).
 */
static size_t arfw_uart_cobs_decode(const u8 *in, size_t in_len, u8 *out,
				    size_t out_max, size_t *bytes_written)
{
	const u8 *in_start = in;
	const u8 *in_end = in + in_len;
	const u8 *out_start = out;
	const u8 *out_end = out + out_max;

	*bytes_written = 0;

	while (in < in_end) {
		u8 block_size = *in++;
		u8 i;

		if (out + block_size > out_end) {
			in--;
			break;
		}

		for (i = 1; i < block_size; i++) {
			if (in >= in_end) {
				*bytes_written = 0;
				return 0;
			}
			*out++ = *in++;
		}
		if (block_size < 0xFF)
			*out++ = 0;
	}

	*bytes_written = (size_t)(out - out_start);
	return (size_t)(in - in_start);
}

int arfw_uart_frame_encode(const u8 *in, size_t in_len, u8 *out, size_t out_max,
			   size_t *out_len)
{
	u32 crc;
	u8 header[ARFW_UART_HEADER_SIZE];
	size_t header_enc_len, payload_enc_len;
	size_t needed = arfw_uart_frame_max_encoded_size(in_len);
	size_t pos = 0;

	if (out_max < needed)
		return -ENOSPC;

	crc = arfw_uart_crc32(in, in_len);
	arfw_uart_serialize_header(crc, header);

	header_enc_len = arfw_uart_cobs_encode(header, ARFW_UART_HEADER_SIZE,
					       out + pos, out_max - pos);
	if (header_enc_len == 0)
		return -ENOSPC;
	pos += header_enc_len;

	payload_enc_len =
		arfw_uart_cobs_encode(in, in_len, out + pos, out_max - pos);
	if (payload_enc_len == 0)
		return -ENOSPC;
	pos += payload_enc_len;

	out[pos++] = ARFW_UART_FRAME_DELIMITER;

	*out_len = pos;
	return 0;
}

int arfw_uart_frame_decode(const u8 *in, size_t in_len, u8 *out, size_t out_max,
			   size_t *out_len)
{
	u8 header_buf[ARFW_UART_HEADER_SIZE + 1];
	size_t header_bytes_written, header_bytes_read;
	size_t payload_bytes_written, payload_bytes_read;
	u32 expected_crc, actual_crc;

	*out_len = 0;

	if (in_len == 0)
		return -EINVAL;

	header_bytes_read = arfw_uart_cobs_decode(in, in_len, header_buf,
						  sizeof(header_buf),
						  &header_bytes_written);
	if (header_bytes_read == 0 || header_bytes_written == 0)
		return -EINVAL;

	/* Strip the artificial trailing zero appended by COBS decode */
	header_bytes_written--;
	if (header_bytes_written != ARFW_UART_HEADER_SIZE)
		return -EINVAL;

	expected_crc = arfw_uart_deserialize_header(header_buf);

	/*
	 * COBS decode appends a trailing zero that we strip afterward.
	 * The caller's buffer must have at least 1 byte of slack beyond
	 * the expected payload — which is guaranteed because the encoded
	 * payload is always larger than the decoded payload.
	 */
	payload_bytes_read = arfw_uart_cobs_decode(in + header_bytes_read,
						   in_len - header_bytes_read,
						   out, out_max,
						   &payload_bytes_written);
	if (payload_bytes_read == 0 || payload_bytes_written == 0)
		return -EINVAL;

	/* Strip the artificial trailing zero */
	payload_bytes_written--;

	if (payload_bytes_read != in_len - header_bytes_read)
		return -EINVAL;

	actual_crc = arfw_uart_crc32(out, payload_bytes_written);
	if (actual_crc != expected_crc)
		return -EINVAL;

	*out_len = payload_bytes_written;
	return 0;
}
