// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-direct.h>
#include <linux/of_platform.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/shmem_fs.h>

#include "kgsl_device.h"
#include "kgsl_lazy.h"
#include "kgsl_pool.h"
#include "kgsl_reclaim.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"

#define PSS_SHIFT 12

/*
 * The user can set this from debugfs to force failed memory allocations to
 * fail without trying OOM first.  This is a debug setting useful for
 * stress applications that want to test failure cases without pushing the
 * system into unrecoverable OOM panics
 */

static bool sharedmem_noretry_flag;

/* An attribute for showing per-process memory statistics */
struct kgsl_mem_entry_attribute {
	struct kgsl_process_attribute attr;
	int memtype;
	ssize_t (*show)(struct kgsl_process_private *priv,
		int type, char *buf);
};

static inline struct kgsl_process_attribute *to_process_attr(
		struct attribute *attr)
{
	return container_of(attr, struct kgsl_process_attribute, attr);
}

#define to_mem_entry_attr(a) \
container_of(a, struct kgsl_mem_entry_attribute, attr)

#define __MEM_ENTRY_ATTR(_type, _name, _show) \
{ \
	.attr = __ATTR(_name, 0444, mem_entry_sysfs_show, NULL), \
	.memtype = _type, \
	.show = _show, \
}

static ssize_t mem_entry_sysfs_show(struct kobject *kobj,
	struct kgsl_process_attribute *attr, char *buf)
{
	struct kgsl_mem_entry_attribute *pattr = to_mem_entry_attr(attr);
	struct kgsl_process_private *priv =
		container_of(kobj, struct kgsl_process_private, kobj);

	return pattr->show(priv, pattr->memtype, buf);
}

/*
 * A structure to hold the attributes for a particular memory type.
 * For each memory type in each process we store the current and maximum
 * memory usage and display the counts in sysfs.  This structure and
 * the following macro allow us to simplify the definition for those
 * adding new memory types
 */

struct mem_entry_stats {
	int memtype;
	struct kgsl_mem_entry_attribute attr;
	struct kgsl_mem_entry_attribute max_attr;
};


#define MEM_ENTRY_STAT(_type, _name) \
{ \
	.memtype = _type, \
	.attr = __MEM_ENTRY_ATTR(_type, _name, mem_entry_show), \
	.max_attr = __MEM_ENTRY_ATTR(_type, _name##_max, \
		mem_entry_max_show), \
}

static uint64_t imported_mem_account(struct kgsl_process_private *priv,
					struct kgsl_mem_entry *entry)
{
	struct kgsl_memdesc *m = &entry->memdesc;
	unsigned int mt = kgsl_memdesc_get_memtype(m);
	int egl_surface_count = 0, egl_image_count = 0, total_count = 0;
	uint64_t sz = m->size;

	kgsl_get_egl_counts(entry, &egl_surface_count, &egl_image_count,
			&total_count);

	if (mt == KGSL_MEMTYPE_EGL_SURFACE) {
		do_div(sz, egl_surface_count ?: 1);
		return sz;
	}

	if (egl_surface_count == 0) {
		if (total_count == egl_image_count || total_count <= 1) {
			do_div(sz, total_count ?: 1);
		} else if (mt == KGSL_MEMTYPE_OBJECTANY && total_count >= 2) {
			/* compositor services will typically allocate, import,
			 * then distribute buffers to clients. with these
			 * services, we would prefer to attribute all memory
			 * accounting to clients, and none to the server,
			 * matching the EGL_SURFACE/EGL_IMAGE distinction for
			 * EGL-bound buffers.
			 */
			struct kgsl_process_private *allocator =
					kgsl_find_dmabuf_allocator(entry);
			if (allocator == priv)
				return 0;

			do_div(sz, (total_count - 1));
		} else {
			do_div(sz, total_count ?: 1);
		}
		return sz;
	}

	return 0;
}

static ssize_t
imported_mem_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	struct kgsl_mem_entry *entry;
	uint64_t imported_mem = 0;
	int id = 0;

	rcu_read_lock();
	idr_for_each_entry_continue(&priv->mem_idr, entry, id) {
		struct kgsl_memdesc *memdesc = &entry->memdesc;

		if (kgsl_memdesc_usermem_type(memdesc) != KGSL_MEM_ENTRY_ION ||
				kgsl_mem_entry_get(entry) == 0)
			continue;

		imported_mem += imported_mem_account(priv, entry);

		/*
		 * If refcount on mem entry is the last refcount, we will
		 * call kgsl_mem_entry_destroy and detach it from process
		 * list. When there is no refcount on the process private,
		 * we will call kgsl_destroy_process_private to do cleanup.
		 * During cleanup, we will try to remove the same sysfs
		 * node which is in use by the current thread and this
		 * situation will end up in a deadloack.
		 * To avoid this situation, use a worker to put the refcount
		 * on mem entry.
		 */
		kgsl_mem_entry_put_deferred(entry);
	}
	rcu_read_unlock();

	return scnprintf(buf, PAGE_SIZE, "%llu\n", imported_mem);
}

uint64_t kgsl_memdesc_get_physsize(struct kgsl_memdesc *memdesc)
{
	struct shmem_inode_info *shmem_info;
	unsigned long flags;
	uint64_t physsize;

	if (!(memdesc->priv & KGSL_MEMDESC_USE_SHMEM))
		return atomic_long_read(&memdesc->physsize);

	BUG_ON(!memdesc->shmem_filp);

	shmem_info = SHMEM_I(memdesc->shmem_filp->f_mapping->host);
	spin_lock_irqsave(&shmem_info->lock, flags);
	physsize = (shmem_info->alloced - shmem_info->swapped) << PAGE_SHIFT;
	spin_unlock_irqrestore(&shmem_info->lock, flags);

	return physsize;
}

uint64_t kgsl_memdesc_get_swapsize(struct kgsl_memdesc *memdesc)
{
	struct shmem_inode_info *shmem_info;
	unsigned long flags;
	uint64_t swapsize;

	if (!(memdesc->priv & KGSL_MEMDESC_USE_SHMEM))
		return 0;

	BUG_ON(!memdesc->shmem_filp);

	shmem_info = SHMEM_I(memdesc->shmem_filp->f_mapping->host);
	spin_lock_irqsave(&shmem_info->lock, flags);
	swapsize = shmem_info->swapped << PAGE_SHIFT;
	spin_unlock_irqrestore(&shmem_info->lock, flags);

	return swapsize;
}

/* This function must be called under rcu_read_lock! */
static uint64_t __get_shmem_mapsize(struct kgsl_memdesc *memdesc)
{
	struct address_space *mapping = memdesc->shmem_filp->f_mapping;
	struct radix_tree_iter iter;
	void __rcu **slot;
	uint64_t mapsize = 0;

	radix_tree_for_each_slot(slot, &mapping->i_pages, &iter, 0) {
		struct page *page = radix_tree_deref_slot(slot);
		uint64_t page_pss = (uint64_t)PAGE_SIZE << PSS_SHIFT;
		size_t mapcount;

		if (radix_tree_deref_retry(page)) {
			slot = radix_tree_iter_retry(&iter);
			continue;
		}

		if (radix_tree_exceptional_entry(page))
			continue;

		mapcount = page_mapcount(page);
		if (!mapcount)
			continue;
		else if (unlikely(mapcount >= 2))
			do_div(page_pss, mapcount);

		mapsize += page_pss;

		if (need_resched()) {
			slot = radix_tree_iter_resume(slot, &iter);
			cond_resched_rcu();
		}
	}

	return mapsize >> PSS_SHIFT;
}

