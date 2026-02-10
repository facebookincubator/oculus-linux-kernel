/*
 * Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#ifndef _HZOS_EXT_FLAGS_H
#define _HZOS_EXT_FLAGS_H

#include <linux/sched.h>
#include <linux/types.h>

#ifdef HZOS_EXT_FLAG
#undef HZOS_EXT_FLAG
#endif

#define HZOS_EXT_FLAG(__flag) HZOS_EXT_FLAG_SHIFT__ ##__flag,
enum {
#include "flags_table.h"
	HZOS_EXT_NUM_FLAGS,
};
#undef HZOS_EXT_FLAG

#define HZOS_EXT_FLAG(__flag) HZOS_EXT_FLAG_ ##__flag = 1ULL << HZOS_EXT_FLAG_SHIFT__ ##__flag,
enum hzos_ext_flags {
#include "flags_table.h"
};
#undef HZOS_EXT_FLAG

int hzos_ext_flag_to_idx(enum hzos_ext_flags flag);
enum hzos_ext_flags hzos_ext_idx_to_flag(int idx);
const char *hzos_ext_flag_to_str(enum hzos_ext_flags flag);
enum hzos_ext_flags hzos_ext_str_to_flag(const char *str, size_t len);

/**
 * hzos_ext_task_has_flag() - Check whether the specified task has the specified
 * flag set.
 *
 * @p: The task being checked
 * @flag: The flag in question
 */
static inline bool hzos_ext_task_has_flag(const struct task_struct *p, enum hzos_ext_flags flag)
{
	return READ_ONCE(p->hzos_ext.flags) & flag;
}

#endif /* _HZOS_EXT_FLAGS_H */
