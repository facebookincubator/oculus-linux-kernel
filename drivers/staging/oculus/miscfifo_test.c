// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit tests for the miscfifo chardev driver.
 *
 * These tests exercise the kernel-side API of miscfifo (write_buf,
 * send_buf, open, release, clear, filter, xchg_context) without
 * going through VFS or touching userspace buffers.
 */

#include <kunit/test.h>
#include <linux/dcache.h>
#include <linux/device.h>
#include <linux/miscfifo.h>
#include <linux/slab.h>

#define TEST_KFIFO_SIZE 4096UL
#define REC_MAX_LENGTH  0xffff

struct test_ctx {
	struct miscfifo mf;
	struct device dev;
};

static void init_miscfifo(struct test_ctx *ctx, miscfifo_filter_fn filter)
{
	memset(ctx, 0, sizeof(*ctx));

	device_initialize(&ctx->dev);
	ctx->dev.init_name = "test_miscfifo_dev";

	ctx->mf.config.kfifo_size = TEST_KFIFO_SIZE;
	ctx->mf.config.filter_fn = filter;
	ctx->mf.dev = &ctx->dev;
	init_rwsem(&ctx->mf.clients.rw_lock);
	INIT_LIST_HEAD(&ctx->mf.clients.list);
	init_waitqueue_head(&ctx->mf.clients.wait);
}

static struct file *fake_file_alloc(struct kunit *test, struct test_ctx *ctx)
{
	struct file *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	struct dentry *d = kunit_kzalloc(test, sizeof(*d), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, f);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, d);
	d->d_name.name = "test_miscfifo";
	d->d_name.len = strlen(d->d_name.name);
	f->f_path.dentry = d;
	return f;
}

/*
 * Open a client on the miscfifo, returning the file whose
 * private_data points to the new miscfifo_client.
 */
static struct file *open_client(struct kunit *test, struct test_ctx *ctx)
{
	struct file *f = fake_file_alloc(test, ctx);
	int rc;

	rc = miscfifo_fop_open(f, &ctx->mf);
	KUNIT_ASSERT_EQ(test, rc, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, f->private_data);
	return f;
}

static void release_client(struct file *f)
{
	miscfifo_fop_release(NULL, f);
}

/*
 * Peek at the next record in a client's kfifo using the kernel-only
 * kfifo_out (no copy_to_user).  Returns the number of bytes copied
 * into @buf, or 0 if the fifo is empty.
 */
static unsigned int client_kfifo_out(struct miscfifo_client *client,
				     u8 *buf, size_t buf_len)
{
	return kfifo_out(&client->fifo, buf, buf_len);
}

static void test_open_release(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;

	init_miscfifo(&ctx, NULL);

	/* open: client should appear in the list */
	f = open_client(test, &ctx);
	client = f->private_data;
	KUNIT_EXPECT_FALSE(test, list_empty(&ctx.mf.clients.list));
	KUNIT_EXPECT_PTR_EQ(test, client->mf, &ctx.mf);

	/* release: list should be empty again */
	release_client(f);
	KUNIT_EXPECT_TRUE(test, list_empty(&ctx.mf.clients.list));
}

static void test_write_single_client(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;
	bool should_wake;
	int rc;
	u8 payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
	u8 readback[8] = { 0 };
	unsigned int copied;

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);
	client = f->private_data;

	rc = miscfifo_write_buf(&ctx.mf, payload, sizeof(payload), &should_wake);
	KUNIT_EXPECT_EQ(test, rc, 0);
	KUNIT_EXPECT_TRUE(test, should_wake);

	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, (unsigned int)sizeof(payload));
	KUNIT_EXPECT_EQ(test, memcmp(readback, payload, sizeof(payload)), 0);

	release_client(f);
}

