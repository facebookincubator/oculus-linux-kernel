// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *
 * KUnit tests for the transport-agnostic UART engine (arfw_uart_dev.c). The
 * framing suite (arfw_uart_framing_test.c) locks the wire codec; this suite
 * exercises the engine that sits above it -- the babel<->arfw header
 * translation, the TX/RX bounds checks (where every reviewed bug lived), the
 * RX frame routing, and queue-slot reuse.
 *
 * Design note (testing-anti-patterns):
 *   - No test-only hooks are added to production. The engine is driven through
 *     its real seams: a fake arfw_client_ops captures the produce/reserve calls
 *     the engine makes back into the framework, and a fake tty captures the
 *     bytes the engine writes out (tty->ops->write).
 *   - The .c is spliced in directly (like the framing test) to reach the static
 *     engine functions and the otherwise-opaque struct arfw_uart_driver, rather
 *     than de-static-ing production code for the test's benefit.
 *
 * The engine resolves the serial controller for runtime-PM bracketing from
 * drv->uart_dev; leaving it NULL (as below) skips the PM get/put entirely
 * (see arfw_uart_tty_write), so no PM mocking is required.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>

#include "arfw_uart_dev.c"

/* Endpoints used across the routing cases. */
#define TEST_HLOS_EP 0x30c
#define TEST_FW_EP 0x8d6
#define TEST_OTHER_HLOS_EP 0x401
/* A second fw endpoint sharing TEST_HLOS_EP, for the multi-session demux case. */
#define TEST_FW_EP_B 0x8d7

/* A non-NULL opaque client handle the fakes can compare against. */
#define TEST_CLIENT ((arfw_client_queue_t)0xc11e)

/* ---- Fake tty: captures bytes the engine writes via tty->ops->write ---- */

struct fake_tty {
	struct tty_struct tty;
	struct tty_operations ops;
	u8 buf[ARFW_UART_RX_BUF_SIZE];
	size_t len;
	bool short_write_once; /* return a partial count on the first write */
};

static struct fake_tty *g_fake_tty;

static int fake_tty_write(struct tty_struct *tty, const unsigned char *buf,
			  int count)
{
	struct fake_tty *ft = g_fake_tty;
	int take = count;

	if (ft->short_write_once && count > 1) {
		ft->short_write_once = false;
		take = 1; /* exercise the TX drain retry loop */
	}
	if (ft->len + take > sizeof(ft->buf))
		take = sizeof(ft->buf) - ft->len;
	memcpy(ft->buf + ft->len, buf, take);
	ft->len += take;
	return take;
}

static struct fake_tty *fake_tty_create(struct kunit *test)
{
	struct fake_tty *ft = kunit_kzalloc(test, sizeof(*ft), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ft);
	ft->ops.write = fake_tty_write;
	ft->tty.ops = &ft->ops;
	g_fake_tty = ft;
	return ft;
}

/* ---- Fake arfw_client_ops: captures produce/reserve calls ---- */

struct fake_client {
	struct arfw_client_ops ops;
	/* last produced io_request (copied out of the engine) */
	bool produced;
	ar_firmware_message_header_t produced_hdr;
	u8 produced_payload[ARFW_UART_MAX_FRAME_SIZE];
	size_t produced_payload_len;
	int produce_ret; /* value handle_client_queue_produce_request returns */
	/* shutdown bookkeeping for detach-style coverage (unused here) */
	int reserve_calls;
};

static struct fake_client *g_fake_client;

/*
 * req is a fixed-ABI client callback slot
 * (arfw_client_ops.handle_client_queue_produce_request); the engine writes
 * through it, and const-qualifying it would change the function-pointer type.
 */
// cppcheck-suppress constParameterCallback
static int fake_produce(arfw_client_queue_t queue, struct arfw_io_request *req)
{
	struct fake_client *fc = g_fake_client;
	const ar_firmware_message_header_t *hdr = req->data;

	if (fc->produce_ret)
		return fc->produce_ret;

	fc->produced = true;
	fc->produced_hdr = *hdr;
	fc->produced_payload_len = req->data_size - sizeof(*hdr);
	if (fc->produced_payload_len > sizeof(fc->produced_payload))
		fc->produced_payload_len = sizeof(fc->produced_payload);
	memcpy(fc->produced_payload, (const u8 *)req->data + sizeof(*hdr),
	       fc->produced_payload_len);
	return 0;
}

