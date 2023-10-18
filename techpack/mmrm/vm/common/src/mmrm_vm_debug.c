// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/slab.h>

#include "mmrm_vm_debug.h"

int mmrm_vm_debug = MMRM_VM_ERR | MMRM_VM_WARN | MMRM_VM_PRINTK;

/**
 * msm_mmrm_debugfs_init - init debug sys entry
 */
struct dentry *msm_mmrm_debugfs_init(void)
{
	struct dentry *dir;

	/* create a directory in debugfs root (/sys/kernel/debug) */
	dir = debugfs_create_dir("mmrm_vm", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		d_mpr_e("%s: Call to debugfs_create_dir(%s) failed!\n", __func__, "mmrm");
		goto failed_create_dir;
	}

	/* add other params here */
	debugfs_create_u32("debug_level", 0644, dir, &mmrm_vm_debug);

	return dir;

failed_create_dir:
	d_mpr_e("%s: error\n", __func__);
	return NULL;
}

/**
 * msm_mmrm_debugfs_deinit - de-init debug sys entry
 * dir: directory in debugfs root
 */
void msm_mmrm_debugfs_deinit(struct dentry *dir)
{
	debugfs_remove_recursive(dir);
}