static void test_write_multiple_clients(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f1, *f2, *f3;
	struct miscfifo_client *c1, *c2, *c3;
	bool should_wake;
	int rc;
	u8 payload[] = { 0x01, 0x02, 0x03 };
	u8 buf[8];
	unsigned int copied;

	init_miscfifo(&ctx, NULL);
	f1 = open_client(test, &ctx);
	f2 = open_client(test, &ctx);
	f3 = open_client(test, &ctx);
	c1 = f1->private_data;
	c2 = f2->private_data;
	c3 = f3->private_data;

	rc = miscfifo_write_buf(&ctx.mf, payload, sizeof(payload), &should_wake);
	KUNIT_EXPECT_EQ(test, rc, 0);
	KUNIT_EXPECT_TRUE(test, should_wake);

	/* Every client should have received the data */
	copied = client_kfifo_out(c1, buf, sizeof(buf));
	KUNIT_EXPECT_EQ(test, copied, (unsigned int)sizeof(payload));

	copied = client_kfifo_out(c2, buf, sizeof(buf));
	KUNIT_EXPECT_EQ(test, copied, (unsigned int)sizeof(payload));

	copied = client_kfifo_out(c3, buf, sizeof(buf));
	KUNIT_EXPECT_EQ(test, copied, (unsigned int)sizeof(payload));

	release_client(f3);
	release_client(f2);
	release_client(f1);
}

static void test_write_overflow(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;
	bool should_wake;
	int rc, i;
	/*
	 * The fifo uses a __STRUCT_KFIFO with recsize=2, so each record
	 * costs len + 2 bytes.  TEST_KFIFO_SIZE is 4096; with a 512-byte
	 * payload that is 514 bytes per record, so floor(4096/514) = 7
	 * records fit.  Write 10 to guarantee tail-drop overflow.
	 *
	 * miscfifo_write_buf does tail-drop: when the fifo is full it
	 * skips the new record rather than evicting old ones.  So the
	 * first 7 records (tags 0-6) must survive and 7-9 are dropped.
	 */
	const int num_writes = 10;
	const int expect_fit = 7;
	u8 payload[512];
	u8 readback[512];
	unsigned int copied;
	int overflow_count = 0;

	memset(payload, 0xAA, sizeof(payload));

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);
	client = f->private_data;

	/* Write 10 records, tagging each with its index */
	for (i = 0; i < num_writes; i++) {
		payload[0] = i;
		rc = miscfifo_write_buf(&ctx.mf, payload, sizeof(payload),
					&should_wake);
		if (i < expect_fit)
			KUNIT_EXPECT_EQ(test, rc, 0);
		else
			KUNIT_EXPECT_GT(test, rc, 0);
		if (rc > 0)
			overflow_count++;
	}

	/* Exactly the tail records should have overflowed */
	KUNIT_EXPECT_EQ(test, overflow_count, num_writes - expect_fit);

	/* Read back and verify the first 7 records survived in order */
	for (i = 0; i < expect_fit; i++) {
		copied = client_kfifo_out(client, readback, sizeof(readback));
		KUNIT_ASSERT_EQ(test, copied, (unsigned int)sizeof(payload));
		KUNIT_EXPECT_EQ(test, readback[0], (u8)i);
	}

	/* FIFO should be empty — the dropped records are gone */
	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, 0U);

	release_client(f);
}

static void test_write_empty_or_oversized(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	bool should_wake;
	int rc;
	u8 byte = 0x42;

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);

	/* Zero length */
	rc = miscfifo_write_buf(&ctx.mf, &byte, 0, &should_wake);
	KUNIT_EXPECT_EQ(test, rc, -EINVAL);

	/* Oversized (> REC_MAX_LENGTH, which is 0xffff) */
	rc = miscfifo_write_buf(&ctx.mf, &byte, REC_MAX_LENGTH + 1,
				&should_wake);
	KUNIT_EXPECT_EQ(test, rc, -EINVAL);

	release_client(f);
}

static void test_client_clear(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;
	bool should_wake;
	u8 payload[] = { 0x01, 0x02 };
	u8 readback[8];
	unsigned int copied;

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);
	client = f->private_data;

	/* Write some data and set cancel */
	miscfifo_write_buf(&ctx.mf, payload, sizeof(payload), &should_wake);
	miscfifo_cancel(f);

	KUNIT_EXPECT_FALSE(test, kfifo_is_empty(&client->fifo));
	KUNIT_EXPECT_EQ(test, atomic_read(&client->cancel), 1);

	/* Clear should reset both */
	miscfifo_client_clear(client);

	KUNIT_EXPECT_TRUE(test, kfifo_is_empty(&client->fifo));
	KUNIT_EXPECT_EQ(test, atomic_read(&client->cancel), 0);

	/* Verify no data readable */
	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, 0U);

	release_client(f);
}