uint64_t kgsl_memdesc_get_mapsize(struct kgsl_memdesc *memdesc)
{
	uint64_t mapsize;

	if (!(memdesc->priv & KGSL_MEMDESC_USE_SHMEM))
		mapsize = atomic_long_read(&memdesc->mapsize);
	else {
		rcu_read_lock();
		mapsize = __get_shmem_mapsize(memdesc);
		rcu_read_unlock();
	}

	return mapsize;
}

static void gpumem_account(struct kgsl_process_private *priv, int type,
		int64_t *dedicated_mapped, int64_t *dedicated_unmapped,
		int64_t *system_mapped, int64_t *system_unmapped)
{
	struct kgsl_mem_entry *entry;
	int64_t gpumem_dedicated_mapped = 0;
	int64_t gpumem_dedicated_unmapped = 0;
	int64_t gpumem_system_mapped = 0;
	int64_t gpumem_system_unmapped = 0;
	int id = 0;

	rcu_read_lock();
	idr_for_each_entry_continue(&priv->mem_idr, entry, id) {
		struct kgsl_memdesc *m = &entry->memdesc;
		struct page *page, *tmp;
		LIST_HEAD(page_list);
		int64_t entry_mapped = 0;
		int64_t entry_unmapped;
		int entry_mapcount;
		int ret;

		/* Relinquish the RCU read lock if this thread needs resched. */
		if (need_resched())
			cond_resched_rcu();

		if (atomic_read(&entry->pending_free) ||
				kgsl_memdesc_usermem_type(m) != type)
			continue;

		entry_mapcount = atomic_read(&entry->map_count);

		/*
		 * If this entry is shmem-backed then we need to handle the
		 * mapped size for it in a special way.
		 */
		if (m->priv & KGSL_MEMDESC_USE_SHMEM) {
			if (entry_mapcount > 0) {
				entry_mapped = __get_shmem_mapsize(m);
				gpumem_system_mapped += entry_mapped;
			}

			gpumem_system_unmapped += kgsl_memdesc_get_physsize(m) -
					entry_mapped;
			gpumem_system_mapped += kgsl_memdesc_get_swapsize(m);
			continue;
		}

		entry_mapped = atomic_long_read(&m->mapsize);
		entry_unmapped = atomic_long_read(&m->physsize) - entry_mapped;

		/*
		 * If the entry is not mapped to userspace or only mapped once
		 * then we can fastpath handling this entry's accounting without
		 * needing to grab a reference.
		 */
		if (entry_mapcount <= 1) {
			gpumem_dedicated_unmapped += entry_unmapped;
			gpumem_dedicated_mapped += entry_mapped;
			continue;
		}

		/* No such luck. Try and grab a reference to the entry. */
		if (kgsl_mem_entry_get(entry) == 0)
			continue;

		/* Exit the RCU read lock since the code below can sleep. */
		rcu_read_unlock();

		/*
		 * Multiple VMAs cover this entry. Walk the backing pages and
		 * check their individual mapcount when counting PSS to match
		 * proc/smaps behavior.
		 */
		ret = kgsl_mmu_get_backing_pages(m, &page_list);
		if (ret) {
			kgsl_mmu_release_page_list(m, &page_list);

			kgsl_mem_entry_put_deferred(entry);
			rcu_read_lock();
			continue;
		}

		list_for_each_entry_safe(page, tmp, &page_list, lru) {
			size_t compound_size, mapcount;

			/*
			 * We only need to worry about pages that have been
			 * mapped into more than one VMA since mapsize already
			 * accounts for the first userspace mapping.
			 */
			mapcount = page_mapcount(page);
			if (mapcount <= 1)
				continue;

			compound_size = PAGE_SIZE << compound_order(page);
			entry_mapped += (compound_size / mapcount) - compound_size;
		}

		/* Clean up after ourselves. */
		kgsl_mmu_release_page_list(m, &page_list);

		gpumem_dedicated_unmapped += entry_unmapped;
		gpumem_dedicated_mapped += entry_mapped;

		kgsl_mem_entry_put_deferred(entry);
		rcu_read_lock();
	}
	rcu_read_unlock();

	if (dedicated_mapped)
		*dedicated_mapped = gpumem_dedicated_mapped;
	if (dedicated_unmapped)
		*dedicated_unmapped = gpumem_dedicated_unmapped;
	if (system_mapped)
		*system_mapped = gpumem_system_mapped;
	if (system_unmapped)
		*system_unmapped = gpumem_system_unmapped;
}

static ssize_t
gpumem_account_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	int64_t dedicated_mapped = 0;
	int64_t dedicated_unmapped = 0;
	int64_t system_mapped = 0;
	int64_t system_unmapped = 0;

	gpumem_account(priv, type, &dedicated_mapped, &dedicated_unmapped,
			&system_mapped, &system_unmapped);

	return scnprintf(buf, PAGE_SIZE, "%lld,%lld,%lld,%lld\n",
			dedicated_mapped, dedicated_unmapped,
			system_mapped, system_unmapped);
}

static ssize_t
gpumem_mapped_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	int64_t dedicated_mapped = 0;
	int64_t system_mapped = 0;

	gpumem_account(priv, type, &dedicated_mapped, NULL, &system_mapped,
			NULL);

	return scnprintf(buf, PAGE_SIZE, "%lld\n", dedicated_mapped +
			system_mapped);
}

static ssize_t
gpumem_unmapped_show(struct kgsl_process_private *priv, int type, char *buf)
{
	int64_t dedicated_unmapped = 0;
	int64_t system_unmapped = 0;

	gpumem_account(priv, type, NULL, &dedicated_unmapped, NULL,
			&system_unmapped);

	return scnprintf(buf, PAGE_SIZE, "%lld\n", dedicated_unmapped +
			system_unmapped);
}

static struct kgsl_mem_entry_attribute debug_memstats[] = {
	__MEM_ENTRY_ATTR(0, imported_mem, imported_mem_show),
	__MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_KERNEL, gpumem_account,
				gpumem_account_show),
	__MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_KERNEL, gpumem_mapped,
				gpumem_mapped_show),
	__MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_KERNEL, gpumem_unmapped,
				gpumem_unmapped_show),
};

/**
 * Show the current amount of memory allocated for the given memtype
 */

static ssize_t
mem_entry_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%ld\n",
			atomic_long_read(&priv->stats[type].cur));
}

/**
 * Show the maximum memory allocated for the given memtype through the life of
 * the process
 */

static ssize_t
mem_entry_max_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%llu\n", priv->stats[type].max);
}

static ssize_t process_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_process_attribute *pattr = to_process_attr(attr);

	return pattr->show(kobj, pattr, buf);
}

static ssize_t process_sysfs_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t count)
{
	struct kgsl_process_attribute *pattr = to_process_attr(attr);

	if (pattr->store)
		return pattr->store(kobj, pattr, buf, count);
	return -EIO;
}

/* Dummy release function - we have nothing to do here */
static void process_sysfs_release(struct kobject *kobj)
{
}

static const struct sysfs_ops process_sysfs_ops = {
	.show = process_sysfs_show,
	.store = process_sysfs_store,
};

static struct kobj_type process_ktype = {
	.sysfs_ops = &process_sysfs_ops,
	.release = &process_sysfs_release,
};