static int fake_reserve(arfw_client_queue_t queue, struct arfw_io_request *req)
{
	g_fake_client->reserve_calls++;
	return -ENOENT; /* no slots to replenish in these tests */
}

static struct fake_client *fake_client_create(struct kunit *test)
{
	struct fake_client *fc = kunit_kzalloc(test, sizeof(*fc), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fc);
	fc->ops.handle_client_queue_produce_request = fake_produce;
	fc->ops.handle_client_queue_reserve_request = fake_reserve;
	g_fake_client = fc;
	return fc;
}

/* ---- Engine fixture: a real struct arfw_uart_driver wired to the fakes ---- */

struct engine_fixture {
	struct arfw_uart_driver *drv;
	struct fake_tty *ft;
	struct fake_client *fc;
};

static struct engine_fixture *engine_create(struct kunit *test)
{
	struct engine_fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	struct arfw_uart_driver *drv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, f);
	drv = kunit_kzalloc(test, sizeof(*drv), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drv);

	f->ft = fake_tty_create(test);
	f->fc = fake_client_create(test);

	mutex_init(&drv->tx_lock);
	mutex_init(&drv->queues_lock);
	spin_lock_init(&drv->rx_lock);
	INIT_WORK(&drv->rx_work, arfw_uart_rx_work_fn);
	drv->tty = &f->ft->tty;
	drv->uart_dev = NULL; /* skip runtime-PM bracketing */
	drv->client_ops = &f->fc->ops;
	f->drv = drv;
	return f;
}

/*
 * Create a queue through the real handle_queue_create op and back its data
 * region with a real buffer (deliver_frame writes into data->ptr). Returns the
 * engine's queue_context, as the framework would hand it to later callbacks.
 */
static struct arfw_uart_queue *
engine_make_queue(struct kunit *test, struct arfw_uart_driver *drv,
		  enum ar_queue_direction dir, ar_endpoint_id_t hlos_ep,
		  ar_endpoint_id_t fw_ep, u32 element_size, u16 depth)
{
	struct arfw_client_queue_create_params params = {
		.queue_direction = dir,
		.element_size = element_size,
		.depth = depth,
		.hlos_endpoint_id = hlos_ep,
		.fw_endpoint_id = fw_ep,
	};
	struct arfw_queue_mem_region *region;
	void *qctx = NULL;
	int ret;

	region = kunit_kzalloc(test, sizeof(*region), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, region);
	region->size = (size_t)element_size * depth;
	region->ptr = kunit_kzalloc(test, region->size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, region->ptr);

	ret = arfw_uart_handle_queue_create(TEST_CLIENT, drv, &params, region,
					    &qctx);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, qctx);

	/* FW->HLOS queues must be marked ready before deliver_frame routes. */
	if (dir == AR_QUEUE_FW_TO_HLOS)
		arfw_uart_handle_notify_queue_ready(qctx);
	return qctx;
}

/* Build a [babel_header][payload] buffer as deliver_frame expects post-decode. */
static size_t build_babel_msg(u8 *out, ar_endpoint_id_t dst,
			      ar_endpoint_id_t src, u8 tracknum, u8 seqnum,
			      u16 msg_id, const u8 *payload, size_t payload_len)
{
	struct arfw_uart_babel_header bh = {
		.dst = dst,
		.src = src,
		.tracknum = tracknum,
		.seqnum = seqnum,
		.msg_id = msg_id,
		.payload_bytes = payload_len,
		.flags = 0,
	};

	memcpy(out, &bh, sizeof(bh));
	if (payload_len)
		memcpy(out + sizeof(bh), payload, payload_len);
	return sizeof(bh) + payload_len;
}

/* ---- TX: babel-header translation round-trip ---- */

/*
 * Send an arfw message through handle_send_queue, decode the captured wire
 * frame, and assert the babel header on the wire mirrors the arfw header
 * field-for-field (this is the TX half of the translation).
 */
