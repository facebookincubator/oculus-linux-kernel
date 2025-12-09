// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta Orchestrator Agent memory subsystem
 *
 * Copyright (c) 2025 Meta Platforms, Inc. and affiliates
 */

#ifdef CONFIG_ANDROID_VENDOR_HOOKS

#include <trace/hooks/vmscan.h>
#include "features.h"

static void balance_reclaim(void *unused, bool *balance_anon_file_reclaim)
{
	*balance_anon_file_reclaim = orchestrator_feature_enabled(
		ORCHESTRATOR_FEATURE_BALANCE_ANON_FILE_RECLAIM);
}

void orchestrator_mem_init(void)
{
	int ret;

	pr_info("Registering balance_anon_file_reclaim hook\n");
	ret = register_trace_android_rvh_set_balance_anon_file_reclaim(balance_reclaim, NULL);
	if (ret) {
		pr_err("Failed to register balance_anon_file_reclaim hook\n");
	}
}

#else

void orchestrator_mem_init(void)
{
}

#endif /* CONFIG_ANDROID_VENDOR_HOOKS */