static struct mem_entry_stats mem_stats[] = {
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_KERNEL, kernel),
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_USER, user),
#ifdef CONFIG_ION
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_ION, ion),
#endif
};

static struct device_attribute dev_attr_max_reclaim_limit = {
	.attr = { .name = "max_reclaim_limit", .mode = 0644 },
	.show = kgsl_proc_max_reclaim_limit_show,
	.store = kgsl_proc_max_reclaim_limit_store,
};

void
kgsl_process_uninit_sysfs(struct kgsl_process_private *private)
{
	kobject_put(&private->kobj);
}

/**
 * kgsl_process_init_sysfs() - Initialize and create sysfs files for a process
 *
 * @device: Pointer to kgsl device struct
 * @private: Pointer to the structure for the process
 *
 * kgsl_process_init_sysfs() is called at the time of creating the
 * process struct when a process opens the kgsl device for the first time.
 * This function creates the sysfs files for the process.
 */
void kgsl_process_init_sysfs(struct kgsl_device *device,
		struct kgsl_process_private *private)
{
	int i;

	if (kobject_init_and_add(&private->kobj, &process_ktype,
		kgsl_driver.prockobj, "%d", pid_nr(private->pid))) {
		dev_err(device->dev, "Unable to add sysfs for process %d\n",
			pid_nr(private->pid));
		return;
	}

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		int ret;

		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].attr.attr.attr);
		ret |= sysfs_create_file(&private->kobj,
			&mem_stats[i].max_attr.attr.attr);

		if (ret)
			dev_err(device->dev,
				"Unable to create sysfs files for process %d\n",
				pid_nr(private->pid));
	}

	for (i = 0; i < ARRAY_SIZE(debug_memstats); i++) {
		if (sysfs_create_file(&private->kobj,
			&debug_memstats[i].attr.attr))
			WARN(1, "Couldn't create sysfs file '%s'\n",
				debug_memstats[i].attr.attr.name);
	}

	kgsl_reclaim_proc_sysfs_init(private);
}

static ssize_t memstat_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	uint64_t val = 0;

	if (!strcmp(attr->attr.name, "page_alloc"))
		val = atomic_long_read(&kgsl_driver.stats.page_alloc);
	else if (!strcmp(attr->attr.name, "page_alloc_max"))
		val = atomic_long_read(&kgsl_driver.stats.page_alloc_max);
	else if (!strcmp(attr->attr.name, "secure"))
		val = atomic_long_read(&kgsl_driver.stats.secure);
	else if (!strcmp(attr->attr.name, "secure_max"))
		val = atomic_long_read(&kgsl_driver.stats.secure_max);
	else if (!strcmp(attr->attr.name, "mapped"))
		val = atomic_long_read(&kgsl_driver.stats.mapped);
	else if (!strcmp(attr->attr.name, "mapped_max"))
		val = atomic_long_read(&kgsl_driver.stats.mapped_max);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", val);
}

static ssize_t full_cache_threshold_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	unsigned int thresh = 0;

	ret = kgsl_sysfs_store(buf, &thresh);
	if (ret)
		return ret;

	kgsl_driver.full_cache_threshold = thresh;
	return count;
}

static ssize_t full_cache_threshold_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			kgsl_driver.full_cache_threshold);
}

static DEVICE_ATTR(page_alloc, 0444, memstat_show, NULL);
static DEVICE_ATTR(page_alloc_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(secure, 0444, memstat_show, NULL);
static DEVICE_ATTR(secure_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(mapped, 0444, memstat_show, NULL);
static DEVICE_ATTR(mapped_max, 0444, memstat_show, NULL);
static DEVICE_ATTR_RW(full_cache_threshold);

static const struct attribute *drv_attr_list[] = {
	&dev_attr_page_alloc.attr,
	&dev_attr_page_alloc_max.attr,
	&dev_attr_secure.attr,
	&dev_attr_secure_max.attr,
	&dev_attr_mapped.attr,
	&dev_attr_mapped_max.attr,
	&dev_attr_full_cache_threshold.attr,
	&dev_attr_max_reclaim_limit.attr,
	NULL,
};

void
kgsl_sharedmem_uninit_sysfs(void)
{
	sysfs_remove_files(&kgsl_driver.virtdev.kobj, drv_attr_list);
}

int
kgsl_sharedmem_init_sysfs(void)
{
	return sysfs_create_files(&kgsl_driver.virtdev.kobj, drv_attr_list);
}

static int kgsl_paged_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	int pgoff;
	unsigned int offset;
	struct page *page;
	struct kgsl_process_private *priv =
		((struct kgsl_mem_entry *)vma->vm_private_data)->priv;
	const bool shmem = !!(memdesc->priv & KGSL_MEMDESC_USE_SHMEM);

	offset = vmf->address - vma->vm_start;

	if (offset >= memdesc->size)
		return VM_FAULT_SIGBUS;

	pgoff = offset >> PAGE_SHIFT;

	page = kgsl_mmu_find_mapped_page(memdesc, offset);
	if (page == ZERO_PAGE(0))
		return VM_FAULT_SIGSEGV;
	else if (!IS_ERR_OR_NULL(page))
		get_page(page);
	else if (kgsl_memdesc_is_reclaimed(memdesc)) {
		/* We are here because page was reclaimed */
		page = shmem_read_mapping_page_gfp(
			memdesc->shmem_filp->f_mapping, pgoff,
			kgsl_gfp_mask(0));
		if (IS_ERR(page))
			return VM_FAULT_SIGBUS;
		kgsl_flush_page(page);

		/*
		 * Get a reference to the page only if the page was
		 * not already brought back.
		 */
		if (!TestSetPagePrivate2(page)) {
			memdesc->reclaimed_page_count--;
			atomic_dec(&priv->reclaimed_page_count);
			get_page(page);

			/*
			 * Isolate the page. If it fails after draining the
			 * local CPU's pagevec, try it again after draining the
			 * rest of the pagevecs.
			 */
			lru_add_drain();
			if (isolate_lru_page(page)) {
				lru_add_drain_all();
				WARN_ON(isolate_lru_page(page));
			}
		}
	} else
		return VM_FAULT_SIGBUS;

	/*
	 * If this is the first time this page is being mapped into userspace
	 * then bump the entry's mapsize.
	 */
	if (!shmem && page_mapcount(page) == 0)
		atomic_long_add(PAGE_SIZE, &memdesc->mapsize);

	vmf->page = page;

	return 0;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)

#include <soc/qcom/secure_buffer.h>

static int lock_sgt(struct sg_table *sgt, u64 size)
{
	int dest_perms = PERM_READ | PERM_WRITE;
	int source_vm = VMID_HLOS;
	int dest_vm = VMID_CP_PIXEL;
	int ret;

	do {
		ret = hyp_assign_table(sgt, &source_vm, 1, &dest_vm,
					&dest_perms, 1);
	} while (ret == -EAGAIN);

	if (ret) {
		/*
		 * If returned error code is EADDRNOTAVAIL, then this
		 * memory may no longer be in a usable state as security
		 * state of the pages is unknown after this failure. This
		 * memory can neither be added back to the pool nor buddy
		 * system.
		 */
		if (ret == -EADDRNOTAVAIL)
			pr_err("Failure to lock secure GPU memory 0x%llx bytes will not be recoverable\n",
				size);

		return ret;
	}

	return 0;
}

int kgsl_lock_page(struct page *page)
{
	struct sg_table sgt;
	struct scatterlist sgl;

	sgt.sgl = &sgl;
	sgt.nents = 1;
	sgt.orig_nents = 1;
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);

	return lock_sgt(&sgt, PAGE_SIZE);
}

