// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "swd.h"
#include "fwupdate_operations.h"

static int mock_prepare_count;
static int mock_prepare_ret;

static int mock_child_prepare_count;
static int mock_child_prepare_ret;

static int mock_chip_erase_count;
static int mock_chip_erase_ret;

static int mock_target_erase(struct device *dev)
{
	return 0;
}

static int mock_target_prepare(struct device *dev)
{
	mock_prepare_count++;
	return mock_prepare_ret;
}

static int mock_child_target_prepare(struct device *dev)
{
	mock_child_prepare_count++;
	return mock_child_prepare_ret;
}

static int mock_target_chip_erase(struct device *dev)
{
	mock_chip_erase_count++;
	return mock_chip_erase_ret;
}

struct test_ctx {
	struct device dev;
	struct swd_dev_data devdata;
	struct swd_mcu_data children[2];
};

static void reset_mocks(void)
{
	mock_prepare_count = 0;
	mock_prepare_ret = 0;
	mock_child_prepare_count = 0;
	mock_child_prepare_ret = 0;
	mock_chip_erase_count = 0;
	mock_chip_erase_ret = 0;
}

static void init_test_ctx(struct test_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	mutex_init(&ctx->devdata.state_mutex);
	mutex_init(&ctx->devdata.write_mutex);
	dev_set_drvdata(&ctx->dev, &ctx->devdata);
}

static void test_check_ops_parent_erase_only(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	ctx.devdata.mcu_data.swd_ops.target_erase = mock_target_erase;
	ctx.devdata.num_children = 0;

	KUNIT_EXPECT_EQ(test, fwupdate_check_swd_ops(&ctx.dev), 0);
}

static void test_check_ops_child_erase_only(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	ctx.devdata.num_children = 1;
	ctx.devdata.child_mcu_data = ctx.children;
	ctx.children[0].swd_ops.target_erase = mock_target_erase;

	KUNIT_EXPECT_EQ(test, fwupdate_check_swd_ops(&ctx.dev), 0);
}

static void test_check_ops_both_have_erase(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	ctx.devdata.mcu_data.swd_ops.target_erase = mock_target_erase;
	ctx.devdata.num_children = 1;
	ctx.devdata.child_mcu_data = ctx.children;
	ctx.children[0].swd_ops.target_erase = mock_target_erase;

	KUNIT_EXPECT_EQ(test, fwupdate_check_swd_ops(&ctx.dev), -EINVAL);
}

static void test_check_ops_neither_has_erase(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	ctx.devdata.num_children = 1;
	ctx.devdata.child_mcu_data = ctx.children;

	KUNIT_EXPECT_EQ(test, fwupdate_check_swd_ops(&ctx.dev), -EINVAL);
}

static void test_check_ops_child_has_chip_erase(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	ctx.devdata.num_children = 1;
	ctx.devdata.child_mcu_data = ctx.children;
	ctx.children[0].swd_ops.target_erase = mock_target_erase;
	ctx.children[0].swd_ops.target_chip_erase = mock_target_chip_erase;

	KUNIT_EXPECT_EQ(test, fwupdate_check_swd_ops(&ctx.dev), -EINVAL);
}

static void test_check_ops_second_child_has_erase(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	ctx.devdata.num_children = 2;
	ctx.devdata.child_mcu_data = ctx.children;
	ctx.children[1].swd_ops.target_erase = mock_target_erase;

	KUNIT_EXPECT_EQ(test, fwupdate_check_swd_ops(&ctx.dev), 0);
}

#ifdef CONFIG_META_SWD_SYNCBOSS_NRF52XXX
static void test_init_ops_valid_flavor(struct kunit *test)
{
	struct test_ctx ctx;
	struct swd_mcu_data mcudata = {};

	init_test_ctx(&ctx);
	mcudata.target_flavor = "nrf52832";

	KUNIT_EXPECT_EQ(test, fwupdate_init_swd_ops(&ctx.dev, &mcudata), 0);
	KUNIT_EXPECT_TRUE(test, mcudata.swd_ops.target_erase != NULL);
}
#endif

