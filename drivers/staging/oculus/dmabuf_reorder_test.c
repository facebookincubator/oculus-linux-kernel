// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit tests for dmabuf_reorder scatterlist helpers.
 *
 * Tests count_sg(), copy_sg(), and regions_overlap() — the pure
 * kernel-side logic that reorders scatterlist page mappings.
 * These run in QEMU/UML without QCOM_MEM_BUF.
 *
 * Including dmabuf_reorder.c directly gives the test access to the
 * static helpers.  The QCOM_MEM_BUF-dependent code is compiled out
 * via IS_ENABLED() in that file.
 */

#include <kunit/test.h>
#include <linux/slab.h>

/* Suppress module_{init,exit} and MODULE_* — kunit_test_suite() provides them. */
#define DMABUF_REORDER_KUNIT_INCLUDE
#include "dmabuf_reorder.c"

/* ---- test infrastructure ------------------------------------------ */

#define NUM_TEST_PAGES 4

struct dmabuf_reorder_test_ctx {
	struct page *pages[NUM_TEST_PAGES];
	struct sg_table sgt;
};

static int dmabuf_reorder_test_init(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx;
	struct scatterlist *sg;
	int i;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (i = 0; i < NUM_TEST_PAGES; i++) {
		ctx->pages[i] = alloc_page(GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx->pages[i]);
	}

	KUNIT_ASSERT_EQ(test,
			sg_alloc_table(&ctx->sgt, NUM_TEST_PAGES, GFP_KERNEL),
			0);

	sg = ctx->sgt.sgl;
	for (i = 0; i < NUM_TEST_PAGES; i++) {
		sg_set_page(sg, ctx->pages[i], PAGE_SIZE, 0);
		sg = sg_next(sg);
	}

	test->priv = ctx;
	return 0;
}

static void dmabuf_reorder_test_exit(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	int i;

	sg_free_table(&ctx->sgt);
	for (i = 0; i < NUM_TEST_PAGES; i++)
		if (ctx->pages[i])
			__free_page(ctx->pages[i]);
}

/* ---- regions_overlap tests ---------------------------------------- */

static void test_regions_no_overlap_adjacent(struct kunit *test)
{
	struct dmabuf_reorder_region regions[] = {
		{ .offset = 0,         .length = PAGE_SIZE },
		{ .offset = PAGE_SIZE, .length = PAGE_SIZE },
	};

	KUNIT_EXPECT_FALSE(test, regions_overlap(regions, 2));
}

static void test_regions_overlap_partial(struct kunit *test)
{
	struct dmabuf_reorder_region regions[] = {
		{ .offset = 0,         .length = 2 * PAGE_SIZE },
		{ .offset = PAGE_SIZE, .length = 2 * PAGE_SIZE },
	};

	KUNIT_EXPECT_TRUE(test, regions_overlap(regions, 2));
}

static void test_regions_overlap_identical(struct kunit *test)
{
	struct dmabuf_reorder_region regions[] = {
		{ .offset = 0, .length = PAGE_SIZE },
		{ .offset = 0, .length = PAGE_SIZE },
	};

	KUNIT_EXPECT_TRUE(test, regions_overlap(regions, 2));
}

static void test_regions_single(struct kunit *test)
{
	struct dmabuf_reorder_region regions[] = {
		{ .offset = 0, .length = PAGE_SIZE },
	};

	KUNIT_EXPECT_FALSE(test, regions_overlap(regions, 1));
}

static void test_regions_no_overlap_gap(struct kunit *test)
{
	struct dmabuf_reorder_region regions[] = {
		{ .offset = 0,             .length = PAGE_SIZE },
		{ .offset = 2 * PAGE_SIZE, .length = PAGE_SIZE },
	};

	KUNIT_EXPECT_FALSE(test, regions_overlap(regions, 2));
}