static int unlock_sgt(struct sg_table *sgt)
{
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int source_vm = VMID_CP_PIXEL;
	int dest_vm = VMID_HLOS;
	int ret;

	do {
		ret = hyp_assign_table(sgt, &source_vm, 1, &dest_vm,
					&dest_perms, 1);
	} while (ret == -EAGAIN);

	if (ret)
		return ret;

	return 0;
}

int kgsl_unlock_page(struct page *page)
{
	struct sg_table sgt;
	struct scatterlist sgl;

	sgt.sgl = &sgl;
	sgt.nents = 1;
	sgt.orig_nents = 1;
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);

	return unlock_sgt(&sgt);
}
#endif

static void _dma_cache_op(struct device *dev, struct page *page,
		size_t size, unsigned int op)
{
	dma_addr_t dma_addr = phys_to_dma(dev, page_to_phys(page));

	switch (op) {
	case KGSL_CACHE_OP_FLUSH:
		dma_sync_single_for_device(dev, dma_addr, size, DMA_TO_DEVICE);
		dma_sync_single_for_device(dev, dma_addr, size, DMA_FROM_DEVICE);
		break;
	case KGSL_CACHE_OP_CLEAN:
		dma_sync_single_for_device(dev, dma_addr, size, DMA_TO_DEVICE);
		break;
	case KGSL_CACHE_OP_INV:
		dma_sync_single_for_device(dev, dma_addr, size, DMA_FROM_DEVICE);
		break;
	}
}

int kgsl_cache_range_op(struct kgsl_memdesc *memdesc, uint64_t offset,
		uint64_t size, unsigned int op)
{
	struct kgsl_device *device;
	struct page **pages;
	unsigned int i, page_count = 0;

	if (memdesc->flags & KGSL_MEMFLAGS_IOCOHERENT)
		return 0;

	if (size == 0 || size > UINT_MAX)
		return -EINVAL;

	/* Make sure that the offset + size does not overflow */
	if ((offset + size < offset) || (offset + size < size))
		return -ERANGE;

	/* Check that offset+length does not exceed memdesc->size */
	if (offset + size > memdesc->size)
		return -ERANGE;

	/* The entry must be associated with a page table. */
	if (IS_ERR_OR_NULL(memdesc->pagetable))
		return -ENODEV;

	device = KGSL_MMU_DEVICE(memdesc->pagetable->mmu);

	/* Get the pages backing this entry from the IOMMU. */
	pages = kgsl_mmu_find_mapped_page_range(memdesc, offset, size,
			&page_count);
	if (IS_ERR_OR_NULL(pages))
		return PTR_ERR(pages);

	for (i = 0; i < page_count;) {
		unsigned int num_pages;

		/* Skip over any unmapped/invalid pages. */
		if (IS_ERR_OR_NULL(pages[i])) {
			i++;
			continue;
		}

		num_pages = 1 << compound_order(pages[i]);
		_dma_cache_op(device->dev->parent, pages[i],
				min_t(size_t, num_pages, page_count - i)
				<< PAGE_SHIFT, op);
		i += num_pages;
	}

	kfree(pages);

	return 0;
}

void kgsl_memdesc_init(struct kgsl_device *device, struct kgsl_memdesc *memdesc,
		uint64_t flags, uint32_t priv)
{
	struct kgsl_mmu *mmu = &device->mmu;
	unsigned int align;

	memset(memdesc, 0, sizeof(*memdesc));
	/* Turn off SVM if the system doesn't support it */
	if (!kgsl_mmu_use_cpu_map(mmu))
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Secure memory disables advanced addressing modes */
	if (flags & KGSL_MEMFLAGS_SECURE)
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Disable IO coherence if it is not supported on the chip */
	if (!kgsl_mmu_has_feature(device, KGSL_MMU_IO_COHERENT))
		flags &= ~((uint64_t) KGSL_MEMFLAGS_IOCOHERENT);

	/*
	 * We can't enable I/O coherency on uncached surfaces because of
	 * situations where hardware might snoop the cpu caches which can
	 * have stale data. This happens primarily due to the limitations
	 * of dma caching APIs available on arm64
	 */
	if (!kgsl_cachemode_is_cached(flags))
		flags &= ~((u64) KGSL_MEMFLAGS_IOCOHERENT);

	if (kgsl_mmu_has_feature(device, KGSL_MMU_NEED_GUARD_PAGE))
		priv |= KGSL_MEMDESC_GUARD_PAGE;

	if (flags & KGSL_MEMFLAGS_SECURE)
		priv |= KGSL_MEMDESC_SECURE;

	if (device->flags & KGSL_FLAG_USE_SHMEM)
		priv |= KGSL_MEMDESC_USE_SHMEM;

	/* Never use shmem for system memory allocations. */
	if (priv & KGSL_MEMDESC_SYSMEM)
		priv &= ~KGSL_MEMDESC_USE_SHMEM;

	/* Configure lazy allocation flags for this entry. */
	kgsl_memdesc_set_lazy_configuration(device, memdesc, &flags, &priv);

	memdesc->flags = flags;
	memdesc->priv = priv;

	align = max_t(unsigned int,
		(memdesc->flags & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT,
		ilog2(PAGE_SIZE));
	kgsl_memdesc_set_align(memdesc, align);
	spin_lock_init(&memdesc->gpuaddr_lock);

	atomic_long_set(&memdesc->physsize, 0);
	atomic_long_set(&memdesc->mapsize, 0);
}

static int kgsl_shmem_alloc_page(struct page **pages,
			struct file *shmem_filp, unsigned int page_off)
{
	struct page *page;

	if (pages == NULL)
		return -EINVAL;

	page = shmem_read_mapping_page_gfp(shmem_filp->f_mapping, page_off,
			kgsl_gfp_mask(0));
	if (IS_ERR(page))
		return PTR_ERR(page);

	kgsl_zero_page(page, 0);

	*pages = page;

	/* Mark this shmem page as being active. */
	SetPagePrivate2(page);

	return 1;
}

static void kgsl_shmem_free_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
	struct page *page, *tmp;

	list_for_each_entry_safe(page, tmp, page_list, lru) {
		list_del(&page->lru);

		/* Mark this shmem page as being inactive. */
		ClearPageActive(page);
		ClearPagePrivate2(page);
		put_page(page);

		/* isolate_lru_page takes a ref: drop it here. */
		putback_lru_page(page);
	}
}

static int kgsl_memdesc_file_setup(struct kgsl_memdesc *memdesc, uint64_t size)
{
	int ret;

	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM) {
		memdesc->shmem_filp = shmem_file_setup("dev/kgsl-3d0", size,
				VM_NORESERVE);
		if (IS_ERR(memdesc->shmem_filp)) {
			ret = PTR_ERR(memdesc->shmem_filp);
			pr_err("kgsl: unable to setup shmem file err %d\n",
					ret);
			memdesc->shmem_filp = NULL;
			return ret;
		}

		/* Set the GFP mask for the shmem mapping. */
		mapping_set_gfp_mask(memdesc->shmem_filp->f_mapping,
				kgsl_gfp_mask(0));
	}

	return 0;
}

int kgsl_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			struct kgsl_memdesc *memdesc, unsigned int page_off)
{
	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM)
		return kgsl_shmem_alloc_page(pages, memdesc->shmem_filp,
						page_off);

	return kgsl_pool_alloc_page(page_size, pages, pages_len, align,
					memdesc);
}