static void test_tx_header_translation(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	struct arfw_uart_queue *q =
		engine_make_queue(test, f->drv, AR_QUEUE_HLOS_TO_FW,
				  TEST_HLOS_EP, TEST_FW_EP, 512, 4);
	const u8 payload[5] = { 'h', 'e', 'l', 'l', 'o' };
	u8 reqbuf[sizeof(ar_firmware_message_header_t) + sizeof(payload)];
	ar_firmware_message_header_t ah = {
		.msg_id = 0x1234,
		.tracking_id = 0x5a,
		.sequence_id = 0x77,
		.data_size = sizeof(payload),
		.inline_msg_len = sizeof(payload),
		.data_location = ARFW_BUFFER_LOC_IN_LINE,
	};
	struct arfw_io_request req;
	struct arfw_uart_babel_header bh;
	/* Heap-allocated: the kernel caps on-stack frame size (-Wframe-larger-than). */
	u8 *decoded = kunit_kzalloc(test, ARFW_UART_MAX_FRAME_SIZE, GFP_KERNEL);
	size_t decoded_len = 0;
	bool consumed = false;
	bool keep;
	int ret;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, decoded);
	memcpy(reqbuf, &ah, sizeof(ah));
	memcpy(reqbuf + sizeof(ah), payload, sizeof(payload));
	req.data = reqbuf;
	req.data_size = sizeof(reqbuf);
	req.index = 0;

	/*
	 * Force the first tty write to take only one byte so the engine's TX
	 * drain retry loop (arfw_uart_tty_write) is exercised -- the whole frame
	 * must still arrive intact across the partial write.
	 */
	f->ft->short_write_once = true;

	keep = arfw_uart_handle_send_queue(q, &req, &consumed);
	KUNIT_EXPECT_TRUE(test, keep);
	KUNIT_EXPECT_TRUE(test, consumed);
	KUNIT_ASSERT_GT(test, f->ft->len, (size_t)0);

	/* The captured wire bytes end in the frame delimiter; decode the body. */
	KUNIT_ASSERT_EQ(test, f->ft->buf[f->ft->len - 1],
			(u8)ARFW_UART_FRAME_DELIMITER);
	ret = arfw_uart_frame_decode(f->ft->buf, f->ft->len - 1, decoded,
				     ARFW_UART_MAX_FRAME_SIZE, &decoded_len);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_GE(test, decoded_len, sizeof(bh));

	memcpy(&bh, decoded, sizeof(bh));
	/* dst/src are swapped relative to the arfw queue's endpoints on TX. */
	KUNIT_EXPECT_EQ(test, bh.dst, (u16)TEST_FW_EP);
	KUNIT_EXPECT_EQ(test, bh.src, (u16)TEST_HLOS_EP);
	KUNIT_EXPECT_EQ(test, bh.tracknum, ah.tracking_id);
	/* seqnum is u8 on the wire; ah.sequence_id is u16 -- compare same width. */
	KUNIT_EXPECT_EQ(test, bh.seqnum, (u8)ah.sequence_id);
	KUNIT_EXPECT_EQ(test, bh.msg_id, ah.msg_id);
	KUNIT_EXPECT_EQ(test, bh.payload_bytes, (u32)sizeof(payload));
	KUNIT_EXPECT_EQ(test, decoded_len - sizeof(bh), sizeof(payload));
	KUNIT_EXPECT_EQ(test,
			memcmp(decoded + sizeof(bh), payload, sizeof(payload)),
			0);
}

/* ---- TX: oversized payload_len is rejected (the Task-1 OOB-read bug) ---- */

/*
 * ah->data_size is uint16_t and the only later bound is
 * ARFW_UART_MAX_FRAME_SIZE, NOT the request buffer. A reply that advertises
 * more payload than req->data actually holds must be rejected BEFORE the memcpy
 * reads past the buffer. Pre-fix this test reads out of bounds (and produces a
 * frame); post-fix the engine drops it and writes nothing.
 */
static void test_tx_rejects_payload_exceeding_buffer(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	struct arfw_uart_queue *q =
		engine_make_queue(test, f->drv, AR_QUEUE_HLOS_TO_FW,
				  TEST_HLOS_EP, TEST_FW_EP, 512, 4);
	const u8 payload[4] = { 1, 2, 3, 4 };
	u8 reqbuf[sizeof(ar_firmware_message_header_t) + sizeof(payload)];
	ar_firmware_message_header_t ah = {
		.msg_id = 1,
		/* Lie: claim 200 bytes of payload in a buffer that holds 4. */
		.data_size = 200,
		.inline_msg_len = 200,
		.data_location = ARFW_BUFFER_LOC_IN_LINE,
	};
	struct arfw_io_request req;
	bool consumed = false;
	bool keep;

	memcpy(reqbuf, &ah, sizeof(ah));
	memcpy(reqbuf + sizeof(ah), payload, sizeof(payload));
	req.data = reqbuf;
	req.data_size = sizeof(reqbuf); /* actual buffer: header + 4 bytes */
	req.index = 0;

	keep = arfw_uart_handle_send_queue(q, &req, &consumed);
	KUNIT_EXPECT_TRUE(test, keep);
	/* Rejected before framing: nothing was written to the wire. */
	KUNIT_EXPECT_EQ(test, f->ft->len, (size_t)0);
}

