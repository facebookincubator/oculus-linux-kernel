/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#ifndef _ORCHESTRATOR_SCHED_H
#define _ORCHESTRATOR_SCHED_H

#include <linux/sched.h>

void orchestrator_sched_post_clone(struct task_struct *p);
void orchestrator_sched_init(void);

#endif /* _ORCHESTRATOR_SCHED_H */