void kgsl_free_page(struct kgsl_memdesc *memdesc, struct page *p)
{
	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM)
		put_page(p);
	else
		kgsl_pool_free_page(p);
}

static void kgsl_free_pages_from_sgt(struct kgsl_memdesc *memdesc)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(memdesc->sgt->sgl, sg, memdesc->sgt->nents, i) {
		/*
		 * sg_alloc_table_from_pages() will collapse any physically
		 * adjacent pages into a single scatterlist entry. We cannot
		 * just call __free_pages() on the entire set since we cannot
		 * ensure that the size is a whole order. Instead, free each
		 * page or compound page group individually.
		 */
		struct page *p = sg_page(sg), *next;
		unsigned int count;
		unsigned int j = 0;

		while (j < (sg->length/PAGE_SIZE)) {
			count = 1 << compound_order(p);
			next = nth_page(p, count);
			kgsl_free_page(memdesc, p);

			p = next;
			j += count;
		}
	}
}

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	LIST_HEAD(page_list);
	int ret = 0;
	bool usermem, secured, lazy;

	if (!memdesc || !memdesc->size)
		return;

	/* ION/usermem buffers need special handling on unmap. */
	usermem = kgsl_memdesc_usermem_type(memdesc) != KGSL_MEM_ENTRY_KERNEL;
	secured = kgsl_memdesc_is_secured(memdesc);
	lazy = (memdesc->priv & KGSL_MEMDESC_LAZY_ALLOCATION) != 0;

	/* Look up the backing pages of the entry (if applicable). */
	if (!(memdesc->priv & KGSL_MEMDESC_FIXED) && !usermem && (!secured || lazy))
		ret = kgsl_mmu_get_backing_pages(memdesc, &page_list);

	/*
	 * For non-global entries make sure the memory object has been unmapped.
	 * Globals are handled separately in kgsl_iommu_unmap_globals.
	 */
	if (!ret && !kgsl_memdesc_is_global(memdesc))
		ret = kgsl_mmu_unmap(memdesc->pagetable, memdesc, &page_list);

	/*
	 * Do not free the gpuaddr/size if unmap failed. If we try to map over
	 * this range in future the iommu driver will throw an error because it
	 * feels we are overwriting a mapping.
	 */
	if (!ret)
		kgsl_mmu_put_gpuaddr(memdesc);

	/*
	 * Even if the page list is empty there might be something we need to do
	 * to free this entry. Panic if for whatever reason we don't have a free
	 * operation specified for this memdesc but we've still got a non-empty
	 * list, because this should never happen (that would imply that there's
	 * a memory type we're not handling correctly).
	 */
	if (memdesc->ops && memdesc->ops->free)
		memdesc->ops->free(memdesc, &page_list);
	else
		BUG_ON(!list_empty(&page_list));

	/*
	 * The entry's SG table should have been freed by now (except for
	 * globals and non-lazy secure entries) since that's handled when
	 * entries are mapped in, but check here just in case.
	 */
	if (!IS_ERR_OR_NULL(memdesc->sgt)) {
		sg_free_table(memdesc->sgt);
		kfree(memdesc->sgt);

		memdesc->sgt = NULL;
	}

	if (memdesc->shmem_filp) {
		fput(memdesc->shmem_filp);
		memdesc->shmem_filp = NULL;
	}
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
void kgsl_free_secure_page(struct page *page)
{
	if (!page)
		return;

	if (!kgsl_unlock_page(page)) {
		__free_page(page);

		atomic_long_sub(PAGE_SIZE, &kgsl_driver.stats.secure);
	}
}

struct page *kgsl_alloc_secure_page(void)
{
	struct page *page;
	int status;

	page = alloc_page(GFP_KERNEL | __GFP_ZERO |
			__GFP_NORETRY | __GFP_HIGHMEM);
	if (!page)
		return NULL;

	KGSL_STATS_ADD(PAGE_SIZE, &kgsl_driver.stats.secure,
			&kgsl_driver.stats.secure_max);

	status = kgsl_lock_page(page);
	if (status) {
		if (status == -EADDRNOTAVAIL)
			return NULL;

		__free_page(page);
		atomic_long_sub(PAGE_SIZE, &kgsl_driver.stats.secure);
		return NULL;
	}
	return page;
}
#else
void kgsl_free_secure_page(struct page *page)
{
}

struct page *kgsl_alloc_secure_page(void)
{
	return NULL;
}
#endif

static const char * const memtype_str[] = {
	[KGSL_MEMTYPE_OBJECTANY] = "any(0)",
	[KGSL_MEMTYPE_FRAMEBUFFER] = "framebuffer",
	[KGSL_MEMTYPE_RENDERBUFFER] = "renderbuffer",
	[KGSL_MEMTYPE_ARRAYBUFFER] = "arraybuffer",
	[KGSL_MEMTYPE_ELEMENTARRAYBUFFER] = "elementarraybuffer",
	[KGSL_MEMTYPE_VERTEXARRAYBUFFER] = "vertexarraybuffer",
	[KGSL_MEMTYPE_TEXTURE] = "texture",
	[KGSL_MEMTYPE_SURFACE] = "surface",
	[KGSL_MEMTYPE_EGL_SURFACE] = "egl_surface",
	[KGSL_MEMTYPE_GL] = "gl",
	[KGSL_MEMTYPE_CL] = "cl",
	[KGSL_MEMTYPE_CL_BUFFER_MAP] = "cl_buffer_map",
	[KGSL_MEMTYPE_CL_BUFFER_NOMAP] = "cl_buffer_nomap",
	[KGSL_MEMTYPE_CL_IMAGE_MAP] = "cl_image_map",
	[KGSL_MEMTYPE_CL_IMAGE_NOMAP] = "cl_image_nomap",
	[KGSL_MEMTYPE_CL_KERNEL_STACK] = "cl_kernel_stack",
	[KGSL_MEMTYPE_COMMAND] = "command",
	[KGSL_MEMTYPE_2D] = "2d",
	[KGSL_MEMTYPE_EGL_IMAGE] = "egl_image",
	[KGSL_MEMTYPE_EGL_SHADOW] = "egl_shadow",
	[KGSL_MEMTYPE_MULTISAMPLE] = "egl_multisample",
	[KGSL_MEMTYPE_VK_ANY] = "vk_any",
	[KGSL_MEMTYPE_VK_INSTANCE] = "vk_instance",
	[KGSL_MEMTYPE_VK_PHYSICALDEVICE] = "vk_physicaldevice",
	[KGSL_MEMTYPE_VK_DEVICE] = "vk_device",
	[KGSL_MEMTYPE_VK_QUEUE] = "vk_queue",
	[KGSL_MEMTYPE_VK_CMDBUFFER] = "vk_cmdbuffer",
	[KGSL_MEMTYPE_VK_DEVICEMEMORY] = "vk_devicememory",
	[KGSL_MEMTYPE_VK_BUFFER] = "vk_buffer",
	[KGSL_MEMTYPE_VK_BUFFERVIEW] = "vk_bufferview",
	[KGSL_MEMTYPE_VK_IMAGE] = "vk_image",
	[KGSL_MEMTYPE_VK_IMAGEVIEW] = "vk_imageview",
	[KGSL_MEMTYPE_VK_SHADERMODULE] = "vk_shadermodule",
	[KGSL_MEMTYPE_VK_PIPELINE] = "vk_pipeline",
	[KGSL_MEMTYPE_VK_PIPELINECACHE] = "vk_pipelinecache",
	[KGSL_MEMTYPE_VK_PIPELINELAYOUT] = "vk_pipelinelayout",
	[KGSL_MEMTYPE_VK_SAMPLER] = "vk_sampler",
	[KGSL_MEMTYPE_VK_SAMPLERYCBCRCONVERSIONKHR] = "vk_samplerycbcrconversionkhr",
	[KGSL_MEMTYPE_VK_DESCRIPTORSET] = "vk_descriptorset",
	[KGSL_MEMTYPE_VK_DESCRIPTORSETLAYOUT] = "vk_descriptorsetlayout",
	[KGSL_MEMTYPE_VK_DESCRIPTORPOOL] = "vk_descriptorpool",
	[KGSL_MEMTYPE_VK_FENCE] = "vk_fence",
	[KGSL_MEMTYPE_VK_SEMAPHORE] = "vk_semaphore",
	[KGSL_MEMTYPE_VK_EVENT] = "vk_event",
	[KGSL_MEMTYPE_VK_QUERYPOOL] = "vk_querypool",
	[KGSL_MEMTYPE_VK_FRAMEBUFFER] = "vk_framebuffer",
	[KGSL_MEMTYPE_VK_RENDERPASS] = "vk_renderpass",
	[KGSL_MEMTYPE_VK_PROGRAM] = "vk_program",
	[KGSL_MEMTYPE_VK_QUERY] = "vk_query",
	/* KGSL_MEMTYPE_KERNEL handled below, to avoid huge array */
};