static void test_init_ops_invalid_flavor(struct kunit *test)
{
	struct test_ctx ctx;
	struct swd_mcu_data mcudata = {};

	init_test_ctx(&ctx);
	mcudata.target_flavor = "nonexistent_chip";

	KUNIT_EXPECT_EQ(test, fwupdate_init_swd_ops(&ctx.dev, &mcudata), -EINVAL);
}

static void test_chip_erase_called_when_erase_all(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.erase_all = true;
	ctx.devdata.mcu_data.swd_ops.target_chip_erase = mock_target_chip_erase;

	KUNIT_EXPECT_EQ(test, fwupdate_update_chip_erase(&ctx.dev), 0);
	KUNIT_EXPECT_EQ(test, mock_chip_erase_count, 1);
}

static void test_chip_erase_skipped_when_not_erase_all(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.erase_all = false;
	ctx.devdata.mcu_data.swd_ops.target_chip_erase = mock_target_chip_erase;

	KUNIT_EXPECT_EQ(test, fwupdate_update_chip_erase(&ctx.dev), 0);
	KUNIT_EXPECT_EQ(test, mock_chip_erase_count, 0);
}

static void test_chip_erase_propagates_error(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.erase_all = true;
	ctx.devdata.mcu_data.swd_ops.target_chip_erase = mock_target_chip_erase;
	mock_chip_erase_ret = -EIO;

	KUNIT_EXPECT_EQ(test, fwupdate_update_chip_erase(&ctx.dev), -EIO);
	KUNIT_EXPECT_EQ(test, mock_chip_erase_count, 1);
}

static void test_prepare_calls_parent(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.mcu_data.swd_ops.target_prepare = mock_target_prepare;
	ctx.devdata.num_children = 0;

	KUNIT_EXPECT_EQ(test, fwupdate_update_prepare(&ctx.dev), 0);
	KUNIT_EXPECT_EQ(test, mock_prepare_count, 1);
}

static void test_prepare_calls_children(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.mcu_data.swd_ops.target_prepare = mock_target_prepare;
	ctx.devdata.num_children = 2;
	ctx.devdata.child_mcu_data = ctx.children;
	ctx.children[0].swd_ops.target_prepare = mock_child_target_prepare;
	ctx.children[1].swd_ops.target_prepare = mock_child_target_prepare;

	KUNIT_EXPECT_EQ(test, fwupdate_update_prepare(&ctx.dev), 0);
	KUNIT_EXPECT_EQ(test, mock_prepare_count, 1);
	KUNIT_EXPECT_EQ(test, mock_child_prepare_count, 2);
}

static void test_prepare_skips_child_without_prepare(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.mcu_data.swd_ops.target_prepare = mock_target_prepare;
	ctx.devdata.num_children = 2;
	ctx.devdata.child_mcu_data = ctx.children;
	ctx.children[1].swd_ops.target_prepare = mock_child_target_prepare;

	KUNIT_EXPECT_EQ(test, fwupdate_update_prepare(&ctx.dev), 0);
	KUNIT_EXPECT_EQ(test, mock_prepare_count, 1);
	KUNIT_EXPECT_EQ(test, mock_child_prepare_count, 1);
}

static void test_prepare_parent_error(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.mcu_data.swd_ops.target_prepare = mock_target_prepare;
	mock_prepare_ret = -ENXIO;

	KUNIT_EXPECT_EQ(test, fwupdate_update_prepare(&ctx.dev), -ENXIO);
	KUNIT_EXPECT_EQ(test, mock_prepare_count, 1);
}

static void test_prepare_child_error(struct kunit *test)
{
	struct test_ctx ctx;

	init_test_ctx(&ctx);
	reset_mocks();
	ctx.devdata.mcu_data.swd_ops.target_prepare = mock_target_prepare;
	ctx.devdata.num_children = 2;
	ctx.devdata.child_mcu_data = ctx.children;
	ctx.children[0].swd_ops.target_prepare = mock_child_target_prepare;
	ctx.children[1].swd_ops.target_prepare = mock_child_target_prepare;
	mock_child_prepare_ret = -ETIMEDOUT;

	KUNIT_EXPECT_EQ(test, fwupdate_update_prepare(&ctx.dev), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock_child_prepare_count, 1);
}