/* A payload that overflows ARFW_UART_MAX_FRAME_SIZE is also dropped. */
static void test_tx_rejects_over_max_frame(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	struct arfw_uart_queue *q =
		engine_make_queue(test, f->drv, AR_QUEUE_HLOS_TO_FW,
				  TEST_HLOS_EP, TEST_FW_EP, 8192, 4);
	size_t huge = ARFW_UART_MAX_FRAME_SIZE; /* + babel header > MAX */
	u8 *reqbuf = kunit_kzalloc(
		test, sizeof(ar_firmware_message_header_t) + huge, GFP_KERNEL);
	ar_firmware_message_header_t ah = {
		.data_size = huge,
		.inline_msg_len = huge,
		.data_location = ARFW_BUFFER_LOC_IN_LINE,
	};
	struct arfw_io_request req;
	bool consumed = false;
	bool keep;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, reqbuf);
	memcpy(reqbuf, &ah, sizeof(ah));
	req.data = reqbuf;
	req.data_size = sizeof(ar_firmware_message_header_t) + huge;
	req.index = 0;

	keep = arfw_uart_handle_send_queue(q, &req, &consumed);
	KUNIT_EXPECT_TRUE(test, keep);
	KUNIT_EXPECT_EQ(test, f->ft->len, (size_t)0);
}

/* ---- RX: deliver_frame routes by babel dst and translates the header ---- */

static void test_rx_routes_and_translates(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	const u8 payload[6] = { 9, 8, 7, 6, 5, 4 };
	u8 *msg = kunit_kzalloc(test, ARFW_UART_MAX_FRAME_SIZE, GFP_KERNEL);
	size_t msg_len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	(void)engine_make_queue(test, f->drv, AR_QUEUE_FW_TO_HLOS, TEST_HLOS_EP,
				TEST_FW_EP, 512, 4);
	msg_len = build_babel_msg(msg, /*dst=*/TEST_HLOS_EP,
				  /*src=*/TEST_FW_EP, /*tracknum=*/0x11,
				  /*seqnum=*/0x22, /*msg_id=*/0x3344, payload,
				  sizeof(payload));

	arfw_uart_deliver_frame(f->drv, msg, msg_len);

	KUNIT_EXPECT_TRUE(test, f->fc->produced);
	KUNIT_EXPECT_EQ(test, f->fc->produced_hdr.msg_id, (u16)0x3344);
	KUNIT_EXPECT_EQ(test, f->fc->produced_hdr.tracking_id, (u8)0x11);
	KUNIT_EXPECT_EQ(test, f->fc->produced_hdr.sequence_id, (u16)0x22);
	KUNIT_EXPECT_EQ(test, f->fc->produced_hdr.data_size,
			(u16)sizeof(payload));
	KUNIT_EXPECT_EQ(test, f->fc->produced_hdr.inline_msg_len,
			(u16)sizeof(payload));
	KUNIT_EXPECT_EQ(test, f->fc->produced_hdr.data_location,
			(u8)ARFW_BUFFER_LOC_IN_LINE);
	KUNIT_EXPECT_EQ(test, f->fc->produced_payload_len, sizeof(payload));
	KUNIT_EXPECT_EQ(
		test, memcmp(f->fc->produced_payload, payload, sizeof(payload)),
		0);
}

/*
 * Two FW->HLOS sessions sharing one host endpoint (differing only by the fw
 * endpoint) must be demuxed by the babel src: a reply from fw endpoint B must
 * land in B's queue, not the first-registered queue that shares the dst host
 * endpoint.
 */