void kgsl_get_memory_usage(char *name, size_t name_size, uint64_t memflags)
{
	unsigned int type = MEMFLAGS(memflags, KGSL_MEMTYPE_MASK,
		KGSL_MEMTYPE_SHIFT);

	if (type == KGSL_MEMTYPE_KERNEL)
		strlcpy(name, "kernel", name_size);
	else if (type < ARRAY_SIZE(memtype_str) && memtype_str[type] != NULL)
		strlcpy(name, memtype_str[type], name_size);
	else
		snprintf(name, name_size, "VK/others(%3d)", type);
}

static int kgsl_memdesc_sg_dma(struct kgsl_memdesc *memdesc,
		phys_addr_t addr, u64 size)
{
	int ret;
	struct page *page = phys_to_page(addr);

	memdesc->sgt = kmalloc(sizeof(*memdesc->sgt), GFP_KERNEL);
	if (memdesc->sgt == NULL)
		return -ENOMEM;

	ret = sg_alloc_table(memdesc->sgt, 1, GFP_KERNEL);
	if (ret) {
		kfree(memdesc->sgt);
		memdesc->sgt = NULL;
		return ret;
	}

	sg_set_page(memdesc->sgt->sgl, page, (size_t) size, 0);
	return 0;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static void kgsl_free_secure_system_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
	struct scatterlist *sg;
	int i;
	int ret = unlock_sgt(memdesc->sgt);

	if (ret) {
		/*
		 * Unlock of the secure buffer failed. This buffer will
		 * be stuck in secure side forever and is unrecoverable.
		 * Give up on the buffer and don't return it to the
		 * pool.
		 */
		pr_err("kgsl: secure buf unlock failed: size: %llx ret: %d\n",
				memdesc->size, ret);
		return;
	}

	for_each_sg(memdesc->sgt->sgl, sg, memdesc->sgt->nents, i) {
		struct page *page = sg_page(sg);

		__free_page(page);
	}

	atomic_long_sub(memdesc->size, &memdesc->physsize);
	atomic_long_sub(memdesc->size, &kgsl_driver.stats.secure);
}

static void kgsl_free_secure_pool_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
	int ret = unlock_sgt(memdesc->sgt);

	if (ret) {
		/*
		 * Unlock of the secure buffer failed. This buffer will
		 * be stuck in secure side forever and is unrecoverable.
		 * Give up on the buffer and don't return it to the
		 * pool.
		 */
		pr_err("kgsl: secure buf unlock failed: size: %llx ret: %d\n",
				memdesc->size, ret);
		return;
	}

	kgsl_free_pages_from_sgt(memdesc);

	atomic_long_sub(memdesc->size, &memdesc->physsize);
	atomic_long_sub(memdesc->size, &kgsl_driver.stats.secure);
}
#endif

static void kgsl_free_system_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
	struct page *page, *tmp;
	size_t freed_size = 0;

	list_for_each_entry_safe(page, tmp, page_list, lru) {
		list_del(&page->lru);

		freed_size += PAGE_SIZE;
		__free_page(page);
	}

	atomic_long_sub(freed_size, &memdesc->physsize);
	atomic_long_sub(freed_size, &kgsl_driver.stats.page_alloc);
}

static void kgsl_pool_free_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
	struct page *page, *tmp;
	size_t freed_size = 0;
	unsigned int order;

	list_for_each_entry_safe(page, tmp, page_list, lru) {
		list_del(&page->lru);

		freed_size += (size_t)order << PAGE_SHIFT;
		kgsl_pool_free_page(page);
	}

	atomic_long_sub(freed_size, &memdesc->physsize);
	atomic_long_sub(freed_size, &kgsl_driver.stats.page_alloc);
}

static void kgsl_free_pool_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM)
		kgsl_shmem_free_pages(memdesc, page_list);
	else
		kgsl_pool_free_pages(memdesc, page_list);
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static struct kgsl_memdesc_ops kgsl_secure_system_ops = {
	.free = kgsl_free_secure_system_pages,
	/* Do not allow CPU mappings for secure buffers */
	.vmflags = 0,
	.vmfault = NULL,
};

static struct kgsl_memdesc_ops kgsl_secure_pool_ops = {
	.free = kgsl_free_secure_pool_pages,
	/* Do not allow CPU mappings for secure buffers */
	.vmflags = 0,
	.vmfault = NULL,
};

static struct kgsl_memdesc_ops kgsl_secure_lazy_ops = {
	.free = kgsl_free_lazy_pages,
	/* Do not allow CPU mappings for secure buffers */
	.vmflags = 0,
	.vmfault = NULL,
};
#endif

static struct kgsl_memdesc_ops kgsl_pool_ops = {
	.free = kgsl_free_pool_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_paged_vmfault,
};

static struct kgsl_memdesc_ops kgsl_system_ops = {
	.free = kgsl_free_system_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_paged_vmfault,
};

static struct kgsl_memdesc_ops kgsl_lazy_ops = {
	.free = kgsl_free_lazy_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_lazy_vmfault,
};

static int kgsl_system_alloc_pages(u64 size, struct page ***pages,
		struct device *dev)
{
	struct scatterlist sg;
	struct page **local;
	int i, npages = size >> PAGE_SHIFT;

	local = kvcalloc(npages, sizeof(*pages), GFP_KERNEL | __GFP_NORETRY);
	if (!local)
		return -ENOMEM;

	for (i = 0; i < npages; i++) {
		gfp_t gfp = __GFP_ZERO | kgsl_gfp_mask(0);

		local[i] = alloc_page(gfp);
		if (!local[i]) {
			for (i = i - 1; i >= 0; i--)
				__free_page(local[i]);
			kvfree(local);
			return -ENOMEM;
		}

		/* Make sure the cache is clean */
		sg_init_table(&sg, 1);
		sg_set_page(&sg, local[i], PAGE_SIZE, 0);
		sg_dma_address(&sg) = page_to_phys(local[i]);

		dma_sync_sg_for_device(dev, &sg, 1, DMA_BIDIRECTIONAL);
	}