static void test_regions_no_overlap_max(struct kunit *test)
{
	struct dmabuf_reorder_region regions[DMABUF_REORDER_MAX_REGIONS];
	u32 i;

	for (i = 0; i < DMABUF_REORDER_MAX_REGIONS; i++) {
		regions[i].offset = (u64)i * PAGE_SIZE;
		regions[i].length = PAGE_SIZE;
	}

	KUNIT_EXPECT_FALSE(test, regions_overlap(regions, DMABUF_REORDER_MAX_REGIONS));
}

static void test_regions_overlap_max_last_two(struct kunit *test)
{
	struct dmabuf_reorder_region regions[DMABUF_REORDER_MAX_REGIONS];
	u32 i;

	for (i = 0; i < DMABUF_REORDER_MAX_REGIONS; i++) {
		regions[i].offset = (u64)i * PAGE_SIZE;
		regions[i].length = PAGE_SIZE;
	}
	regions[DMABUF_REORDER_MAX_REGIONS - 1].offset =
		regions[DMABUF_REORDER_MAX_REGIONS - 2].offset;

	KUNIT_EXPECT_TRUE(test, regions_overlap(regions, DMABUF_REORDER_MAX_REGIONS));
}

/* ---- count_sg tests ----------------------------------------------- */

static void test_count_sg_full_range(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;

	KUNIT_EXPECT_EQ(test,
			count_sg(&ctx->sgt, 0, 4 * PAGE_SIZE), 4);
}

static void test_count_sg_partial_range(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;

	KUNIT_EXPECT_EQ(test,
			count_sg(&ctx->sgt, PAGE_SIZE, 2 * PAGE_SIZE), 2);
}

static void test_count_sg_single_entry_partial(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;

	KUNIT_EXPECT_EQ(test, count_sg(&ctx->sgt, 0, PAGE_SIZE), 1);
}

static void test_count_sg_no_overlap(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;

	KUNIT_EXPECT_EQ(test,
			count_sg(&ctx->sgt, 4 * PAGE_SIZE, PAGE_SIZE), 0);
}

static void test_count_sg_zero_length(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;

	KUNIT_EXPECT_EQ(test, count_sg(&ctx->sgt, 0, 0), 0);
}

static void test_count_sg_unaligned_start(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;

	/* Start at PAGE_SIZE/2: straddles entries 0 and 1. */
	KUNIT_EXPECT_EQ(test,
			count_sg(&ctx->sgt, PAGE_SIZE / 2, PAGE_SIZE), 2);
}

/* ---- copy_sg tests ------------------------------------------------ */

static void test_copy_sg_identity(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	struct sg_table dst;
	struct scatterlist *cur, *last, *sg;
	int i;

	KUNIT_ASSERT_EQ(test, sg_alloc_table(&dst, 4, GFP_KERNEL), 0);

	cur = dst.sgl;
	last = dst.sgl;
	copy_sg(&cur, &last, &ctx->sgt, 0, 4 * PAGE_SIZE);
	sg_mark_end(last);

	sg = dst.sgl;
	for (i = 0; i < 4; i++) {
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
		KUNIT_EXPECT_PTR_EQ(test, sg_page(sg), ctx->pages[i]);
		KUNIT_EXPECT_EQ(test, sg->length, (unsigned int)PAGE_SIZE);
		sg = sg_next(sg);
	}

	sg_free_table(&dst);
}

static void test_copy_sg_swap_halves(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	struct sg_table dst;
	struct scatterlist *cur, *last, *sg;
	struct page *expected[] = {
		ctx->pages[2], ctx->pages[3],
		ctx->pages[0], ctx->pages[1],
	};
	int i;

	KUNIT_ASSERT_EQ(test, sg_alloc_table(&dst, 4, GFP_KERNEL), 0);

	cur = dst.sgl;
	last = dst.sgl;
	copy_sg(&cur, &last, &ctx->sgt, 2 * PAGE_SIZE, 2 * PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 0, 2 * PAGE_SIZE);
	sg_mark_end(last);

	sg = dst.sgl;
	for (i = 0; i < 4; i++) {
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
		KUNIT_EXPECT_PTR_EQ(test, sg_page(sg), expected[i]);
		sg = sg_next(sg);
	}

	sg_free_table(&dst);
}