static void test_rx_routes_by_src_among_shared_hlos(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	const u8 payload[4] = { 0xa, 0xb, 0xc, 0xd };
	u8 *msg = kunit_kzalloc(test, ARFW_UART_MAX_FRAME_SIZE, GFP_KERNEL);
	struct arfw_uart_queue *qa, *qb;
	size_t msg_len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

	/* qa is created first; both advertise the same HLOS endpoint. */
	qa = engine_make_queue(test, f->drv, AR_QUEUE_FW_TO_HLOS, TEST_HLOS_EP,
			       TEST_FW_EP, 512, 4);
	qb = engine_make_queue(test, f->drv, AR_QUEUE_FW_TO_HLOS, TEST_HLOS_EP,
			       TEST_FW_EP_B, 512, 4);

	msg_len = build_babel_msg(msg, /*dst=*/TEST_HLOS_EP,
				  /*src=*/TEST_FW_EP_B, /*tracknum=*/0x11,
				  /*seqnum=*/0x22, /*msg_id=*/0x3344, payload,
				  sizeof(payload));

	arfw_uart_deliver_frame(f->drv, msg, msg_len);

	/* It must land in qb (matched by src), leaving qa untouched. */
	KUNIT_EXPECT_EQ(test, qb->wr_idx, (u32)1);
	KUNIT_EXPECT_EQ(test, qb->reserved, (u32)3);
	KUNIT_EXPECT_EQ(test, qa->wr_idx, (u32)0);
	KUNIT_EXPECT_EQ(test, qa->reserved, (u32)4);
}

/* A frame shorter than the babel header is dropped, not produced. */
static void test_rx_drops_short_frame(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	u8 msg[4] = { 0 };

	(void)engine_make_queue(test, f->drv, AR_QUEUE_FW_TO_HLOS, TEST_HLOS_EP,
				TEST_FW_EP, 512, 4);
	arfw_uart_deliver_frame(f->drv, msg, sizeof(msg));
	KUNIT_EXPECT_FALSE(test, f->fc->produced);
}

/* A decoded frame larger than the destination slot is dropped (RX twin). */
static void test_rx_drops_over_element_size(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	/* element_size only fits header + a few bytes. */
	u32 element_size = sizeof(ar_firmware_message_header_t) + 4;
	const u8 payload[64] = { 0 };
	u8 *msg = kunit_kzalloc(test, ARFW_UART_MAX_FRAME_SIZE, GFP_KERNEL);
	size_t msg_len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	(void)engine_make_queue(test, f->drv, AR_QUEUE_FW_TO_HLOS, TEST_HLOS_EP,
				TEST_FW_EP, element_size, 4);
	msg_len = build_babel_msg(msg, TEST_HLOS_EP, TEST_FW_EP, 0, 0, 0,
				  payload, sizeof(payload));
	arfw_uart_deliver_frame(f->drv, msg, msg_len);
	KUNIT_EXPECT_FALSE(test, f->fc->produced);
}

/* A frame whose dst matches no FW->HLOS queue is dropped. */
static void test_rx_drops_no_matching_queue(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	const u8 payload[4] = { 1, 2, 3, 4 };
	u8 *msg = kunit_kzalloc(test, ARFW_UART_MAX_FRAME_SIZE, GFP_KERNEL);
	size_t msg_len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	(void)engine_make_queue(test, f->drv, AR_QUEUE_FW_TO_HLOS, TEST_HLOS_EP,
				TEST_FW_EP, 512, 4);
	/* Address a different HLOS endpoint than any queue advertises. */
	msg_len = build_babel_msg(msg, TEST_OTHER_HLOS_EP, TEST_FW_EP, 0, 0, 0,
				  payload, sizeof(payload));
	arfw_uart_deliver_frame(f->drv, msg, msg_len);
	KUNIT_EXPECT_FALSE(test, f->fc->produced);
}

/*
 * When the RX ring has no reserved slot (consumer behind), the frame is dropped
 * rather than produced into an unreserved slot (which BUGs the ring). Drive
 * reserved to 0 by consuming the whole depth, then deliver one more.
 */
static void test_rx_drops_no_reserved_slot(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	const u8 payload[4] = { 1, 2, 3, 4 };
	u8 *msg = kunit_kzalloc(test, ARFW_UART_MAX_FRAME_SIZE, GFP_KERNEL);
	size_t msg_len;
	int i;
	const int depth = 2;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	(void)engine_make_queue(test, f->drv, AR_QUEUE_FW_TO_HLOS, TEST_HLOS_EP,
				TEST_FW_EP, 512, depth);
	msg_len = build_babel_msg(msg, TEST_HLOS_EP, TEST_FW_EP, 0, 0, 0,
				  payload, sizeof(payload));

	/* Consume the whole pre-reserved ring (reserved == depth at create). */
	for (i = 0; i < depth; i++) {
		f->fc->produced = false;
		arfw_uart_deliver_frame(f->drv, msg, msg_len);
		KUNIT_EXPECT_TRUE(test, f->fc->produced);
	}

	/* reserved is now 0; the next frame must be dropped. */
	f->fc->produced = false;
	arfw_uart_deliver_frame(f->drv, msg, msg_len);
	KUNIT_EXPECT_FALSE(test, f->fc->produced);
}

