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

#ifdef HZOS_EXT_FEATURE
#undef HZOS_EXT_FEATURE
#endif

#define HZOS_EXT_FEATURE(__feature, __enabled) #__feature,
static const char * const hzos_ext_feature_strings[] = {
#include "feature_table.h"
};
#undef HZOS_EXT_FEATURE

#define feature_key__true  DEFINE_STATIC_KEY_TRUE
#define feature_key__false DEFINE_STATIC_KEY_FALSE
#define HZOS_EXT_FEATURE(__feature, __enabled) feature_key__ ##__enabled(static_key__ ##__feature);
#include "feature_table.h"
#undef feature_key__true
#undef feature_key__false
#undef HZOS_EXT_FEATURE

static void toggle_feature(enum hzos_ext_feature feature, bool disable)
{
	cpus_read_lock();
	switch (feature) {
#define HZOS_EXT_FEATURE(__feature, __enabled)						\
		case HZOS_EXT_FEATURE_ ##__feature:						\
			if (disable)								\
				static_branch_disable_cpuslocked(&static_key__ ##__feature);	\
			else									\
				static_branch_enable_cpuslocked(&static_key__ ##__feature);	\
			break;
#include "feature_table.h"
#undef HZOS_EXT_FEATURE
		default:
			BUG();
	}
	cpus_read_unlock();
}

static enum hzos_ext_feature hzos_ext_str_to_feature(const char *str, size_t len)
{
	int i;

	for (i = 0; i < HZOS_EXT_NUM_FEATURES; i++) {
		const char *feature_str = hzos_ext_feature_strings[i];

		if (!strncmp(str, feature_str, len))
			return i;
	}

	return HZOS_EXT_NUM_FEATURES;
}

#define MAX_FEATURE_LEN 64
#define KERN_BUFFER_LEN (HZOS_EXT_NUM_FEATURES * MAX_FEATURE_LEN)

static ssize_t features_read_procfs(struct file *file, char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	enum hzos_ext_feature i;
	bool had_prior = false;
	char tmpbuf[KERN_BUFFER_LEN];
	size_t remaining = KERN_BUFFER_LEN;
	char *kbuf = tmpbuf;

	for (i = 0; i < HZOS_EXT_NUM_FEATURES && remaining > 0; i++) {
		ssize_t feat_str_len;
		const char *prefix, *prior;

		if (hzos_ext_feature_enabled(i))
			prefix = "";
		else
			prefix = "NO_";

		if (had_prior)
			prior = " ";
		else
			prior = "";

		feat_str_len = scnprintf(kbuf, remaining, "%s%s%s", prior,
					 prefix,
					 hzos_ext_feature_strings[i]);

		kbuf += feat_str_len;
		remaining -= feat_str_len;
		had_prior = true;
	}

	return simple_read_from_buffer(ubuf, count, ppos, tmpbuf, KERN_BUFFER_LEN);
}

static ssize_t feature_write_procfs(struct file *file,
				    const char __user *ubuf, size_t count,
				    loff_t *ppos)
{
	char tmpbuf[MAX_FEATURE_LEN];
	enum hzos_ext_feature feature;
	bool disable;
	char *kbuf;
	size_t len = MAX_FEATURE_LEN;

	/* No partial writes */
	if (*ppos != 0)
		return -EINVAL;

	/* Shouldn't exceed maximum feat length */
	if (count > MAX_FEATURE_LEN || count <= 4)
		return -EINVAL;

	/* If invalid user buffer, return */
	memset(tmpbuf, 0, MAX_FEATURE_LEN);
	if (copy_from_user(tmpbuf, ubuf, count))
		return -EFAULT;
	kbuf = strstrip(tmpbuf);

	disable = !strncmp(kbuf, "NO_", 3);
	if (disable) {
		kbuf += 3;
		len -= 3;
	}

	feature = hzos_ext_str_to_feature(kbuf, len);
	if (feature >= HZOS_EXT_NUM_FEATURES)
		return -EINVAL;
	toggle_feature(feature, disable);

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
