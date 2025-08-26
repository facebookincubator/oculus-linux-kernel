/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#ifndef _ORCHESTRATOR_FLAGS_H
#define _ORCHESTRATOR_FLAGS_H

#include <linux/sched.h>
#include <linux/types.h>

#ifdef ORCHESTRATOR_FLAG
#undef ORCHESTRATOR_FLAG
#endif

#define ORCHESTRATOR_FLAG(__flag) ORCHESTRATOR_FLAG_SHIFT__ ##__flag,
enum {
#include "flags_table.h"
	ORCHESTRATOR_NUM_FLAGS,
};
#undef ORCHESTRATOR_FLAG

#define ORCHESTRATOR_FLAG(__flag) ORCHESTRATOR_FLAG_ ##__flag = 1ULL << ORCHESTRATOR_FLAG_SHIFT__ ##__flag,
enum orchestrator_flags {
#include "flags_table.h"
};
#undef ORCHESTRATOR_FLAG

int orchestrator_flag_to_idx(enum orchestrator_flags flag);
enum orchestrator_flags orchestrator_idx_to_flag(int idx);
const char *orchestrator_flag_to_str(enum orchestrator_flags flag);
enum orchestrator_flags orchestrator_str_to_flag(const char *str, size_t len);

/**
 * orchestrator_task_has_flag() - Check whether the specified task has the specified
 * flag set.
 *
 * @p: The task being checked
 * @flag: The flag in question
 */
static inline bool orchestrator_task_has_flag(const struct task_struct *p, enum orchestrator_flags flag)
{
	return READ_ONCE(p->orchestrator.flags) & flag;
}

#endif /* _ORCHESTRATOR_FLAGS_H */
