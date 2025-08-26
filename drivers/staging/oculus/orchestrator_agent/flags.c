// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/bug.h>
#include <linux/proc_fs.h>
#include <linux/orchestrator.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>

#include "flags.h"

#ifdef ORCHESTRATOR_FLAG
#undef ORCHESTRATOR_FLAG
#endif

#define MAX_FLAGS_LEN 64

ssize_t orchestrator_flags_read_procfs(struct task_struct *task,
				       char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	size_t remaining = MAX_FLAGS_LEN;
	int i;
	bool had_prior = false;
	char tmpbuf[MAX_FLAGS_LEN];
	char *kbuf = tmpbuf;

	for (i = 0; i < ORCHESTRATOR_NUM_FLAGS && remaining > 0; i++) {
		ssize_t flag_str_len;
		u64 flag = orchestrator_idx_to_flag(i);
		const char *prefix, *prior;

		if (READ_ONCE(task->orchestrator.flags) & flag)
			prefix = "";
		else
			prefix = "NO_";

		if (had_prior)
			prior = " ";
		else
			prior = "";

		flag_str_len = scnprintf(kbuf, remaining, "%s%s%s", prior,
					 prefix,
					 orchestrator_flag_to_str(flag));

		kbuf += flag_str_len;
		remaining -= flag_str_len;
		had_prior = true;
	}

	return simple_read_from_buffer(ubuf, count, ppos, tmpbuf, MAX_FLAGS_LEN);
}

ssize_t orchestrator_flags_write_procfs(struct task_struct *task,
				        const char __user *ubuf,
				        size_t count, loff_t *ppos)
{
	char tmpbuf[MAX_FLAGS_LEN];
	enum orchestrator_flags flag;
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

	flag = orchestrator_str_to_flag(kbuf, len);
	if (!flag)
		return -ENOENT;

	/* Ensure writes are consistent */
	mutex_lock(&task->orchestrator.lock);
	if (off)
		WRITE_ONCE(task->orchestrator.flags,
			   (task->orchestrator.flags & ~flag));
	else
		WRITE_ONCE(task->orchestrator.flags,
			   (task->orchestrator.flags | flag));
	mutex_unlock(&task->orchestrator.lock);
	return count;
}

#define ORCHESTRATOR_FLAG(__flag) #__flag,
static const char * const orchestrator_flag_strings[] = {
#include "flags_table.h"
	"<UNKNOWN>"
};
#undef ORCHESTRATOR_FLAG

int orchestrator_flag_to_idx(enum orchestrator_flags flag)
{
#define ORCHESTRATOR_FLAG(__flag) case(ORCHESTRATOR_FLAG_ ##__flag): return ORCHESTRATOR_FLAG_SHIFT__ ##__flag;
	switch (flag) {
#include "flags_table.h"
#undef ORCHESTRATOR_FLAG
		default:
			WARN_ONCE(1, "Invalid Orchestrator Agent flag %llu", (u64)flag);
			return ORCHESTRATOR_NUM_FLAGS;
	}
}

enum orchestrator_flags orchestrator_idx_to_flag(int idx)
{
#define ORCHESTRATOR_FLAG(__flag) case(ORCHESTRATOR_FLAG_SHIFT__ ##__flag): return ORCHESTRATOR_FLAG_ ##__flag;
	switch (idx) {
#include "flags_table.h"
#undef ORCHESTRATOR_FLAG
		default:
			WARN_ONCE(1, "Invalid Orchestrator Agent index %d", idx);
			return 0;
	}
}

const char *orchestrator_flag_to_str(enum orchestrator_flags flag)
{
	return orchestrator_flag_strings[orchestrator_flag_to_idx(flag)];
}

enum orchestrator_flags orchestrator_str_to_flag(const char *str, size_t len)
{
	int i;

	for (i = 0; i < ORCHESTRATOR_NUM_FLAGS; i++) {
		const char *flag_str = orchestrator_flag_strings[i];

		if (!strncmp(str, flag_str, len))
			return orchestrator_idx_to_flag(i);
	}

	return 0;
}
