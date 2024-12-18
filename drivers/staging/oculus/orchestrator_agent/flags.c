// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/bug.h>
#include <linux/string.h>
#include <linux/types.h>

#include "flags.h"

#ifdef ORCHESTRATOR_FLAG
#undef ORCHESTRATOR_FLAG
#endif

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