static void test_copy_sg_reverse_pages(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	struct sg_table dst;
	struct scatterlist *cur, *last, *sg;
	struct page *expected[] = {
		ctx->pages[3], ctx->pages[2],
		ctx->pages[1], ctx->pages[0],
	};
	int i;

	KUNIT_ASSERT_EQ(test, sg_alloc_table(&dst, 4, GFP_KERNEL), 0);

	cur = dst.sgl;
	last = dst.sgl;
	copy_sg(&cur, &last, &ctx->sgt, 3 * PAGE_SIZE, PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 2 * PAGE_SIZE, PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 1 * PAGE_SIZE, PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 0, PAGE_SIZE);
	sg_mark_end(last);

	sg = dst.sgl;
	for (i = 0; i < 4; i++) {
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
		KUNIT_EXPECT_PTR_EQ(test, sg_page(sg), expected[i]);
		sg = sg_next(sg);
	}

	sg_free_table(&dst);
}

static void test_copy_sg_subset(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	struct sg_table dst;
	struct scatterlist *cur, *last, *sg;

	KUNIT_ASSERT_EQ(test, sg_alloc_table(&dst, 2, GFP_KERNEL), 0);

	cur = dst.sgl;
	last = dst.sgl;
	copy_sg(&cur, &last, &ctx->sgt, PAGE_SIZE, 2 * PAGE_SIZE);
	sg_mark_end(last);

	sg = dst.sgl;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
	KUNIT_EXPECT_PTR_EQ(test, sg_page(sg), ctx->pages[1]);
	sg = sg_next(sg);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
	KUNIT_EXPECT_PTR_EQ(test, sg_page(sg), ctx->pages[2]);

	sg_free_table(&dst);
}

static void test_copy_sg_multi_sg_entries(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	struct sg_table dst;
	struct scatterlist *cur, *last, *sg;
	struct page *expected[] = {
		ctx->pages[2], ctx->pages[0],
		ctx->pages[3], ctx->pages[1],
	};
	int i;

	KUNIT_ASSERT_EQ(test, sg_alloc_table(&dst, 4, GFP_KERNEL), 0);

	cur = dst.sgl;
	last = dst.sgl;
	copy_sg(&cur, &last, &ctx->sgt, 2 * PAGE_SIZE, PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 0, PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 3 * PAGE_SIZE, PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 1 * PAGE_SIZE, PAGE_SIZE);
	sg_mark_end(last);

	sg = dst.sgl;
	for (i = 0; i < 4; i++) {
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
		KUNIT_EXPECT_PTR_EQ(test, sg_page(sg), expected[i]);
		sg = sg_next(sg);
	}

	sg_free_table(&dst);
}

/*
 * Verify that copy_sg produces the correct byte content at each output
 * position — not just the right struct page*, but the right offset within
 * that page. Stamps each source page with a distinct pattern, performs a
 * swap-halves copy, and reads back through the output sg entries.
 */
