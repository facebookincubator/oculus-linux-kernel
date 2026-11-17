// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/hzos_ext_tag.h>

static void store_bumps_gen_only_on_change(struct kunit *test)
{
	struct hzos_ext_tag tag = {};
	char a[] = "THREAD_TAG_TIMEWARP", b[] = "THREAD_TAG_TIMEWARP\n", c[] = "  SFMainPolicy ";

	KUNIT_EXPECT_TRUE(test, hzos_ext_tag_store(&tag, a));
	KUNIT_EXPECT_EQ(test, tag.gen, 1u);
	KUNIT_EXPECT_FALSE(test, hzos_ext_tag_store(&tag, b));	/* newline stripped, unchanged */
	KUNIT_EXPECT_EQ(test, tag.gen, 1u);
	KUNIT_EXPECT_TRUE(test, hzos_ext_tag_store(&tag, c));
	KUNIT_EXPECT_STREQ(test, (const char *)tag.buf, "SFMainPolicy");
	KUNIT_EXPECT_EQ(test, tag.gen, 2u);
}

static void store_keeps_longest_accepted_value(struct kunit *test)
{
	struct hzos_ext_tag tag = {};
	char v[HZOS_EXT_TASK_PROFILES_LEN];

	memset(v, 'a', sizeof(v) - 1);
	v[sizeof(v) - 1] = '\0';	/* LEN-1 chars: the largest write the handler accepts */
	KUNIT_EXPECT_TRUE(test, hzos_ext_tag_store(&tag, v));
	KUNIT_EXPECT_EQ(test, strlen(tag.buf), (size_t)HZOS_EXT_TASK_PROFILES_LEN - 1);
}

static void store_truncates_over_longest_accepted_value(struct kunit *test)
{
	struct hzos_ext_tag tag = {};
	char v[HZOS_EXT_TASK_PROFILES_LEN * 2];

	memset(v, 'a', HZOS_EXT_TASK_PROFILES_LEN);
	memset(v + HZOS_EXT_TASK_PROFILES_LEN, 'b',
			sizeof(v) - HZOS_EXT_TASK_PROFILES_LEN - 1);
	v[sizeof(v) - 1] = '\0';
	KUNIT_EXPECT_TRUE(test, hzos_ext_tag_store(&tag, v));
	KUNIT_EXPECT_EQ(test, strlen(tag.buf), (size_t)HZOS_EXT_TASK_PROFILES_LEN - 1);
	KUNIT_EXPECT_EQ(test, tag.buf[HZOS_EXT_TASK_PROFILES_LEN - 2], (char)'a');
}

static void clearing_counts_as_change(struct kunit *test)
{
	struct hzos_ext_tag tag = {};
	char a[] = "X", empty[] = "\n";

	hzos_ext_tag_store(&tag, a);
	KUNIT_EXPECT_TRUE(test, hzos_ext_tag_store(&tag, empty));
	KUNIT_EXPECT_STREQ(test, (const char *)tag.buf, "");
	KUNIT_EXPECT_EQ(test, tag.gen, 2u);
}

static void process_slot_is_leaders_thread_slot_is_own(struct kunit *test)
{
	struct task_struct *leader = kunit_kzalloc(test, sizeof(*leader), GFP_KERNEL);
	struct task_struct *thread = kunit_kzalloc(test, sizeof(*thread), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, leader);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, thread);
	leader->group_leader = leader;
	thread->group_leader = leader;
	KUNIT_EXPECT_PTR_EQ(test, hzos_ext_tag_owner_of(thread, HZOS_EXT_TAG_PROCESS), leader);
	KUNIT_EXPECT_PTR_EQ(test, hzos_ext_tag_owner_of(thread, HZOS_EXT_TAG_THREAD), thread);
	KUNIT_EXPECT_PTR_EQ(test, hzos_ext_tag_slot(leader, HZOS_EXT_TAG_PROCESS),
			    &leader->hzos_ext.task_profiles);
	KUNIT_EXPECT_PTR_EQ(test, hzos_ext_tag_slot(thread, HZOS_EXT_TAG_THREAD),
			    &thread->hzos_ext.thread_profiles);
}

static struct kunit_case hzos_ext_tag_cases[] = {
	KUNIT_CASE(store_bumps_gen_only_on_change),
	KUNIT_CASE(store_keeps_longest_accepted_value),
	KUNIT_CASE(store_truncates_over_longest_accepted_value),
	KUNIT_CASE(clearing_counts_as_change),
	KUNIT_CASE(process_slot_is_leaders_thread_slot_is_own),
	{}
};
static struct kunit_suite hzos_ext_tag_suite = {
	.name = "hzos_ext_tag",
	.test_cases = hzos_ext_tag_cases,
};
kunit_test_suite(hzos_ext_tag_suite);
MODULE_LICENSE("GPL");
