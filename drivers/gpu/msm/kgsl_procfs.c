// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2008-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/ptrace.h>

#include "kgsl_procfs.h"
#include "kgsl_device.h"
#include "kgsl_lazy.h"
#include "kgsl_sharedmem.h"

static struct proc_dir_entry *kgsl_procfs_dir;
static struct proc_dir_entry *proc_d_procfs;


void kgsl_core_procfs_init(void)
{
	kgsl_procfs_dir = proc_mkdir("kgsl", NULL);
	if (!kgsl_procfs_dir)
		return;

	proc_d_procfs = proc_mkdir("proc", kgsl_procfs_dir);
}

void kgsl_core_procfs_close(void)
{
	proc_remove(kgsl_procfs_dir);
}

struct type_entry {
	int type;
	const char *str;
};

static const struct type_entry memtypes[] = { KGSL_MEM_TYPES };

static const char *memtype_str(int memtype)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(memtypes); i++)
		if (memtypes[i].type == memtype)
			return memtypes[i].str;
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
	char flags[10];
	char usage[16];
	struct kgsl_memdesc *m = &entry->memdesc;
	unsigned int usermem_type = kgsl_memdesc_usermem_type(m);
	int egl_surface_count = 0, egl_image_count = 0, total_count = 0;

	flags[0] = kgsl_memdesc_is_global(m) ?  'g' :
			(m->priv & KGSL_MEMDESC_USE_SHMEM) ? 'S' : '-';
	flags[1] = (m->priv & KGSL_MEMDESC_LAZY_ALLOCATION) ? 'z' : '-';
	flags[2] = !(m->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 'w' : '-';
	flags[3] = get_alignflag(m);
	flags[4] = get_cacheflag(m);
	flags[5] = kgsl_memdesc_use_cpu_map(m) ? 'p' : '-';
	/*
	 * Show Y if at least one vma has this entry
	 * mapped (could be multiple)
	 */
	flags[6] = atomic_read(&entry->map_count) ? 'Y' : 'N';
	flags[7] = kgsl_memdesc_is_secured(m) ?  's' : '-';
	flags[8] = kgsl_memdesc_is_reclaimed(m) ? 'R' : '-';
	flags[9] = '\0';

	kgsl_get_memory_usage(usage, sizeof(usage), m->flags);

	if (usermem_type == KGSL_MEM_ENTRY_ION)
		kgsl_get_egl_counts(entry, &egl_surface_count,
						&egl_image_count, &total_count);

	seq_printf(s, "%pK %16llu %16llu %16llu %5d %9s %10s %16s %5d %16d %6d %6d",
			(uint64_t *)(uintptr_t) m->gpuaddr,
			m->size, kgsl_memdesc_get_physsize(m),
			kgsl_memdesc_get_swapsize(m), entry->id, flags,
			memtype_str(usermem_type), usage,
			(m->sgt ? m->sgt->nents : 0),
			kgsl_memdesc_get_mapsize(m),
			egl_surface_count, egl_image_count);

#if IS_ENABLED(CONFIG_QCOM_KGSL_ENTRY_METADATA)
	if (entry->metadata)
		seq_printf(s, " %s", entry->metadata);
#endif

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
		seq_printf(s, "%16s %16s %16s %16s %5s %9s %10s %16s %5s %16s %6s %6s\n",
			"gpuaddr", "virtsize", "physsize", "swapsize", "id",
			"flags", "type", "usage", "sglen", "mapsize", "eglsrf",
			"eglimg");
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
	pid_t pid = (pid_t) (unsigned long) PDE_DATA(inode);
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

static int print_mem_entry_page(struct seq_file *s, void *ptr)
{
	struct kgsl_mem_entry *entry = s->private;
	struct page *page;
	const size_t page_index = *(loff_t *)ptr - 1;
	const size_t offset = page_index << PAGE_SHIFT;
	void *kptr, *buf;

	const size_t rowsize = 32;
	size_t linelen = rowsize;
	size_t remaining = PAGE_SIZE;
	size_t i;

	unsigned char linebuf[32 * 3 + 2 + 32 + 1];

	/* Skip unallocated pages. */
	page = kgsl_mmu_find_mapped_page(&entry->memdesc, offset);
	if (IS_ERR_OR_NULL(page))
		return 0;

	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	kptr = vm_map_ram(&page, 1, -1, kgsl_pgprot_modify(&entry->memdesc,
			PAGE_KERNEL_RO));
	if (!kptr)
		goto no_mapping;

	memcpy(buf, kptr, PAGE_SIZE);
	vm_unmap_ram(kptr, 1);

	for (i = 0; i < PAGE_SIZE; i += rowsize) {
		const size_t page_offset = offset + i + (kptr_restrict < 2 ?
				entry->memdesc.gpuaddr : 0);

		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(buf + i, linelen, rowsize, 4,
				linebuf, sizeof(linebuf), true);

		seq_printf(s, "%016llx: %s\n", page_offset, linebuf);
	}

no_mapping:
	kfree(buf);

	return 0;
}

static void *mem_entry_seq_start(struct seq_file *s, loff_t *pos)
{
	struct kgsl_mem_entry *entry = s->private;

	if (*pos == 0)
		return SEQ_START_TOKEN;
	else
		return (*pos <= (entry->memdesc.size >> PAGE_SHIFT)) ? pos : NULL;
}

static void *mem_entry_seq_next(struct seq_file *s, void *ptr, loff_t *pos)
{
	struct kgsl_mem_entry *entry = s->private;

	++(*pos);
	return (*pos <= (entry->memdesc.size >> PAGE_SHIFT)) ? pos : NULL;
}

static int mem_entry_seq_show(struct seq_file *s, void *ptr)
{
	if (ptr == SEQ_START_TOKEN)
		return print_mem_entry(s, s->private);
	else
		return print_mem_entry_page(s, ptr);
}

static void mem_entry_seq_stop(struct seq_file *s, void *ptr)
{
}

static const struct seq_operations mem_entry_seq_ops = {
	.start = mem_entry_seq_start,
	.next = mem_entry_seq_next,
	.show = mem_entry_seq_show,
	.stop = mem_entry_seq_stop,
};

static int mem_entry_open(struct inode *inode, struct file *file)
{
	struct kgsl_process_private *private;
	struct seq_file *s;
	struct task_struct *task;
	pid_t pid = (pid_t) (unsigned long) PDE_DATA(inode);
	int ret = -ENODEV;
	bool may_access;

	private = kgsl_process_private_find(pid);
	if (!private)
		return ret;

	/*
	 * First check if the dump request is coming from the process itself
	 * or from a privileged process (like root).
	 */
	task = get_pid_task(private->pid, PIDTYPE_PID);
	if (!task) {
		ret = -ENODEV;
		goto err;
	}

	may_access = ptrace_may_access(task, PTRACE_MODE_READ_FSCREDS);
	put_task_struct(task);
	if (!may_access) {
		ret = -EACCES;
		goto err;
	}

	if ((file->f_mode & (FMODE_READ | FMODE_WRITE)) ==
			(FMODE_READ | FMODE_WRITE)) {
		ret = -EINVAL;
		goto err;
	}

	ret = seq_open(file, &mem_entry_seq_ops);
	if (ret < 0)
		goto err;

	s = file->private_data;

	if (file->f_mode & FMODE_WRITE)
		s->private = private;
	else if (file->f_mode & FMODE_READ) {
		struct kgsl_mem_entry *entry;
		const uint64_t blocked_flags = KGSL_MEMFLAGS_USERMEM_MASK |
			KGSL_MEMFLAGS_SECURE;

		entry = kgsl_sharedmem_find_id(private, private->dump_id);
		if (IS_ERR_OR_NULL(entry)) {
			ret = -ENOENT;
			goto err;
		}

		if (!(entry->memdesc.priv & KGSL_MEMDESC_MAPPED) ||
				(entry->memdesc.flags & blocked_flags)) {
			kgsl_mem_entry_put(entry);
			ret = -EINVAL;
			goto err;
		}

		s->private = entry;
	}

	return ret;

err:
	kgsl_process_private_put(private);
	return ret;
}

static ssize_t mem_entry_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct kgsl_process_private *private = s->private;
	long result, ret;

	ret = kstrtol_from_user(ubuf, count, 10, &result);
	if (ret)
		return ret;

	private->dump_id = result;

	return count;
}

