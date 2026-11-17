/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Meta Platforms, Inc. and affiliates
 */

#ifndef _HZOS_EXT_SOFT_RT_H
#define _HZOS_EXT_SOFT_RT_H

#include <linux/sched.h>
#include <linux/types.h>

void hzos_ext_soft_rt_init(void);
void hzos_ext_soft_rt_post_clone(struct task_struct *p);

ssize_t soft_rt_slice_write(s64 value);
s64 soft_rt_slice_get(void);
ssize_t soft_rt_limit_write(s64 value);
s64 soft_rt_limit_get(void);
ssize_t soft_rt_rq_window_write(s64 value);
s64 soft_rt_rq_window_get(void);
ssize_t soft_rt_rq_share_write(s64 value);
s64 soft_rt_rq_share_get(void);

#endif /* _HZOS_EXT_SOFT_RT_H */