	*pages = local;
	return npages;
}

static void _free_pages_from_array(struct kgsl_memdesc *memdesc,
		struct page **pages, int count)
{
	LIST_HEAD(page_list);
	int i;

	if (!memdesc->ops || !memdesc->ops->free)
		return;

	for (i = 0; i < count; i++)
		list_add_tail(&pages[i]->lru, &page_list);

	memdesc->ops->free(memdesc, &page_list);
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static int kgsl_alloc_secure_pages(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	struct page **pages = NULL;
	struct sg_table *sgt = NULL;
	int count = 0;
	int ret = 0;

	size = PAGE_ALIGN(size);

	if (!size || size > UINT_MAX)
		return -EINVAL;

	kgsl_memdesc_init(device, memdesc, flags, priv);

	/* Secure entries never use shmem. */
	BUG_ON(memdesc->priv & KGSL_MEMDESC_USE_SHMEM);

	if (memdesc->priv & KGSL_MEMDESC_LAZY_ALLOCATION) {
		/*
		 * Lazy allocation should be masked off from priv when the
		 * kernel config is disabled, so panic here if something's
		 * trying to work around that.
		 */
		BUG_ON(!IS_ENABLED(CONFIG_QCOM_KGSL_LAZY_ALLOCATION) ||
		       !IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION));

		/* Force a minimum alignment on lazy entries based on size. */
		kgsl_memdesc_set_lazy_align(memdesc, size);

		memdesc->ops = &kgsl_secure_lazy_ops;
		goto done;
	} else if (memdesc->priv & KGSL_MEMDESC_SYSMEM) {
		memdesc->ops = &kgsl_secure_system_ops;
		count = kgsl_system_alloc_pages(size, &pages, device->dev);
	} else {
		memdesc->ops = &kgsl_secure_pool_ops;
		count = kgsl_pool_alloc_pages(memdesc, size, &pages);
	}

	if (count < 0) {
		if (memdesc->shmem_filp)
			fput(memdesc->shmem_filp);
		memdesc->shmem_filp = NULL;
		return count;
	}

	sgt = kgsl_alloc_sgt_from_pages(pages, count);
	if (IS_ERR_OR_NULL(sgt)) {
		_free_pages_from_array(memdesc, pages, count);
		return PTR_ERR(sgt);
	}

	ret = lock_sgt(sgt, size);
	if (ret) {
		if (ret != -EADDRNOTAVAIL)
			_free_pages_from_array(memdesc, pages, count);
		sg_free_table(sgt);
		kfree(sgt);
		kvfree(pages);
		return ret;
	}

	/* Now that we've moved to a sg table don't need the pages anymore */
	kvfree(pages);

done:
	memdesc->sgt = sgt;
	memdesc->size = size;

	if (!(memdesc->priv & KGSL_MEMDESC_LAZY_ALLOCATION)) {
		atomic_long_add((size_t)count << PAGE_SHIFT,
			&memdesc->physsize);

		KGSL_STATS_ADD(size, &kgsl_driver.stats.secure,
			&kgsl_driver.stats.secure_max);
	}

	return 0;
}
#endif