static void test_clear_all(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f1, *f2;
	struct miscfifo_client *c1, *c2;
	bool should_wake;
	u8 payload[] = { 0xAA };

	init_miscfifo(&ctx, NULL);
	f1 = open_client(test, &ctx);
	f2 = open_client(test, &ctx);
	c1 = f1->private_data;
	c2 = f2->private_data;

	miscfifo_write_buf(&ctx.mf, payload, sizeof(payload), &should_wake);
	KUNIT_EXPECT_FALSE(test, kfifo_is_empty(&c1->fifo));
	KUNIT_EXPECT_FALSE(test, kfifo_is_empty(&c2->fifo));

	miscfifo_clear(&ctx.mf);

	KUNIT_EXPECT_TRUE(test, kfifo_is_empty(&c1->fifo));
	KUNIT_EXPECT_TRUE(test, kfifo_is_empty(&c2->fifo));

	release_client(f2);
	release_client(f1);
}

static void test_xchg_context(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	int dummy_a = 1, dummy_b = 2;
	void *old;

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);

	/* Initially NULL */
	old = miscfifo_fop_xchg_context(f, &dummy_a);
	KUNIT_EXPECT_PTR_EQ(test, old, NULL);

	/* Now should return &dummy_a */
	old = miscfifo_fop_xchg_context(f, &dummy_b);
	KUNIT_EXPECT_PTR_EQ(test, old, (void *)&dummy_a);

	/* And now &dummy_b */
	old = miscfifo_fop_xchg_context(f, NULL);
	KUNIT_EXPECT_PTR_EQ(test, old, (void *)&dummy_b);

	release_client(f);
}

/* Accept only payloads whose first byte is 0x01 */
static bool accept_tag_01(const void *context, const u8 *header,
			  size_t header_len, const u8 *payload,
			  size_t payload_len)
{
	return payload_len > 0 && payload[0] == 0x01;
}

static void test_filter_fn(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;
	bool should_wake;
	int rc;
	u8 accepted[] = { 0x01, 0xFF };
	u8 rejected[] = { 0x02, 0xFF };
	u8 readback[8];
	unsigned int copied;

	init_miscfifo(&ctx, accept_tag_01);
	f = open_client(test, &ctx);
	client = f->private_data;

	/* Rejected packet: should_wake must stay false, fifo stays empty */
	rc = miscfifo_write_buf(&ctx.mf, rejected, sizeof(rejected),
				&should_wake);
	KUNIT_EXPECT_EQ(test, rc, 0);
	KUNIT_EXPECT_FALSE(test, should_wake);
	KUNIT_EXPECT_TRUE(test, kfifo_is_empty(&client->fifo));

	/* Accepted packet: arrives in the fifo */
	rc = miscfifo_write_buf(&ctx.mf, accepted, sizeof(accepted),
				&should_wake);
	KUNIT_EXPECT_EQ(test, rc, 0);
	KUNIT_EXPECT_TRUE(test, should_wake);

	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, (unsigned int)sizeof(accepted));
	KUNIT_EXPECT_EQ(test, memcmp(readback, accepted, sizeof(accepted)), 0);

	release_client(f);
}