static int mem_entry_release(struct inode *inode, struct file *file)
{
	struct seq_file *s = file->private_data;
	struct kgsl_process_private *private = NULL;

	if (file->f_mode & FMODE_WRITE)
		private = s->private;
	else if (file->f_mode & FMODE_READ) {
		struct kgsl_mem_entry *entry = s->private;

		private = entry->priv;
		kgsl_mem_entry_put(entry);
	}

	if (private)
		kgsl_process_private_put(private);

	return seq_release(inode, file);
}

static const struct file_operations mem_entry_fops = {
	.open = mem_entry_open,
	.read = seq_read,
	.write = mem_entry_write,
	.llseek = seq_lseek,
	.release = mem_entry_release,
};

/**
 * kgsl_process_init_procfs() - Initialize procfs for a process
 * @private: Pointer to process private structure created for the process
 *
 * kgsl_process_init_procfs() is called at the time of creating the
 * process struct when a process opens kgsl device for the first time.
 * This function is not fatal - all we do is print a warning message if
 * the files can't be created
 */
void kgsl_process_init_procfs(struct kgsl_process_private *private)
{
	unsigned char name[16];
	struct proc_dir_entry *dentry;

	snprintf(name, sizeof(name), "%d", pid_nr(private->pid));

	private->proc_root = proc_mkdir(name, proc_d_procfs);

	if (!private->proc_root) {
		WARN(1, "Unable to create procfs dir for %s\n", name);
		return;
	}

	dentry = proc_create_data("mem", 0444, private->proc_root,
		&process_mem_fops, (void *) ((unsigned long) pid_nr(private->pid)));

	WARN((dentry == NULL),
		"Unable to create 'mem' file for %s\n", name);

	dentry = proc_create_data("dump", 0666, private->proc_root,
		&mem_entry_fops, (void *) ((unsigned long) pid_nr(private->pid)));

	WARN((dentry == NULL),
		"Unable to create 'dump' file for %s\n", name);
}

