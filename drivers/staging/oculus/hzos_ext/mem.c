// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta HzOS Ext memory subsystem
 *
 * Copyright (c) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "features.h"
#include "mem.h"

/* MEM_HAIRCUT feature state */
static DEFINE_MUTEX(mem_haircut_mutex);
static void *mem_haircut_ptr;
static s64 mem_haircut_size;

ssize_t mem_haircut_write(s64 value)
{
	ssize_t status = 0;
	void *tmp = NULL;

	if (value < 0)
		return -EINVAL;

	mutex_lock(&mem_haircut_mutex);
	if (value > 0) {
		tmp = kvmalloc(value, GFP_KERNEL);
		if (!tmp) {
			status = -ENOMEM;
			goto out;
		}
	}
	kvfree(mem_haircut_ptr);
	mem_haircut_ptr = tmp;
	WRITE_ONCE(mem_haircut_size, value);
out:
	mutex_unlock(&mem_haircut_mutex);
	return status;
}

s64 mem_haircut_get(void)
{
	return READ_ONCE(mem_haircut_size);
}

#ifdef CONFIG_ANDROID_VENDOR_HOOKS

#include <trace/hooks/vmscan.h>

static void balance_reclaim(void *unused, bool *balance_anon_file_reclaim)
{
	*balance_anon_file_reclaim = hzos_ext_feature_enabled(
		HZOS_EXT_FEATURE_BALANCE_ANON_FILE_RECLAIM);
}

void hzos_ext_mem_init(void)
{
	int ret;

	pr_info("Registering balance_anon_file_reclaim hook\n");
	ret = register_trace_android_rvh_set_balance_anon_file_reclaim(
		balance_reclaim, NULL);
	if (ret)
		pr_err("Failed to register balance_anon_file_reclaim hook\n");
}

#else

void hzos_ext_mem_init(void)
{
}

#endif /* CONFIG_ANDROID_VENDOR_HOOKS */
