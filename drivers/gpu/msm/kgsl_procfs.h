/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2002,2008-2011,2013,2015,2017 The Linux Foundation.
 * All rights reserved.
 */
#ifndef _KGSL_PROCFS_H
#define _KGSL_PROCFS_H

struct kgsl_process_private;
struct kgsl_mem_entry;

void kgsl_core_procfs_init(void);
void kgsl_core_procfs_close(void);

void kgsl_process_init_procfs(struct kgsl_process_private *priv);

#endif
