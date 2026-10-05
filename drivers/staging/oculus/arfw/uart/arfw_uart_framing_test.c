// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *
 * KUnit tests for the UART transport COBS+CRC32 framing layer. The framing
 * code is a clean-room port of the super_serial library and must
 * stay bit-for-bit wire-compatible with it; the golden-vector cases below lock
 * the exact on-wire bytes so a regression in the COBS/CRC math is caught here
 * rather than as a silent link failure on device.
 *
 * The framing unit has no kernel dependencies to mock, so the .c is spliced in
 * directly to reach the static helpers (CRC32, COBS) for fine-grained coverage.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "arfw_uart_framing.c"

/* Scratch buffer size for test payloads/frames, comfortably above any case. */
#define ARFW_UART_TEST_BUF_SIZE 4096

/*
 * Encode @payload, then decode the result and assert the round-trip reproduces
 * the original bytes. The encoder appends a trailing delimiter; the decoder
 * expects the bytes between delimiters (exclusive), i.e. enc_len - 1.
 */
static void framing_assert_roundtrip(struct kunit *test, const u8 *payload,
				     size_t len)
{
	/* Buffers are heap-allocated: the kernel caps on-stack frame size. */
	u8 *enc = kunit_kzalloc(test, ARFW_UART_TEST_BUF_SIZE, GFP_KERNEL);
	u8 *dec = kunit_kzalloc(test, ARFW_UART_TEST_BUF_SIZE, GFP_KERNEL);
	size_t enc_len = 0, dec_len = 0;
	int ret;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, enc);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dec);

	ret = arfw_uart_frame_encode(payload, len, enc, ARFW_UART_TEST_BUF_SIZE,
				     &enc_len);
	KUNIT_ASSERT_EQ_MSG(test, ret, 0, "encode failed for len=%zu", len);
	KUNIT_ASSERT_GT(test, enc_len, (size_t)0);
	KUNIT_EXPECT_EQ(test, enc[enc_len - 1], (u8)ARFW_UART_FRAME_DELIMITER);

	ret = arfw_uart_frame_decode(enc, enc_len - 1, dec,
				     ARFW_UART_TEST_BUF_SIZE, &dec_len);
	KUNIT_ASSERT_EQ_MSG(test, ret, 0, "decode failed for len=%zu", len);
	KUNIT_EXPECT_EQ_MSG(test, dec_len, len, "len mismatch for len=%zu",
			    len);
	if (len)
		KUNIT_EXPECT_EQ_MSG(test, memcmp(dec, payload, len), 0,
				    "payload mismatch for len=%zu", len);
}

static void test_crc32_known_values(struct kunit *test)
{
	const u8 ab[2] = { 0x41, 0x42 };

	/* CRC32 over zero bytes is the table's initial value, 0. */
	KUNIT_EXPECT_EQ(test, arfw_uart_crc32(NULL, 0), (u32)0);
	/* "AB" -> 0x30694c07 (little-endian header 07 4c 69 30 on the wire). */
	KUNIT_EXPECT_EQ(test, arfw_uart_crc32(ab, sizeof(ab)), (u32)0x30694c07);
}

static void test_roundtrip_empty(struct kunit *test)
{
	framing_assert_roundtrip(test, NULL, 0);
}

static void test_roundtrip_ascii(struct kunit *test)
{
	framing_assert_roundtrip(test, "hello", 5);
}

static void test_roundtrip_embedded_zeros(struct kunit *test)
{
	const u8 p[8] = { 1, 0, 2, 0, 0, 3, 0, 4 };

	framing_assert_roundtrip(test, p, sizeof(p));
}

static void test_roundtrip_all_zeros(struct kunit *test)
{
	u8 p[16];

	memset(p, 0, sizeof(p));
	framing_assert_roundtrip(test, p, sizeof(p));
}

/*
 * COBS inserts a code byte every 254 non-zero bytes; exercise the block_size
 * == 0xFF rollover at and around that boundary.
 */
static void test_roundtrip_cobs_254_boundary(struct kunit *test)
{
	u8 *p = kunit_kzalloc(test, 520, GFP_KERNEL);
	const size_t sizes[] = { 253, 254, 255, 508, 509 };
	size_t s, i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	for (s = 0; s < ARRAY_SIZE(sizes); s++) {
		for (i = 0; i < sizes[s]; i++)
			p[i] = (u8)((i % 255) + 1); /* never zero */
		framing_assert_roundtrip(test, p, sizes[s]);
	}
}

static void test_roundtrip_max_payload(struct kunit *test)
{
	u8 *p = kunit_kzalloc(test, ARFW_UART_TEST_BUF_SIZE, GFP_KERNEL);
	size_t len = 1500;
	size_t i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	for (i = 0; i < len; i++)
		p[i] = (u8)(i * 7 + 1);
	framing_assert_roundtrip(test, p, len);
}

