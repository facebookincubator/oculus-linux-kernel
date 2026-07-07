/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/bug.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/static_key.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "features.h"
#include "mem.h"

/* Undefine macros if previously defined */
#ifdef HZOS_EXT_FEATURE
#undef HZOS_EXT_FEATURE
#endif
#ifdef HZOS_EXT_NUMERIC_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE
#endif

/* Generate string array for boolean features */
#define HZOS_EXT_FEATURE(__feature, __enabled) (#__feature),
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) /* skip numeric */
static const char * const hzos_ext_feature_strings[] = {
#include "feature_table.h"
};
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

/* Generate string array for numeric features */
#define HZOS_EXT_FEATURE(__feature, __enabled) /* skip boolean */
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) (#__feature),
static const char * const hzos_ext_numeric_feature_strings[] = {
#include "feature_table.h"
};
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

/* Define static keys for boolean features */
#define feature_key__true  DEFINE_STATIC_KEY_TRUE
#define feature_key__false DEFINE_STATIC_KEY_FALSE
#define HZOS_EXT_FEATURE(__feature, __enabled) feature_key__ ##__enabled(static_key__ ##__feature);
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) /* skip numeric */
#include "feature_table.h"
#undef feature_key__true
#undef feature_key__false
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

/* Generate write callback array for numeric features */
#define HZOS_EXT_FEATURE(__feature, __enabled) /* skip boolean */
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) __write_cb,
static ssize_t (* const hzos_ext_numeric_write_cbs[])(s64) = {
#include "feature_table.h"
};
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

/* Generate read callback array for numeric features */
#define HZOS_EXT_FEATURE(__feature, __enabled) /* skip boolean */
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) __read_cb,
static s64 (* const hzos_ext_numeric_read_cbs[])(void) = {
#include "feature_table.h"
};
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