static int lazy_gpumem_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, PDE_DATA(inode));
}

static ssize_t lazy_gpumem_enable_read(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct seq_file *sfile = file->private_data;
	struct kgsl_device *device = sfile->private;
	char buf[2];

	buf[0] = kgsl_lazy_procfs_is_process_lazy(device) ? '1' : '0';
	buf[1] = '\n';

	return simple_read_from_buffer(buffer, count, ppos, buf, 2);
}

static ssize_t lazy_gpumem_enable_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *ppos)
{
	struct seq_file *sfile;
	struct kgsl_device *device;
	int val, ret;

	ret = kstrtoint_from_user(buffer, count, 0, &val);
	if (ret)
		return ret;
	if (val != 0 && val != 1)
		return -EINVAL;

	sfile = file->private_data;
	device = sfile->private;

	ret = kgsl_lazy_procfs_process_enable(device, val);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations lazy_gpumem_enable_fops = {
	.open		= lazy_gpumem_enable_open,
	.read		= lazy_gpumem_enable_read,
	.write		= lazy_gpumem_enable_write,
	.llseek		= no_llseek,
	.release	= single_release,
};

static int globals_print(struct seq_file *s, void *unused)
{
	struct kgsl_device *device = s->private;
	struct kgsl_global_memdesc *md;

	seq_printf(s, "%37s %16s %16s %5s %s\n",
			"gpuaddr", "virtsize", "physsize", "flags", "name");

	list_for_each_entry(md, &device->globals, node) {
		struct kgsl_memdesc *memdesc = &md->memdesc;
		char flags[6];

		flags[0] = memdesc->priv & KGSL_MEMDESC_PRIVILEGED ?  'p' : '-';
		flags[1] = !(memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 'w' : '-';
		flags[2] = kgsl_memdesc_is_secured(memdesc) ?  's' : '-';
		flags[3] = memdesc->priv & KGSL_MEMDESC_RANDOM ?  'r' : '-';
		flags[4] = memdesc->priv & KGSL_MEMDESC_UCODE ? 'u' : '-';
		flags[5] = '\0';

		seq_printf(s, "0x%pK-0x%pK %16llu %16llu %5s %s\n",
			(u64 *)(uintptr_t) memdesc->gpuaddr,
			(u64 *)(uintptr_t) (memdesc->gpuaddr +
			memdesc->size - 1), memdesc->size,
			atomic_long_read(&memdesc->physsize), flags, md->name);
	}

	return 0;
}

static int globals_open(struct inode *inode, struct file *file)
{
	return single_open(file, globals_print, PDE_DATA(inode));
}

static const struct file_operations global_fops = {
	.open = globals_open,
	.read = seq_read,
	.llseek = no_llseek,
	.release = single_release,
};

void kgsl_procfs_init(struct kgsl_device *device)
{
	struct proc_dir_entry *dentry;

	dentry = proc_create_data("lazy_gpumem_enable", 0666, kgsl_procfs_dir,
			&lazy_gpumem_enable_fops, device);
	WARN(!dentry, "Unable to create 'lazy_gpumem_enable' file\n");

	dentry = proc_create_data("globals", 0444, kgsl_procfs_dir,
			&global_fops, device);
	WARN(!dentry, "Unable to create 'globals' file\n");
}