static void test_show_idle(struct kunit *test)
{
	struct test_ctx ctx;
	char *buf;
	ssize_t ret;

	init_test_ctx(&ctx);
	ctx.devdata.fw_update_state = FW_UPDATE_STATE_IDLE;
	buf = kunit_kzalloc(test, PAGE_SIZE, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	ret = fwupdate_update_firmware_show(&ctx.dev, buf);
	KUNIT_EXPECT_GT(test, ret, (ssize_t)0);
	KUNIT_EXPECT_STREQ(test, buf, "idle\n");
}

static void test_show_writing(struct kunit *test)
{
	struct test_ctx ctx;
	char *buf;
	ssize_t ret;

	init_test_ctx(&ctx);
	ctx.devdata.fw_update_state = FW_UPDATE_STATE_WRITING_TO_HW;
	atomic_set(&ctx.devdata.mcu_data.fw_update_steps_done, 5);
	atomic_set(&ctx.devdata.mcu_data.fw_update_steps_total, 10);
	buf = kunit_kzalloc(test, PAGE_SIZE, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	ret = fwupdate_update_firmware_show(&ctx.dev, buf);
	KUNIT_EXPECT_GT(test, ret, (ssize_t)0);
	KUNIT_EXPECT_STREQ(test, buf, "writing 5/10\n");
}

static void test_show_error(struct kunit *test)
{
	struct test_ctx ctx;
	char *buf;
	ssize_t ret;

	init_test_ctx(&ctx);
	ctx.devdata.fw_update_state = FW_UPDATE_STATE_ERROR;
	buf = kunit_kzalloc(test, PAGE_SIZE, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	ret = fwupdate_update_firmware_show(&ctx.dev, buf);
	KUNIT_EXPECT_GT(test, ret, (ssize_t)0);
	KUNIT_EXPECT_STREQ(test, buf, "error\n");
}

static struct kunit_case fwupdate_manager_test_cases[] = {
	KUNIT_CASE(test_check_ops_parent_erase_only),
	KUNIT_CASE(test_check_ops_child_erase_only),
	KUNIT_CASE(test_check_ops_both_have_erase),
	KUNIT_CASE(test_check_ops_neither_has_erase),
	KUNIT_CASE(test_check_ops_child_has_chip_erase),
	KUNIT_CASE(test_check_ops_second_child_has_erase),
#ifdef CONFIG_META_SWD_SYNCBOSS_NRF52XXX
	KUNIT_CASE(test_init_ops_valid_flavor),
#endif
	KUNIT_CASE(test_init_ops_invalid_flavor),
	KUNIT_CASE(test_chip_erase_called_when_erase_all),
	KUNIT_CASE(test_chip_erase_skipped_when_not_erase_all),
	KUNIT_CASE(test_chip_erase_propagates_error),
	KUNIT_CASE(test_prepare_calls_parent),
	KUNIT_CASE(test_prepare_calls_children),
	KUNIT_CASE(test_prepare_skips_child_without_prepare),
	KUNIT_CASE(test_prepare_parent_error),
	KUNIT_CASE(test_prepare_child_error),
	KUNIT_CASE(test_show_idle),
	KUNIT_CASE(test_show_writing),
	KUNIT_CASE(test_show_error),
	{}
};

static struct kunit_suite fwupdate_manager_test_suite = {
	.name = "fwupdate_manager",
	.test_cases = fwupdate_manager_test_cases,
};

kunit_test_suite(fwupdate_manager_test_suite);

MODULE_AUTHOR("Meta");
MODULE_DESCRIPTION("KUnit tests for the SWD firmware update manager");
MODULE_LICENSE("GPL");
