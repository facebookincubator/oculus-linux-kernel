/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#ifndef _HZOS_EXT_SCHED_H
#define _HZOS_EXT_SCHED_H

#include <linux/sched.h>

void hzos_ext_sched_post_clone(struct task_struct *p);
void hzos_ext_sched_init(void);

#endif /* _HZOS_EXT_SCHED_H */