static int kgsl_alloc_pages(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	struct page **pages = NULL;
	struct sg_table *sgt = NULL;
	int count = 0;
	int i;
	bool use_shmem;

	size = PAGE_ALIGN(size);

	if (!size || size > UINT_MAX)
		return -EINVAL;

	kgsl_memdesc_init(device, memdesc, flags, priv);

	/* Create the shmem file for this entry if applicable. */
	use_shmem = (memdesc->priv & KGSL_MEMDESC_USE_SHMEM) != 0;
	if (use_shmem) {
		count = kgsl_memdesc_file_setup(memdesc, size);
		if (count)
			return count;
	}

	if (memdesc->priv & KGSL_MEMDESC_LAZY_ALLOCATION) {
		/*
		 * Lazy allocation should be masked off from priv when the
		 * kernel config is disabled, so panic here if something's
		 * trying to work around that.
		 */
		BUG_ON(!IS_ENABLED(CONFIG_QCOM_KGSL_LAZY_ALLOCATION));

		/* Force a minimum alignment on lazy entries based on size. */
		kgsl_memdesc_set_lazy_align(memdesc, size);

		memdesc->ops = &kgsl_lazy_ops;
		goto done;
	} else if (memdesc->priv & KGSL_MEMDESC_SYSMEM) {
		memdesc->ops = &kgsl_system_ops;
		count = kgsl_system_alloc_pages(size, &pages, device->dev);
	} else {
		memdesc->ops = &kgsl_pool_ops;
		count = kgsl_pool_alloc_pages(memdesc, size, &pages);
	}

	if (count < 0) {
		if (memdesc->shmem_filp)
			fput(memdesc->shmem_filp);
		memdesc->shmem_filp = NULL;
		return count;
	}

	/* Isolate any shmem pages that may have been allocated. */
	if (use_shmem) {
		/* Flush the LRU pagevecs to ensure the pages are assigned. */
		lru_add_drain_all();

		/* Isolate the pages. */
		for (i = 0; i < count; i++)
			WARN_ON(isolate_lru_page(pages[i]));
	}

	sgt = kgsl_alloc_sgt_from_pages(pages, count);
	if (IS_ERR_OR_NULL(sgt)) {
		_free_pages_from_array(memdesc, pages, count);
		return PTR_ERR(sgt);
	}

	/* Now that we've moved to a sg table don't need the pages anymore */
	kvfree(pages);

done:
	memdesc->sgt = sgt;
	memdesc->size = size;

	if (!(memdesc->priv & KGSL_MEMDESC_LAZY_ALLOCATION) && !use_shmem) {
		atomic_long_add((size_t)count << PAGE_SHIFT,
				&memdesc->physsize);

		KGSL_STATS_ADD(size, &kgsl_driver.stats.page_alloc,
				&kgsl_driver.stats.page_alloc_max);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static int kgsl_allocate_secure(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	return kgsl_alloc_secure_pages(device, memdesc, size, flags, priv);
}
#else
static int kgsl_allocate_secure(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	return -ENODEV;
}
#endif

int kgsl_allocate_user(struct kgsl_device *device, struct kgsl_memdesc *memdesc,
		u64 size, u64 flags, u32 priv)
{
	if (flags & KGSL_MEMFLAGS_SECURE)
		return kgsl_allocate_secure(device, memdesc, size, flags, priv);

	return kgsl_alloc_pages(device, memdesc, size, flags, priv);
}

struct kgsl_memdesc *kgsl_allocate_global_fixed(struct kgsl_device *device,
		const char *resource, const char *name)
{
	struct kgsl_global_memdesc *md;
	u32 entry[2];
	int ret;

	if (of_property_read_u32_array(device->pdev->dev.of_node,
		resource, entry, 2))
		return ERR_PTR(-ENODEV);

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return ERR_PTR(-ENOMEM);

	kgsl_memdesc_init(device, &md->memdesc, 0, KGSL_MEMDESC_GLOBAL |
			KGSL_MEMDESC_FIXED);
	md->memdesc.size = entry[1];

	ret = kgsl_memdesc_sg_dma(&md->memdesc, entry[0], entry[1]);
	if (ret) {
		kfree(md);
		return ERR_PTR(ret);
	}

	md->name = name;

	/* Allow kernel write access to global buffers by default. */
	md->memdesc.priv |= KGSL_MEMDESC_KERNEL_RW;

	/*
	 * No lock here, because this function is only called during probe/init
	 * while the caller is holding the mutex
	 */
	list_add_tail(&md->node, &device->globals);

	kgsl_mmu_map_global(device, &md->memdesc);

	return &md->memdesc;
}

static struct kgsl_memdesc *
kgsl_allocate_secure_global(struct kgsl_device *device,
		u64 size, u64 flags, u32 priv, const char *name)
{
	struct kgsl_global_memdesc *md;
	int ret;

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return ERR_PTR(-ENOMEM);

	/* Make sure that we get global memory from system memory */
	priv |= KGSL_MEMDESC_GLOBAL | KGSL_MEMDESC_SYSMEM;

	ret = kgsl_allocate_secure(device, &md->memdesc, size, flags, priv);
	if (ret) {
		kfree(md);
		return ERR_PTR(ret);
	}

	md->name = name;

	/*
	 * No lock here, because this function is only called during probe/init
	 * while the caller is holding the mutex
	 */
	list_add_tail(&md->node, &device->globals);

	/*
	 * No offset needed, we'll get an address inside of the pagetable
	 * normally
	 */
	kgsl_mmu_map_global(device, &md->memdesc);

	return &md->memdesc;
}

struct kgsl_memdesc *kgsl_allocate_global(struct kgsl_device *device,
		u64 size, u64 flags, u32 priv, const char *name)
{
	int ret;
	struct kgsl_global_memdesc *md;
	struct kgsl_memdesc *memdesc;

	if (flags & KGSL_MEMFLAGS_SECURE)
		return kgsl_allocate_secure_global(device, size, flags, priv,
			name);

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return ERR_PTR(-ENOMEM);

	/*
	 * Make sure that we get global memory from system memory to keep from
	 * taking up pool memory for the life of the driver
	 */
	priv |= KGSL_MEMDESC_GLOBAL | KGSL_MEMDESC_SYSMEM;

	/* Allow kernel write access to non-secure global buffers by default. */
	priv |= KGSL_MEMDESC_KERNEL_RW;

	memdesc = &md->memdesc;
	ret = kgsl_allocate_user(device, memdesc, size, flags, priv);
	if (ret) {
		kfree(md);
		return ERR_PTR(ret);
	}

	md->name = name;

	/*
	 * No lock here, because this function is only called during probe/init
	 * while the caller is holding the mute
	 */
	list_add_tail(&md->node, &device->globals);

	kgsl_mmu_map_global(device, memdesc);

	return &md->memdesc;
}

void kgsl_free_globals(struct kgsl_device *device)
{
	struct kgsl_global_memdesc *md, *tmp;

	list_for_each_entry_safe(md, tmp, &device->globals, node) {
		kgsl_sharedmem_free(&md->memdesc);
		list_del(&md->node);
		kfree(md);
	}
}

void *kgsl_sharedmem_vm_map_readonly(struct kgsl_memdesc *memdesc, u64 offset,
		u64 size, unsigned int *page_count)
{
	struct page **pages;
	void *kptr;
	unsigned int i;

	/*
	 * vm_map_ram unfortunately can sleep, so don't use this in an atomic
	 * context or you might cause deadlocks.
	 */
	might_sleep();

	/* Secure buffers cannot be mapped into the kernel address space. */
	if (kgsl_memdesc_is_secured(memdesc))
		return ERR_PTR(-EPERM);

	pages = kgsl_mmu_find_mapped_page_range(memdesc, offset, size,
			page_count);
	if (IS_ERR_OR_NULL(pages))
		return (void *)pages;

	/*
	 * Ensure that any NULL or otherwise invalid pages get mapped to the
	 * zero page so that the full requested mapping is valid and readable.
	 */
	for (i = 0; i < *page_count; i++)
		if (IS_ERR_OR_NULL(pages[i]))
			pages[i] = ZERO_PAGE(0);

	kptr = vm_map_ram(pages, *page_count, -1, kgsl_pgprot_modify(memdesc,
			PAGE_KERNEL_RO));

	/* Clean up after ourselves. */
	kfree(pages);

	return kptr;
}

void *kgsl_sharedmem_vm_map_readwrite(struct kgsl_memdesc *memdesc, u64 offset,
		u64 size, unsigned int *page_count)
{
	struct page **pages;
	void *kptr;
	size_t i;
	int ret = 0;

	/*
	 * vm_map_ram unfortunately can sleep, so don't use this in an atomic
	 * context or you might cause deadlocks.
	 */
	might_sleep();

	/* Secure buffers cannot be mapped into the kernel address space. */
	if (kgsl_memdesc_is_secured(memdesc))
		return ERR_PTR(-EPERM);

	/* Error out if the entry does not allow write access to the kernel. */
	if (!(memdesc->priv & KGSL_MEMDESC_KERNEL_RW))
		return ERR_PTR(-EPERM);

	pages = kgsl_mmu_find_mapped_page_range(memdesc, offset, size,
			page_count);
	if (IS_ERR_OR_NULL(pages))
		return (void *)pages;

	/* Ensure that the full requested mapping is valid and writable. */
	for (i = 0; i < *page_count; i++) {
		if (IS_ERR_OR_NULL(pages[i])) {
			if (memdesc->priv & KGSL_MEMDESC_LAZY_ALLOCATION) {
				/*
				 * This entry uses lazy allocation. Attempt
				 * to allocate the missing/invalid page and
				 * continue if successful.
				 */
				ret = kgsl_fetch_lazy_page(memdesc,
						offset + (i << PAGE_SHIFT),
						&pages[i]);
				if (!ret && !IS_ERR_OR_NULL(pages[i]))
					continue;
			}

			kfree(pages);
			*page_count = 0;

			return ERR_PTR(-EPERM);
		}
	}

	kptr = vm_map_ram(pages, *page_count, -1, kgsl_pgprot_modify(memdesc,
			PAGE_KERNEL));

	/* Clean up after ourselves. */
	kfree(pages);

	return kptr;
}

void kgsl_sharedmem_set_noretry(bool val)
{
	sharedmem_noretry_flag = val;
}

bool kgsl_sharedmem_get_noretry(void)
{
	return sharedmem_noretry_flag;
}

void kgsl_zero_page(struct page *p, unsigned int order)
{
	int i;

	for (i = 0; i < (1 << order); i++) {
		struct page *page = nth_page(p, i);
		void *addr = kmap_atomic(page);

		memset(addr, 0, PAGE_SIZE);
		dmac_flush_range(addr, addr + PAGE_SIZE);
		kunmap_atomic(addr);
	}
}

void kgsl_flush_page(struct page *page)
{
	void *addr = kmap_atomic(page);

	dmac_flush_range(addr, addr + PAGE_SIZE);
	kunmap_atomic(addr);
}

unsigned int kgsl_gfp_mask(unsigned int page_order)
{
	unsigned int gfp_mask = __GFP_HIGHMEM;

	if (page_order > 0) {
		gfp_mask |= __GFP_COMP | __GFP_NORETRY | __GFP_NOWARN;
		gfp_mask &= ~__GFP_RECLAIM;
	} else
		gfp_mask |= GFP_KERNEL;

	if (kgsl_sharedmem_get_noretry())
		gfp_mask |= __GFP_NORETRY | __GFP_NOWARN;

	return gfp_mask;
}