static void test_copy_sg_swap_halves_content(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	struct sg_table dst;
	struct scatterlist *cur, *last, *sg;
	/* Stamp each source page with a distinct nonzero byte value. */
	const u8 stamp[] = { 0x11, 0x22, 0x33, 0x44 };
	const u8 expected_stamp[] = { 0x33, 0x44, 0x11, 0x22 };
	int i;

	for (i = 0; i < NUM_TEST_PAGES; i++)
		memset(page_address(ctx->pages[i]), stamp[i], PAGE_SIZE);

	KUNIT_ASSERT_EQ(test, sg_alloc_table(&dst, NUM_TEST_PAGES, GFP_KERNEL), 0);

	cur = last = dst.sgl;
	copy_sg(&cur, &last, &ctx->sgt, 2 * PAGE_SIZE, 2 * PAGE_SIZE);
	copy_sg(&cur, &last, &ctx->sgt, 0,             2 * PAGE_SIZE);
	sg_mark_end(last);

	sg = dst.sgl;
	for (i = 0; i < NUM_TEST_PAGES; i++) {
		u8 *data;

		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
		data = page_address(sg_page(sg)) + sg->offset;
		KUNIT_EXPECT_EQ(test, data[0], expected_stamp[i]);
		KUNIT_EXPECT_EQ(test, data[sg->length - 1], expected_stamp[i]);
		sg = sg_next(sg);
	}

	sg_free_table(&dst);
}

/*
 * Copy half a page starting at PAGE_SIZE/2 within the first entry.
 * Exercises the skip/trim path in copy_sg where a region boundary
 * falls mid-entry: output entry must have the correct offset and length.
 */
static void test_copy_sg_subpage(struct kunit *test)
{
	struct dmabuf_reorder_test_ctx *ctx = test->priv;
	struct sg_table dst;
	struct scatterlist *cur, *last, *sg;

	KUNIT_ASSERT_EQ(test, sg_alloc_table(&dst, 1, GFP_KERNEL), 0);

	cur = last = dst.sgl;
	copy_sg(&cur, &last, &ctx->sgt, PAGE_SIZE / 2, PAGE_SIZE / 2);
	sg_mark_end(last);

	sg = dst.sgl;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sg);
	KUNIT_EXPECT_PTR_EQ(test, sg_page(sg), ctx->pages[0]);
	KUNIT_EXPECT_EQ(test, sg->length, (unsigned int)(PAGE_SIZE / 2));
	KUNIT_EXPECT_EQ(test, sg->offset, (unsigned int)(PAGE_SIZE / 2));

	sg_free_table(&dst);
}

static struct kunit_case dmabuf_reorder_test_cases[] = {
	/* regions_overlap */
	KUNIT_CASE(test_regions_no_overlap_adjacent),
	KUNIT_CASE(test_regions_overlap_partial),
	KUNIT_CASE(test_regions_overlap_identical),
	KUNIT_CASE(test_regions_single),
	KUNIT_CASE(test_regions_no_overlap_gap),
	KUNIT_CASE(test_regions_no_overlap_max),
	KUNIT_CASE(test_regions_overlap_max_last_two),
	/* count_sg */
	KUNIT_CASE(test_count_sg_full_range),
	KUNIT_CASE(test_count_sg_partial_range),
	KUNIT_CASE(test_count_sg_single_entry_partial),
	KUNIT_CASE(test_count_sg_no_overlap),
	KUNIT_CASE(test_count_sg_zero_length),
	KUNIT_CASE(test_count_sg_unaligned_start),
	/* copy_sg */
	KUNIT_CASE(test_copy_sg_identity),
	KUNIT_CASE(test_copy_sg_swap_halves),
	KUNIT_CASE(test_copy_sg_reverse_pages),
	KUNIT_CASE(test_copy_sg_subset),
	KUNIT_CASE(test_copy_sg_multi_sg_entries),
	KUNIT_CASE(test_copy_sg_swap_halves_content),
	KUNIT_CASE(test_copy_sg_subpage),
	{}
};

static struct kunit_suite dmabuf_reorder_test_suite = {
	.name = "dmabuf_reorder",
	.init = dmabuf_reorder_test_init,
	.exit = dmabuf_reorder_test_exit,
	.test_cases = dmabuf_reorder_test_cases,
};

kunit_test_suite(dmabuf_reorder_test_suite);

MODULE_AUTHOR("Anthony Bajoua <anthonybajoua@meta.com>");
MODULE_DESCRIPTION("KUnit tests for dmabuf_reorder scatterlist helpers");
MODULE_LICENSE("GPL");
