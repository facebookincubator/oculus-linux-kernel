// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/gpu_cgroup.h>	/* self-contained: gpu_freq_parse_khz */

static void test_parse_mhz(struct kunit *test)
{
	u32 khz = 123;
	char b1[] = "495";

	KUNIT_EXPECT_EQ(test, gpu_freq_parse_khz(b1, &khz), 0);
	KUNIT_EXPECT_EQ(test, khz, 495000u);
}

static void test_parse_unset_keywords(struct kunit *test)
{
	u32 khz = 999;
	char bmax[] = "max\n", bmin[] = "min", bzero[] = "0";

	KUNIT_EXPECT_EQ(test, gpu_freq_parse_khz(bmax, &khz), 0);
	KUNIT_EXPECT_EQ(test, khz, 0u);
	khz = 999;
	KUNIT_EXPECT_EQ(test, gpu_freq_parse_khz(bmin, &khz), 0);
	KUNIT_EXPECT_EQ(test, khz, 0u);
	khz = 999;
	KUNIT_EXPECT_EQ(test, gpu_freq_parse_khz(bzero, &khz), 0);
	KUNIT_EXPECT_EQ(test, khz, 0u);
}

static void test_parse_invalid(struct kunit *test)
{
	u32 khz;
	char bad[] = "not-a-number";

	KUNIT_EXPECT_LT(test, gpu_freq_parse_khz(bad, &khz), 0);
}

static void test_parse_overflow(struct kunit *test)
{
	u32 khz;
	char big[] = "5000000";	/* 5,000,000 MHz * 1000 > U32_MAX -> -ERANGE */

	KUNIT_EXPECT_LT(test, gpu_freq_parse_khz(big, &khz), 0);
}

static struct kunit_case gpu_freq_cases[] = {
	KUNIT_CASE(test_parse_mhz),
	KUNIT_CASE(test_parse_unset_keywords),
	KUNIT_CASE(test_parse_invalid),
	KUNIT_CASE(test_parse_overflow),
	{}
};

static struct kunit_suite gpu_freq_suite = {
	.name = "hzos_ext_gpu_freq",
	.test_cases = gpu_freq_cases,
};

kunit_test_suite(gpu_freq_suite);

MODULE_LICENSE("GPL");