/* Golden wire vector: payload {0x41,0x42} must encode to exact bytes. */
static void test_golden_vector_ascii(struct kunit *test)
{
	const u8 payload[2] = { 0x41, 0x42 };
	const u8 expect[] = { 0x05, 0x07, 0x4c, 0x69, 0x30,
			      0x03, 0x41, 0x42, 0x00 };
	u8 enc[32];
	size_t enc_len = 0;
	int ret;

	ret = arfw_uart_frame_encode(payload, sizeof(payload), enc, sizeof(enc),
				     &enc_len);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_EQ(test, enc_len, sizeof(expect));
	KUNIT_EXPECT_EQ(test, memcmp(enc, expect, sizeof(expect)), 0);
}

/* Golden wire vector: payload {0x01,0x00,0x02} (contains a zero byte). */
static void test_golden_vector_with_zero(struct kunit *test)
{
	const u8 payload[3] = { 0x01, 0x00, 0x02 };
	const u8 expect[] = { 0x05, 0x09, 0xd2, 0x8d, 0x10,
			      0x02, 0x01, 0x02, 0x02, 0x00 };
	u8 enc[32];
	size_t enc_len = 0;
	int ret;

	ret = arfw_uart_frame_encode(payload, sizeof(payload), enc, sizeof(enc),
				     &enc_len);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_EQ(test, enc_len, sizeof(expect));
	KUNIT_EXPECT_EQ(test, memcmp(enc, expect, sizeof(expect)), 0);
}

/* A single-bit corruption in the payload must fail the CRC check. */
static void test_decode_crc_corruption(struct kunit *test)
{
	const u8 payload[6] = { 'a', 'b', 'c', 'd', 'e', 'f' };
	u8 enc[64], dec[64];
	size_t enc_len = 0, dec_len = 0;
	int ret;

	ret = arfw_uart_frame_encode(payload, sizeof(payload), enc, sizeof(enc),
				     &enc_len);
	KUNIT_ASSERT_EQ(test, ret, 0);

	enc[enc_len - 2] ^= 0xFF; /* flip a payload byte before the delimiter */

	ret = arfw_uart_frame_decode(enc, enc_len - 1, dec, sizeof(dec),
				     &dec_len);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

/* A truncated frame must be rejected, not silently decoded. */
static void test_decode_truncated(struct kunit *test)
{
	const u8 payload[6] = { 'a', 'b', 'c', 'd', 'e', 'f' };
	u8 enc[64], dec[64];
	size_t enc_len = 0, dec_len = 0;
	int ret;

	ret = arfw_uart_frame_encode(payload, sizeof(payload), enc, sizeof(enc),
				     &enc_len);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = arfw_uart_frame_decode(enc, 2, dec, sizeof(dec), &dec_len);
	KUNIT_EXPECT_NE(test, ret, 0);
}

/* Encoding into a buffer smaller than the worst case must report -ENOSPC. */
static void test_encode_no_space(struct kunit *test)
{
	const u8 payload[10] = { 0 };
	u8 enc[4];
	size_t enc_len = 0;
	int ret;

	ret = arfw_uart_frame_encode(payload, sizeof(payload), enc, sizeof(enc),
				     &enc_len);
	KUNIT_EXPECT_EQ(test, ret, -ENOSPC);
}

static struct kunit_case arfw_uart_framing_test_cases[] = {
	KUNIT_CASE(test_crc32_known_values),
	KUNIT_CASE(test_roundtrip_empty),
	KUNIT_CASE(test_roundtrip_ascii),
	KUNIT_CASE(test_roundtrip_embedded_zeros),
	KUNIT_CASE(test_roundtrip_all_zeros),
	KUNIT_CASE(test_roundtrip_cobs_254_boundary),
	KUNIT_CASE(test_roundtrip_max_payload),
	KUNIT_CASE(test_golden_vector_ascii),
	KUNIT_CASE(test_golden_vector_with_zero),
	KUNIT_CASE(test_decode_crc_corruption),
	KUNIT_CASE(test_decode_truncated),
	KUNIT_CASE(test_encode_no_space),
	{},
};

static struct kunit_suite arfw_uart_framing_test_suite = {
	.name = "arfw-uart-framing",
	.test_cases = arfw_uart_framing_test_cases,
};

/*
 * The engine suite lives in arfw_uart_dev_test.c (same module). Both suites are
 * registered here with a single kunit_test_suites() because kunit_test_suite()
 * expands to module_init/module_exit, which may appear only once per module.
 */
extern struct kunit_suite arfw_uart_dev_test_suite;
kunit_test_suites(&arfw_uart_framing_test_suite, &arfw_uart_dev_test_suite);

MODULE_DESCRIPTION("KUnit tests for AR firmware UART framing and engine");
MODULE_LICENSE("GPL v2");
