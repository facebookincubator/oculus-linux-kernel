// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2008-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/ptrace.h>

#include "kgsl_debugfs.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

/*default log levels is error for everything*/
#define KGSL_LOG_LEVEL_MAX     7

struct dentry *kgsl_debugfs_dir;
static struct dentry *proc_d_debugfs;

static int _strict_set(void *data, u64 val)
{
	kgsl_sharedmem_set_noretry(val ? true : false);
	return 0;
}

static int _strict_get(void *data, u64 *val)
{
	*val = kgsl_sharedmem_get_noretry();
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(_strict_fops, _strict_get, _strict_set, "%llu\n");

static void kgsl_qdss_gfx_register_probe(struct kgsl_device *device)
{
	struct resource *res;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
							"qdss_gfx");

	if (res == NULL)
		return;

	device->qdss_gfx_virt = devm_ioremap(device->dev, res->start,
							resource_size(res));

	if (device->qdss_gfx_virt == NULL)
		dev_warn(device->dev, "qdss_gfx ioremap failed\n");
}

static int _isdb_set(void *data, u64 val)
{
	struct kgsl_device *device = data;

	if (device->qdss_gfx_virt == NULL)
		kgsl_qdss_gfx_register_probe(device);

	device->set_isdb_breakpoint = val ? true : false;
	return 0;
}

static int _isdb_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;

	*val = device->set_isdb_breakpoint ? 1 : 0;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(_isdb_fops, _isdb_get, _isdb_set, "%llu\n");

void kgsl_device_debugfs_init(struct kgsl_device *device)
{
	struct dentry *snapshot_dir;

	if (IS_ERR_OR_NULL(kgsl_debugfs_dir))
		return;

	device->d_debugfs = debugfs_create_dir(device->name,
						       kgsl_debugfs_dir);

	snapshot_dir = debugfs_create_dir("snapshot", kgsl_debugfs_dir);
	debugfs_create_file("break_isdb", 0644, snapshot_dir, device,
		&_isdb_fops);
}

void kgsl_device_debugfs_close(struct kgsl_device *device)
{
	debugfs_remove_recursive(device->d_debugfs);
}

/**
 * kgsl_process_init_debugfs() - Initialize debugfs for a process
 * @private: Pointer to process private structure created for the process
 *
 * kgsl_process_init_debugfs() is called at the time of creating the
 * process struct when a process opens kgsl device for the first time.
 * This function is not fatal - all we do is print a warning message if
 * the files can't be created
 */
void kgsl_process_init_debugfs(struct kgsl_process_private *private)
{
	unsigned char name[16];

	snprintf(name, sizeof(name), "%d", pid_nr(private->pid));

	private->debug_root = debugfs_create_dir(name, proc_d_debugfs);

	/*
	 * Both debugfs_create_dir() and debugfs_create_file() return
	 * ERR_PTR(-ENODEV) if debugfs is disabled in the kernel but return
	 * NULL on error when it is enabled. For both usages we need to check
	 * for ERROR or NULL and only print a warning on an actual failure
	 * (i.e. - when the return value is NULL)
	 */

	if (IS_ERR_OR_NULL(private->debug_root)) {
		WARN((private->debug_root == NULL),
			"Unable to create debugfs dir for %s\n", name);
		private->debug_root = NULL;
		return;
	}
}

void kgsl_core_debugfs_init(void)
{
	struct dentry *debug_dir;

	kgsl_debugfs_dir = debugfs_create_dir("kgsl", NULL);
	if (IS_ERR_OR_NULL(kgsl_debugfs_dir))
		return;

	debug_dir = debugfs_create_dir("debug", kgsl_debugfs_dir);

	debugfs_create_file("strict_memory", 0644, debug_dir, NULL,
		&_strict_fops);

	proc_d_debugfs = debugfs_create_dir("proc", kgsl_debugfs_dir);
}

void kgsl_core_debugfs_close(void)
{
	debugfs_remove_recursive(kgsl_debugfs_dir);
}
