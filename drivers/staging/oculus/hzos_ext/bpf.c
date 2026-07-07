// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta HzOS Ext BPF subsystem
 *
 * Copyright (c) 2026 Meta Platforms, Inc. and affiliates
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <uapi/linux/bpf.h>

#include "bpf.h"
#include "flags.h"

static void check_bpf_helper_handler(void *unused, int func_id, int *ret)
{
	if (func_id == BPF_FUNC_probe_write_user &&
	    !hzos_ext_task_has_flag(current,
				    HZOS_EXT_FLAG_ALLOW_BPF_PROBE_WRITE_USER))
		*ret = -EPERM;
}

void hzos_ext_check_bpf_helper(int func_id, int *ret)
{
	check_bpf_helper_handler(NULL, func_id, ret);
}

#ifdef CONFIG_ANDROID_VENDOR_HOOKS

#include <trace/hooks/syscall_check.h>

void hzos_ext_bpf_init(void)
{
	register_trace_android_vh_check_bpf_helper(
		check_bpf_helper_handler, NULL);
}

#else

void hzos_ext_bpf_init(void)
{
}

#endif /* CONFIG_ANDROID_VENDOR_HOOKS */