static void toggle_feature(enum hzos_ext_feature feature, bool disable)
{
	cpus_read_lock();
#define HZOS_EXT_FEATURE(__feature, __enabled)						\
		case HZOS_EXT_FEATURE_ ##__feature:					\
			if (disable)							\
				static_branch_disable_cpuslocked(&static_key__ ##__feature);	\
			else								\
				static_branch_enable_cpuslocked(&static_key__ ##__feature);	\
			break;
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) /* skip numeric */
	switch (feature) {
#include "feature_table.h"
	default:
		BUG();
	}
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE
	cpus_read_unlock();
}

static enum hzos_ext_feature hzos_ext_str_to_feature(const char *str, size_t len)
{
	int i;

	for (i = 0; i < HZOS_EXT_NUM_FEATURES; i++) {
		const char *feature_str = hzos_ext_feature_strings[i];

		if (!strncmp(str, feature_str, len) &&
		    strlen(feature_str) == len)
			return i;
	}

	return HZOS_EXT_NUM_FEATURES;
}

static enum hzos_ext_numeric_feature hzos_ext_str_to_numeric_feature(const char *str, size_t len)
{
	int i;

	for (i = 0; i < HZOS_EXT_NUM_NUMERIC_FEATURES; i++) {
		const char *feature_str = hzos_ext_numeric_feature_strings[i];

		if (!strncmp(str, feature_str, len) &&
		    strlen(feature_str) == len)
			return i;
	}

	return HZOS_EXT_NUM_NUMERIC_FEATURES;
}

#define MAX_FEATURE_LEN 64
#define KERN_BUFFER_LEN ((HZOS_EXT_NUM_FEATURES + HZOS_EXT_NUM_NUMERIC_FEATURES) * MAX_FEATURE_LEN)

static ssize_t features_read_procfs(struct file *file, char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	enum hzos_ext_feature i;
	enum hzos_ext_numeric_feature j;
	char tmpbuf[KERN_BUFFER_LEN];
	size_t remaining = KERN_BUFFER_LEN;
	char *kbuf = tmpbuf;

	/* Print boolean features */
	for (i = 0; i < HZOS_EXT_NUM_FEATURES && remaining > 0; i++) {
		ssize_t feat_str_len;
		const char *prefix;

		if (hzos_ext_feature_enabled(i))
			prefix = "";
		else
			prefix = "NO_";

		feat_str_len = scnprintf(kbuf, remaining, "%s%s\n",
					 prefix,
					 hzos_ext_feature_strings[i]);

		kbuf += feat_str_len;
		remaining -= feat_str_len;
	}

	/* Print numeric features */
	for (j = 0; j < HZOS_EXT_NUM_NUMERIC_FEATURES && remaining > 0; j++) {
		ssize_t feat_str_len;
		s64 value;

		value = hzos_ext_numeric_read_cbs[j]();
		feat_str_len = scnprintf(kbuf, remaining, "%s=%lld\n",
					 hzos_ext_numeric_feature_strings[j],
					 value);

		kbuf += feat_str_len;
		remaining -= feat_str_len;
	}

	return simple_read_from_buffer(ubuf, count, ppos, tmpbuf, KERN_BUFFER_LEN);
}

static ssize_t feature_write_procfs(struct file *file,
				    const char __user *ubuf, size_t count,
				    loff_t *ppos)
{
	char tmpbuf[MAX_FEATURE_LEN];
	enum hzos_ext_feature feature;
	enum hzos_ext_numeric_feature numeric_feature;
	bool disable;
	char *kbuf;
	char *eq_pos;
	size_t name_len;

	/* No partial writes */
	if (*ppos != 0)
		return -EINVAL;

	/* Shouldn't exceed maximum feat length */
	if (count > MAX_FEATURE_LEN || count <= 1)
		return -EINVAL;

	/* If invalid user buffer, return */
	memset(tmpbuf, 0, MAX_FEATURE_LEN);
	if (copy_from_user(tmpbuf, ubuf, count))
		return -EFAULT;
	kbuf = strstrip(tmpbuf);

	/* Check for numeric feature (NAME=VALUE format) */
	eq_pos = strchr(kbuf, '=');
	if (eq_pos) {
		s64 value;
		int ret;
		ssize_t cb_ret;

		name_len = eq_pos - kbuf;
		numeric_feature = hzos_ext_str_to_numeric_feature(kbuf, name_len);
		if (numeric_feature >= HZOS_EXT_NUM_NUMERIC_FEATURES)
			return -EINVAL;

		ret = kstrtos64(eq_pos + 1, 0, &value);
		if (ret)
			return ret;

		cb_ret = hzos_ext_numeric_write_cbs[numeric_feature](value);
		return cb_ret < 0 ? cb_ret : count;
	}

	/* Handle boolean feature */
	disable = !strncmp(kbuf, "NO_", 3);
	if (disable) {
		kbuf += 3;
	}

	name_len = strlen(kbuf);
	feature = hzos_ext_str_to_feature(kbuf, name_len);
	if (feature >= HZOS_EXT_NUM_FEATURES)
		return -EINVAL;
	toggle_feature(feature, disable);

#if defined(CONFIG_BALANCE_ANON_FILE_RECLAIM) && !defined(CONFIG_ANDROID_VENDOR_HOOKS)
	/*
	 * For kona kernel (no vendor hooks): notify vmscan.c of state change.
	 * The set_balance_anon_file_reclaim() function is implemented in mm/vmscan.c.
	 */
	if (feature == HZOS_EXT_FEATURE_BALANCE_ANON_FILE_RECLAIM) {
		extern void set_balance_anon_file_reclaim(bool enabled);
		set_balance_anon_file_reclaim(!disable);
	}
#endif

	return count;
}

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
const struct proc_ops features_proc_ops = {
	.proc_read	= features_read_procfs,
	.proc_write	= feature_write_procfs,
};
#else
const struct file_operations features_proc_ops = {
	.read		= features_read_procfs,
	.write		= feature_write_procfs,
};
#endif

void hzos_ext_features_init(void)
{
	BUG_ON(!proc_create("hzos_ext", S_IRUGO | S_IWUGO, NULL,
			    &features_proc_ops));
}