/* ---- Queue lifecycle: repeated create/destroy reuses slots ---- */

/*
 * Each create/destroy cycle must reclaim its slot. Looping well past
 * ARFW_UART_MAX_QUEUES would fail with -ENOSPC if slots leaked.
 */
static void test_queue_slot_reuse(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	int cycle;
	const int cycles = ARFW_UART_MAX_QUEUES * 4;

	for (cycle = 0; cycle < cycles; cycle++) {
		struct arfw_uart_queue *q =
			engine_make_queue(test, f->drv, AR_QUEUE_HLOS_TO_FW,
					  TEST_HLOS_EP, TEST_FW_EP, 256, 4);
		int ret = arfw_uart_handle_queue_destroy(f->drv, q);

		KUNIT_ASSERT_EQ_MSG(test, ret, 0, "destroy failed on cycle %d",
				    cycle);
	}

	/* After all the churn, a fresh create still succeeds (no slot leak). */
	(void)engine_make_queue(test, f->drv, AR_QUEUE_HLOS_TO_FW, TEST_HLOS_EP,
				TEST_FW_EP, 256, 4);
}

/*
 * Filling every slot then creating one more must fail with -ENOSPC -- this is
 * the bound the reuse test relies on (a leak would surface here far earlier).
 */
static void test_queue_exhaustion_returns_enospc(struct kunit *test)
{
	struct engine_fixture *f = engine_create(test);
	struct arfw_client_queue_create_params params = {
		.queue_direction = AR_QUEUE_HLOS_TO_FW,
		.element_size = 128,
		.depth = 2,
		.hlos_endpoint_id = TEST_HLOS_EP,
		.fw_endpoint_id = TEST_FW_EP,
	};
	struct arfw_queue_mem_region region = { 0 };
	void *qctx;
	int i, ret;

	for (i = 0; i < ARFW_UART_MAX_QUEUES; i++) {
		qctx = NULL;
		ret = arfw_uart_handle_queue_create(TEST_CLIENT, f->drv,
						    &params, &region, &qctx);
		KUNIT_ASSERT_EQ(test, ret, 0);
	}

	qctx = NULL;
	ret = arfw_uart_handle_queue_create(TEST_CLIENT, f->drv, &params,
					    &region, &qctx);
	KUNIT_EXPECT_EQ(test, ret, -ENOSPC);
}

static struct kunit_case arfw_uart_dev_test_cases[] = {
	KUNIT_CASE(test_tx_header_translation),
	KUNIT_CASE(test_tx_rejects_payload_exceeding_buffer),
	KUNIT_CASE(test_tx_rejects_over_max_frame),
	KUNIT_CASE(test_rx_routes_and_translates),
	KUNIT_CASE(test_rx_routes_by_src_among_shared_hlos),
	KUNIT_CASE(test_rx_drops_short_frame),
	KUNIT_CASE(test_rx_drops_over_element_size),
	KUNIT_CASE(test_rx_drops_no_matching_queue),
	KUNIT_CASE(test_rx_drops_no_reserved_slot),
	KUNIT_CASE(test_queue_slot_reuse),
	KUNIT_CASE(test_queue_exhaustion_returns_enospc),
	{},
};

/*
 * Non-static, and NOT registered here via kunit_test_suite(): this suite is
 * linked into the arfw_acro_uart_test module together with the framing suite.
 * kunit_test_suite() expands to module_init/module_exit, which may appear only
 * once per module, so arfw_uart_framing_test.c registers BOTH suites with a
 * single kunit_test_suites(&framing, &dev). MODULE_LICENSE/MODULE_DESCRIPTION
 * are likewise carried only by that file.
 */
struct kunit_suite arfw_uart_dev_test_suite = {
	.name = "arfw-uart-dev",
	.test_cases = arfw_uart_dev_test_cases,
};
