// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2008-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/io.h>

#include "kgsl_debugfs.h"
#include "kgsl_device.h"
#include "kgsl_pool.h"
#include "kgsl_sharedmem.h"

struct dentry *kgsl_debugfs_dir, *mempools_debugfs;
static struct dentry *proc_d_debugfs;

static void kgsl_qdss_gfx_register_probe(struct kgsl_device *device)
{
	struct resource *res;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
							"qdss_gfx");

	if (res == NULL)
		return;

	device->qdss_gfx_virt = devm_ioremap(&device->pdev->dev, res->start,
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

static int globals_show(struct seq_file *s, void *unused)
{
	struct kgsl_device *device = s->private;
	struct kgsl_global_memdesc *md;

	list_for_each_entry(md, &device->globals, node) {
		struct kgsl_memdesc *memdesc = &md->memdesc;
		char flags[6];

		flags[0] = memdesc->priv & KGSL_MEMDESC_PRIVILEGED ?  'p' : '-';
		flags[1] = !(memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 'w' : '-';
		flags[2] = kgsl_memdesc_is_secured(memdesc) ?  's' : '-';
		flags[3] = memdesc->priv & KGSL_MEMDESC_RANDOM ?  'r' : '-';
		flags[4] = memdesc->priv & KGSL_MEMDESC_UCODE ? 'u' : '-';
		flags[5] = '\0';

		seq_printf(s, "0x%pK-0x%pK %16llu %5s %s\n",
			(u64 *)(uintptr_t) memdesc->gpuaddr,
			(u64 *)(uintptr_t) (memdesc->gpuaddr +
			memdesc->size - 1), memdesc->size, flags,
			md->name);
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(globals);

static int _pool_size_get(void *data, u64 *val)
{
	*val = (u64) kgsl_pool_size_total();
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(_pool_size_fops, _pool_size_get, NULL, "%llu\n");

DEFINE_DEBUGFS_ATTRIBUTE(_reserved_fops,
					kgsl_pool_reserved_get, NULL, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(_page_count_fops,
					kgsl_pool_page_count_get, NULL, "%llu\n");

void kgsl_pool_init_debugfs(struct dentry *pool_debugfs,
					char *name, void *pool)
{
	struct dentry *dentry;

	pool_debugfs = debugfs_create_dir(name, mempools_debugfs);

	if (IS_ERR_OR_NULL(pool_debugfs)) {
		WARN((pool_debugfs == NULL),
			"Unable to create debugfs dir for %s\n", name);
		pool_debugfs = NULL;
		return;
	}

	dentry = debugfs_create_file("reserved", 0444,
		pool_debugfs, pool, &_reserved_fops);

	WARN((IS_ERR_OR_NULL(dentry)),
		"Unable to create 'reserved' file for %s\n", name);

	dentry = debugfs_create_file("count", 0444,
		pool_debugfs, pool, &_page_count_fops);

	WARN((IS_ERR_OR_NULL(dentry)),
		"Unable to create 'count' file for %s\n", name);
}

void kgsl_device_debugfs_init(struct kgsl_device *device)
{
	struct dentry *snapshot_dir;

	if (IS_ERR_OR_NULL(kgsl_debugfs_dir))
		return;

	device->d_debugfs = debugfs_create_dir(device->name,
						       kgsl_debugfs_dir);

	debugfs_create_file("globals", 0444, device->d_debugfs, device,
		&globals_fops);

	snapshot_dir = debugfs_create_dir("snapshot", kgsl_debugfs_dir);
	debugfs_create_file("break_isdb", 0644, snapshot_dir, device,
		&_isdb_fops);
}

void kgsl_device_debugfs_close(struct kgsl_device *device)
{
	debugfs_remove_recursive(device->d_debugfs);
}

static const char *memtype_str(int memtype)
{
	if (memtype == KGSL_MEM_ENTRY_KERNEL)
		return "gpumem";
	else if (memtype == KGSL_MEM_ENTRY_USER)
		return "usermem";
	else if (memtype == KGSL_MEM_ENTRY_ION)
		return "ion";

	return "unknown";
}

static char get_alignflag(const struct kgsl_memdesc *m)
{
	int align = kgsl_memdesc_get_align(m);

	if (align >= ilog2(SZ_1M))
		return 'L';
	else if (align >= ilog2(SZ_64K))
		return 'l';
	return '-';
}

static char get_cacheflag(const struct kgsl_memdesc *m)
{
	static const char table[] = {
		[KGSL_CACHEMODE_WRITECOMBINE] = '-',
		[KGSL_CACHEMODE_UNCACHED] = 'u',
		[KGSL_CACHEMODE_WRITEBACK] = 'b',
		[KGSL_CACHEMODE_WRITETHROUGH] = 't',
	};

	return table[kgsl_memdesc_get_cachemode(m)];
}

static int print_mem_entry(void *data, void *ptr)
{
	struct seq_file *s = data;
	struct kgsl_mem_entry *entry = ptr;
	char flags[11];
	char usage[16];
	struct kgsl_memdesc *m = &entry->memdesc;
	unsigned int usermem_type = kgsl_memdesc_usermem_type(m);
	int egl_surface_count = 0, egl_image_count = 0, total_count = 0;
	unsigned long inode_number = 0;
	u32 map_count = atomic_read(&entry->map_count);

	flags[0] = kgsl_memdesc_is_global(m) ?  'g' : '-';
	flags[1] = '-';
	flags[2] = !(m->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 'w' : '-';
	flags[3] = get_alignflag(m);
	flags[4] = get_cacheflag(m);
	flags[5] = kgsl_memdesc_use_cpu_map(m) ? 'p' : '-';
	/* Show Y if at least one vma has this entry mapped (could be multiple) */
	flags[6] = map_count ? 'Y' : 'N';
	flags[7] = kgsl_memdesc_is_secured(m) ?  's' : '-';
	flags[8] = '-';
	flags[9] = m->flags & KGSL_MEMFLAGS_VBO ? 'v' : '-';
	flags[10] = '\0';

	kgsl_get_memory_usage(usage, sizeof(usage), m->flags);

	if (usermem_type == KGSL_MEM_ENTRY_ION) {
		kgsl_get_egl_counts(entry, &egl_surface_count,
						&egl_image_count, &total_count);
		inode_number = kgsl_get_dmabuf_inode_number(entry);
	}

	seq_printf(s, "%pK %pK %16llu %5d %10s %10s %16s %5d %10d %6d %6d %10lu",
			(uint64_t *)(uintptr_t) m->gpuaddr,
			/*
			 * Show zero for the useraddr - we can't reliably track
			 * that value for multiple vmas anyway
			 */
			NULL, m->size, entry->id, flags,
			memtype_str(usermem_type),
			usage, (m->sgt ? m->sgt->nents : 0), map_count,
			egl_surface_count, egl_image_count, inode_number);

	if (entry->metadata[0] != 0)
		seq_printf(s, " %s", entry->metadata);

	seq_putc(s, '\n');

	return 0;
}

static struct kgsl_mem_entry *process_mem_seq_find(struct seq_file *s,
						void *ptr, loff_t pos)
{
	struct kgsl_mem_entry *entry = ptr;
	struct kgsl_process_private *private = s->private;
	int id = 0;

	loff_t temp_pos = 1;

	if (entry != SEQ_START_TOKEN)
		id = entry->id + 1;

	spin_lock(&private->mem_lock);
	for (entry = idr_get_next(&private->mem_idr, &id); entry;
		id++, entry = idr_get_next(&private->mem_idr, &id),
							temp_pos++) {
		if (temp_pos == pos && kgsl_mem_entry_get(entry)) {
			spin_unlock(&private->mem_lock);
			goto found;
		}
	}
	spin_unlock(&private->mem_lock);

	entry = NULL;
found:
	if (ptr != SEQ_START_TOKEN)
		kgsl_mem_entry_put(ptr);

	return entry;
}

static void *process_mem_seq_start(struct seq_file *s, loff_t *pos)
{
	loff_t seq_file_offset = *pos;

	if (seq_file_offset == 0)
		return SEQ_START_TOKEN;
	else
		return process_mem_seq_find(s, SEQ_START_TOKEN,
						seq_file_offset);
}

static void process_mem_seq_stop(struct seq_file *s, void *ptr)
{
	if (ptr && ptr != SEQ_START_TOKEN)
		kgsl_mem_entry_put(ptr);
}

static void *process_mem_seq_next(struct seq_file *s, void *ptr,
							loff_t *pos)
{
	++*pos;
	return process_mem_seq_find(s, ptr, 1);
}

static int process_mem_seq_show(struct seq_file *s, void *ptr)
{
	if (ptr == SEQ_START_TOKEN) {
		seq_printf(s, "%16s %16s %16s %5s %10s %10s %16s %5s %10s %6s %6s %10s\n",
			"gpuaddr", "useraddr", "size", "id", "flags", "type",
			"usage", "sglen", "mapcnt", "eglsrf", "eglimg", "inode");
		return 0;
	} else
		return print_mem_entry(s, ptr);
}

static const struct seq_operations process_mem_seq_fops = {
	.start = process_mem_seq_start,
	.stop = process_mem_seq_stop,
	.next = process_mem_seq_next,
	.show = process_mem_seq_show,
};

static int process_mem_open(struct inode *inode, struct file *file)
{
	int ret;
	pid_t pid = (pid_t) (unsigned long) inode->i_private;
	struct seq_file *s = NULL;
	struct kgsl_process_private *private = NULL;

	private = kgsl_process_private_find(pid);

	if (!private)
		return -ENODEV;

	ret = seq_open(file, &process_mem_seq_fops);
	if (ret)
		kgsl_process_private_put(private);
	else {
		s = file->private_data;
		s->private = private;
	}

	return ret;
}

static int process_mem_release(struct inode *inode, struct file *file)
{
	struct kgsl_process_private *private =
		((struct seq_file *)file->private_data)->private;

	if (private)
		kgsl_process_private_put(private);

	return seq_release(inode, file);
}

static const struct file_operations process_mem_fops = {
	.open = process_mem_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = process_mem_release,
};


static int print_vbo_ranges(int id, void *ptr, void *data)
{
	kgsl_memdesc_print_vbo_ranges(ptr, data);
	return 0;
}

static int vbo_print(struct seq_file *s, void *unused)
{
	struct kgsl_process_private *private = s->private;

	seq_puts(s, "id    child range\n");

	spin_lock(&private->mem_lock);
	idr_for_each(&private->mem_idr, print_vbo_ranges, s);
	spin_unlock(&private->mem_lock);

	return 0;
}

static int vbo_open(struct inode *inode, struct file *file)
{
	pid_t pid = (pid_t) (unsigned long) inode->i_private;
	struct kgsl_process_private *private;
	int ret;

	private = kgsl_process_private_find(pid);

	if (!private)
		return -ENODEV;

	ret = single_open(file, vbo_print, private);
	if (ret)
		kgsl_process_private_put(private);

	return ret;
}

static const struct file_operations vbo_fops = {
	.open = vbo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	/* Reuse the same release function */
	.release = process_mem_release,
};

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
	struct dentry *dentry;

	snprintf(name, sizeof(name), "%d", pid_nr(private->pid));

	private->debug_root = debugfs_create_dir(name, proc_d_debugfs);

	if (IS_ERR(private->debug_root)) {
		WARN_ONCE("Unable to create debugfs dir for %s\n", name);
		private->debug_root = NULL;
		return;
	}

	dentry = debugfs_create_file("mem", 0444, private->debug_root,
		(void *) ((unsigned long) pid_nr(private->pid)), &process_mem_fops);

	if (IS_ERR(dentry))
		WARN_ONCE("Unable to create 'mem' file for %s\n", name);

	debugfs_create_file("vbos", 0444, private->debug_root,
		(void *) ((unsigned long) pid_nr(private->pid)), &vbo_fops);
}

void kgsl_core_debugfs_init(void)
{
	struct dentry *debug_dir, *dentry;

	kgsl_debugfs_dir = debugfs_create_dir("kgsl", NULL);
	if (IS_ERR_OR_NULL(kgsl_debugfs_dir))
		return;

	debug_dir = debugfs_create_dir("debug", kgsl_debugfs_dir);

	proc_d_debugfs = debugfs_create_dir("proc", kgsl_debugfs_dir);

	debugfs_create_bool("strict_memory", 0644, debug_dir,
		&kgsl_sharedmem_noretry_flag);

	mempools_debugfs = debugfs_create_dir("mempools", kgsl_debugfs_dir);

	if (IS_ERR_OR_NULL(mempools_debugfs))
		return;

	dentry = debugfs_create_file("pool_size", 0444,
		mempools_debugfs, NULL, &_pool_size_fops);

	WARN((IS_ERR_OR_NULL(dentry)),
		"Unable to create 'pool_size' file for mempools\n");
}

void kgsl_core_debugfs_close(void)
{
	debugfs_remove_recursive(kgsl_debugfs_dir);
}
