/*
 * Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/bug.h>
#include <linux/proc_fs.h>
#include <linux/hzos_ext.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "flags.h"

#ifdef HZOS_EXT_FLAG
#undef HZOS_EXT_FLAG
#endif

#define MAX_FLAGS_LEN 64

ssize_t hzos_ext_flags_read_procfs(struct task_struct *task,
				       char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	size_t remaining = MAX_FLAGS_LEN;
	int i;
	bool had_prior = false;
	char tmpbuf[MAX_FLAGS_LEN];
	char *kbuf = tmpbuf;

	for (i = 0; i < HZOS_EXT_NUM_FLAGS && remaining > 0; i++) {
		ssize_t flag_str_len;
		u64 flag = hzos_ext_idx_to_flag(i);
		const char *prefix, *prior;

		if (READ_ONCE(task->hzos_ext.flags) & flag)
			prefix = "";
		else
			prefix = "NO_";

		if (had_prior)
			prior = " ";
		else
			prior = "";

		flag_str_len = scnprintf(kbuf, remaining, "%s%s%s", prior,
					 prefix,
					 hzos_ext_flag_to_str(flag));

		kbuf += flag_str_len;
		remaining -= flag_str_len;
		had_prior = true;
	}

	return simple_read_from_buffer(ubuf, count, ppos, tmpbuf, MAX_FLAGS_LEN);
}

ssize_t hzos_ext_flags_write_procfs(struct task_struct *task,
				        const char __user *ubuf,
				        size_t count, loff_t *ppos)
{
	char tmpbuf[MAX_FLAGS_LEN];
	enum hzos_ext_flags flag;
	bool off;
	char *kbuf;
	size_t len = MAX_FLAGS_LEN;

	/* No partial writes. */
	if (*ppos != 0)
		return -EINVAL;

	/* Shouldn't exceed maxmum flag length */
	if (count > MAX_FLAGS_LEN || count <= 3)
		return -EINVAL;

	/* If invalid user buffer, return */
	memset(tmpbuf, 0, MAX_FLAGS_LEN);
	if (copy_from_user(tmpbuf, ubuf, count))
		return -EFAULT;
	kbuf = strstrip(tmpbuf);

	off = !strncmp(kbuf, "NO_", 3);
	if (off) {
		kbuf += 3;
		len -= 3;
	}

	flag = hzos_ext_str_to_flag(kbuf, len);
	if (!flag)
		return -ENOENT;

	/* Ensure writes are consistent */
	mutex_lock(&task->hzos_ext.lock);
	if (off)
		WRITE_ONCE(task->hzos_ext.flags,
			   (task->hzos_ext.flags & ~flag));
	else
		WRITE_ONCE(task->hzos_ext.flags,
			   (task->hzos_ext.flags | flag));
	mutex_unlock(&task->hzos_ext.lock);
	return count;
}

#define HZOS_EXT_FLAG(__flag) #__flag,
static const char * const hzos_ext_flag_strings[] = {
#include "flags_table.h"
	"<UNKNOWN>"
};
#undef HZOS_EXT_FLAG

int hzos_ext_flag_to_idx(enum hzos_ext_flags flag)
{
#define HZOS_EXT_FLAG(__flag) case(HZOS_EXT_FLAG_ ##__flag): return HZOS_EXT_FLAG_SHIFT__ ##__flag;
	switch (flag) {
#include "flags_table.h"
#undef HZOS_EXT_FLAG
		default:
			WARN_ONCE(1, "Invalid HzOS Ext flag %llu", (u64)flag);
			return HZOS_EXT_NUM_FLAGS;
	}
}

enum hzos_ext_flags hzos_ext_idx_to_flag(int idx)
{
#define HZOS_EXT_FLAG(__flag) case(HZOS_EXT_FLAG_SHIFT__ ##__flag): return HZOS_EXT_FLAG_ ##__flag;
	switch (idx) {
#include "flags_table.h"
#undef HZOS_EXT_FLAG
		default:
			WARN_ONCE(1, "Invalid HzOS Ext index %d", idx);
			return 0;
	}
}

const char *hzos_ext_flag_to_str(enum hzos_ext_flags flag)
{
	return hzos_ext_flag_strings[hzos_ext_flag_to_idx(flag)];
}

enum hzos_ext_flags hzos_ext_str_to_flag(const char *str, size_t len)
{
	int i;

	for (i = 0; i < HZOS_EXT_NUM_FLAGS; i++) {
		const char *flag_str = hzos_ext_flag_strings[i];

		if (!strncmp(str, flag_str, len))
			return hzos_ext_idx_to_flag(i);
	}

	return 0;
}