static void test_fifo_ordering(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;
	bool should_wake;
	int i;
	const int num_records = 8;
	u8 payload[4];
	u8 readback[4];
	unsigned int copied;

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);
	client = f->private_data;

	/* Write records tagged 0..7 */
	for (i = 0; i < num_records; i++) {
		payload[0] = i;
		payload[1] = i ^ 0xAA;
		payload[2] = i + 1;
		payload[3] = i * 3;
		miscfifo_write_buf(&ctx.mf, payload, sizeof(payload),
				   &should_wake);
		KUNIT_EXPECT_TRUE(test, should_wake);
	}

	/* Read them back and verify FIFO order */
	for (i = 0; i < num_records; i++) {
		copied = client_kfifo_out(client, readback, sizeof(readback));
		KUNIT_ASSERT_EQ(test, copied, (unsigned int)sizeof(payload));
		KUNIT_EXPECT_EQ(test, readback[0], (u8)i);
		KUNIT_EXPECT_EQ(test, readback[1], (u8)(i ^ 0xAA));
		KUNIT_EXPECT_EQ(test, readback[2], (u8)(i + 1));
		KUNIT_EXPECT_EQ(test, readback[3], (u8)(i * 3));
	}

	/* FIFO should be empty now */
	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, 0U);

	release_client(f);
}

static void test_cancel(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);
	client = f->private_data;

	KUNIT_EXPECT_EQ(test, atomic_read(&client->cancel), 0);

	miscfifo_cancel(f);
	KUNIT_EXPECT_EQ(test, atomic_read(&client->cancel), 1);

	/* cancel again is idempotent */
	miscfifo_cancel(f);
	KUNIT_EXPECT_EQ(test, atomic_read(&client->cancel), 1);

	/* clear resets it */
	miscfifo_client_clear(client);
	KUNIT_EXPECT_EQ(test, atomic_read(&client->cancel), 0);

	release_client(f);
}

static void test_interleaved_write_read(struct kunit *test)
{
	struct test_ctx ctx;
	struct file *f;
	struct miscfifo_client *client;
	bool should_wake;
	int rc;
	u8 payload_a[] = { 0xAA, 0xBB };
	u8 payload_b[] = { 0xCC, 0xDD, 0xEE };
	u8 readback[8];
	unsigned int copied;

	init_miscfifo(&ctx, NULL);
	f = open_client(test, &ctx);
	client = f->private_data;

	/* Write A, then read it back before writing B */
	rc = miscfifo_write_buf(&ctx.mf, payload_a, sizeof(payload_a),
				&should_wake);
	KUNIT_EXPECT_EQ(test, rc, 0);
	KUNIT_EXPECT_TRUE(test, should_wake);

	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, (unsigned int)sizeof(payload_a));
	KUNIT_EXPECT_EQ(test, memcmp(readback, payload_a, sizeof(payload_a)), 0);

	/* FIFO should be empty after draining A */
	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, 0U);

	/* Write B, then read it back */
	rc = miscfifo_write_buf(&ctx.mf, payload_b, sizeof(payload_b),
				&should_wake);
	KUNIT_EXPECT_EQ(test, rc, 0);
	KUNIT_EXPECT_TRUE(test, should_wake);

	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, (unsigned int)sizeof(payload_b));
	KUNIT_EXPECT_EQ(test, memcmp(readback, payload_b, sizeof(payload_b)), 0);

	/* FIFO should be empty again */
	copied = client_kfifo_out(client, readback, sizeof(readback));
	KUNIT_EXPECT_EQ(test, copied, 0U);

	release_client(f);
}

static struct kunit_case miscfifo_test_cases[] = {
	KUNIT_CASE(test_open_release),
	KUNIT_CASE(test_write_single_client),
	KUNIT_CASE(test_write_multiple_clients),
	KUNIT_CASE(test_write_overflow),
	KUNIT_CASE(test_write_empty_or_oversized),
	KUNIT_CASE(test_client_clear),
	KUNIT_CASE(test_clear_all),
	KUNIT_CASE(test_xchg_context),
	KUNIT_CASE(test_filter_fn),
	KUNIT_CASE(test_fifo_ordering),
	KUNIT_CASE(test_cancel),
	KUNIT_CASE(test_interleaved_write_read),
	{}
};

static struct kunit_suite miscfifo_test_suite = {
	.name = "miscfifo",
	.test_cases = miscfifo_test_cases,
};

kunit_test_suite(miscfifo_test_suite);

MODULE_AUTHOR("Meta");
MODULE_DESCRIPTION("KUnit tests for the miscfifo chardev driver");
MODULE_LICENSE("GPL");
