// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-direct.h>
#include <linux/of_platform.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/random.h>

#include "kgsl_device.h"
#include "kgsl_pool.h"
#include "kgsl_sharedmem.h"

/*
 * The user can set this from debugfs to force failed memory allocations to
 * fail without trying OOM first.  This is a debug setting useful for
 * stress applications that want to test failure cases without pushing the
 * system into unrecoverable OOM panics
 */

static bool sharedmem_noretry_flag;

/* An attribute for showing per-process memory statistics */
struct kgsl_mem_entry_attribute {
	struct attribute attr;
	int memtype;
	ssize_t (*show)(struct kgsl_process_private *priv,
		int type, char *buf);
};

#define to_mem_entry_attr(a) \
container_of(a, struct kgsl_mem_entry_attribute, attr)

#define __MEM_ENTRY_ATTR(_type, _name, _show) \
{ \
	.attr = { .name = __stringify(_name), .mode = 0444 }, \
	.memtype = _type, \
	.show = _show, \
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

	if (kgsl_memdesc_usermem_type(m) != KGSL_MEM_ENTRY_ION)
		return 0;

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
				kgsl_get_allocator(entry);
			kgsl_process_private_put(allocator);
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

	spin_lock(&priv->mem_lock);
	for (entry = idr_get_next(&priv->mem_idr, &id); entry;
		id++, entry = idr_get_next(&priv->mem_idr, &id)) {

		if (kgsl_mem_entry_get(entry) == 0)
			continue;
		spin_unlock(&priv->mem_lock);

		imported_mem += imported_mem_account(priv, entry);

		kgsl_mem_entry_put(entry);
		spin_lock(&priv->mem_lock);
	}
	spin_unlock(&priv->mem_lock);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", imported_mem);
}

static void gpumem_account(struct kgsl_process_private *priv, int type,
		int64_t *mapped, int64_t *unmapped)
{
	struct kgsl_mem_entry *entry;
	int64_t gpumem_mapped = 0;
	int64_t gpumem_unmapped = 0;
	int id = 0;
	int i;

	spin_lock(&priv->mem_lock);
	idr_for_each_entry_continue(&priv->mem_idr, entry, id) {
		struct kgsl_memdesc *m = &entry->memdesc;
		int64_t entry_mapped;
		int64_t entry_unmapped;
		uint64_t offset;
		int entry_mapcount;

		if (entry->pending_free || kgsl_memdesc_usermem_type(m) != type)
			continue;

		entry_mapcount = atomic_read(&entry->map_count);
		entry_mapped = atomic_long_read(&m->mapsize);
		entry_unmapped = atomic_long_read(&m->physsize) - entry_mapped;

		/*
		 * If the entry is not mapped to userspace or only mapped once
		 * then we can fastpath handling this entry's accounting without
		 * needing to grab a reference.
		 */
		if (entry_mapcount <= 1) {
			gpumem_unmapped += entry_unmapped;
			gpumem_mapped += entry_mapped;
			continue;
		}

		/* No such luck. Try and grab a reference to the entry. */
		if (kgsl_mem_entry_get(entry) == 0)
			continue;

		/* Exit the spinlock since the code below can sleep. */
		spin_unlock(&priv->mem_lock);

		/*
		 * Multiple VMAs cover this entry. Walk the backing pages and
		 * check their individual mapcount when counting PSS to match
		 * proc/smaps behavior.
		 */
		offset = 0;
		for (i = 0; i < m->page_count; i++, offset += PAGE_SIZE) {
			struct page *page;
			int mapcount;

			page = kgsl_mmu_find_mapped_page(m, offset);
			if (IS_ERR_OR_NULL(page) || page == ZERO_PAGE(0))
				continue;

			/*
			 * We only need to worry about pages that have been
			 * mapped into more than one VMA since mapsize already
			 * accounts for the first userspace mapping.
			 */
			mapcount = page_mapcount(page);
			if (mapcount <= 1)
				continue;

			entry_mapped += (PAGE_SIZE / mapcount) - PAGE_SIZE;
		}

		gpumem_mapped += entry_mapped;
		gpumem_unmapped += entry_unmapped;

		kgsl_mem_entry_put_deferred(entry);
		spin_lock(&priv->mem_lock);
	}
	spin_unlock(&priv->mem_lock);

	if (mapped)
		*mapped = gpumem_mapped;
	if (unmapped)
		*unmapped = gpumem_unmapped;
}

static ssize_t
gpumem_account_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	int64_t gpumem_mapped = 0;
	int64_t gpumem_unmapped = 0;

	gpumem_account(priv, type, &gpumem_mapped, &gpumem_unmapped);

	return scnprintf(buf, PAGE_SIZE, "%lld,%lld\n", gpumem_mapped,
			gpumem_unmapped);
}

static ssize_t
gpumem_mapped_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	int64_t gpumem_mapped = 0;

	gpumem_account(priv, type, &gpumem_mapped, NULL);

	return scnprintf(buf, PAGE_SIZE, "%lld\n", gpumem_mapped);
}

static ssize_t
gpumem_unmapped_show(struct kgsl_process_private *priv, int type, char *buf)
{
	int64_t gpumem_unmapped = 0;

	gpumem_account(priv, type, NULL, &gpumem_unmapped);

	return scnprintf(buf, PAGE_SIZE, "%lld\n", gpumem_unmapped);
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

static ssize_t mem_entry_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_mem_entry_attribute *pattr = to_mem_entry_attr(attr);
	struct kgsl_process_private *priv;
	ssize_t ret;

	/*
	 * kgsl_process_init_sysfs takes a refcount to the process_private,
	 * which is put when the kobj is released. This implies that priv will
	 * not be freed until this function completes, and no further locking
	 * is needed.
	 */
	priv = kobj ? container_of(kobj, struct kgsl_process_private, kobj) :
			NULL;

	if (priv && pattr->show)
		ret = pattr->show(priv, pattr->memtype, buf);
	else
		ret = -EIO;

	return ret;
}

static void mem_entry_release(struct kobject *kobj)
{
	struct kgsl_process_private *priv;

	priv = container_of(kobj, struct kgsl_process_private, kobj);
	/* Put the refcount we got in kgsl_process_init_sysfs */
	kgsl_process_private_put(priv);
}

static const struct sysfs_ops mem_entry_sysfs_ops = {
	.show = mem_entry_sysfs_show,
};

static struct kobj_type ktype_mem_entry = {
	.sysfs_ops = &mem_entry_sysfs_ops,
	.release = &mem_entry_release,
};

static struct mem_entry_stats mem_stats[] = {
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_KERNEL, kernel),
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_USER, user),
#ifdef CONFIG_ION
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_ION, ion),
#endif
};

void
kgsl_process_uninit_sysfs(struct kgsl_process_private *private)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		sysfs_remove_file(&private->kobj, &mem_stats[i].attr.attr);
		sysfs_remove_file(&private->kobj,
			&mem_stats[i].max_attr.attr);
	}

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

	/* Keep private valid until the sysfs enries are removed. */
	kgsl_process_private_get(private);

	if (kobject_init_and_add(&private->kobj, &ktype_mem_entry,
		kgsl_driver.prockobj, "%d", pid_nr(private->pid))) {
		dev_err(device->dev, "Unable to add sysfs for process %d\n",
			pid_nr(private->pid));
		return;
	}

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		int ret;

		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].attr.attr);
		ret |= sysfs_create_file(&private->kobj,
			&mem_stats[i].max_attr.attr);

		if (ret)
			dev_err(device->dev,
				"Unable to create sysfs files for process %d\n",
				pid_nr(private->pid));
	}

	for (i = 0; i < ARRAY_SIZE(debug_memstats); i++) {
		if (sysfs_create_file(&private->kobj,
			&debug_memstats[i].attr))
			WARN(1, "Couldn't create sysfs file '%s'\n",
				debug_memstats[i].attr.name);
	}
}

static ssize_t memstat_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	uint64_t val = 0;

	if (!strcmp(attr->attr.name, "vmalloc"))
		val = atomic_long_read(&kgsl_driver.stats.vmalloc);
	else if (!strcmp(attr->attr.name, "vmalloc_max"))
		val = atomic_long_read(&kgsl_driver.stats.vmalloc_max);
	else if (!strcmp(attr->attr.name, "page_alloc"))
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

static DEVICE_ATTR(vmalloc, 0444, memstat_show, NULL);
static DEVICE_ATTR(vmalloc_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(page_alloc, 0444, memstat_show, NULL);
static DEVICE_ATTR(page_alloc_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(secure, 0444, memstat_show, NULL);
static DEVICE_ATTR(secure_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(mapped, 0444, memstat_show, NULL);
static DEVICE_ATTR(mapped_max, 0444, memstat_show, NULL);
static DEVICE_ATTR_RW(full_cache_threshold);

static const struct attribute *drv_attr_list[] = {
	&dev_attr_vmalloc.attr,
	&dev_attr_vmalloc_max.attr,
	&dev_attr_page_alloc.attr,
	&dev_attr_page_alloc_max.attr,
	&dev_attr_secure.attr,
	&dev_attr_secure_max.attr,
	&dev_attr_mapped.attr,
	&dev_attr_mapped_max.attr,
	&dev_attr_full_cache_threshold.attr,
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
	struct page *page;
	const unsigned long offset = vmf->address - vma->vm_start;

	if (offset >= memdesc->size)
		return VM_FAULT_SIGBUS;

	page = kgsl_mmu_find_mapped_page(memdesc, offset);
	if (IS_ERR_OR_NULL(page))
		return VM_FAULT_SIGBUS;
	if (page == ZERO_PAGE(0))
		return VM_FAULT_SIGSEGV;

	/*
	 * If this is the first time this page is being mapped into userspace
	 * then bump the entry's mapsize.
	 */
	if (page_mapcount(page) == 0)
		atomic_long_add(PAGE_SIZE, &memdesc->mapsize);

	get_page(page);
	vmf->page = page;

	return 0;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)

#include <soc/qcom/secure_buffer.h>

static int lock_sgt(struct sg_table *sgt, u64 size)
{
	struct scatterlist *sg;
	int dest_perms = PERM_READ | PERM_WRITE;
	int source_vm = VMID_HLOS;
	int dest_vm = VMID_CP_PIXEL;
	int ret;
	int i;

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

	/* Set private bit for each sg to indicate that its secured */
	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		SetPagePrivate(sg_page(sg));

	return 0;
}

static int unlock_sgt(struct sg_table *sgt)
{
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int source_vm = VMID_CP_PIXEL;
	int dest_vm = VMID_HLOS;
	int ret;
	struct sg_page_iter sg_iter;

	do {
		ret = hyp_assign_table(sgt, &source_vm, 1, &dest_vm,
					&dest_perms, 1);
	} while (ret == -EAGAIN);

	if (ret)
		return ret;

	for_each_sg_page(sgt->sgl, &sg_iter, sgt->nents, 0)
		ClearPagePrivate(sg_page_iter_page(&sg_iter));
	return 0;
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

	if (memdesc->sgt != NULL) {
		struct sg_page_iter sg_iter;

		/*
		 * Account for potentially starting mid-page with the offset
		 * when calculating how many pages need to get maintained.
		 */
		size += offset & ~PAGE_MASK;

		for_each_sg_page(memdesc->sgt->sgl, &sg_iter,
				PAGE_ALIGN(size) >> PAGE_SHIFT,
				offset >> PAGE_SHIFT)
			_dma_cache_op(memdesc->dev, sg_page_iter_page(&sg_iter),
					PAGE_SIZE, op);
	} else {
		struct page **pages = NULL;
		unsigned int page_count = 0;
		unsigned int i;

		/* Get the pages backing this entry from the IOMMU. */
		pages = kgsl_mmu_find_mapped_page_range(memdesc, offset, size,
				&page_count);
		if (IS_ERR_OR_NULL(pages))
			return PTR_ERR(pages);

		for (i = 0; i < page_count;) {
			unsigned int num_pages;

			/* Skip over any unmapped/invalid pages. */
			if (IS_ERR_OR_NULL(pages[i]) || pages[i] == ZERO_PAGE(0)) {
				i++;
				continue;
			}

			num_pages = 1 << compound_order(pages[i]);
			_dma_cache_op(memdesc->dev, pages[i],
					min_t(size_t, num_pages, page_count - i)
					<< PAGE_SHIFT, op);
			i += num_pages;
		}

		kfree(pages);
	}

	return 0;
}

void kgsl_memdesc_init(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t flags)
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
	if (!kgsl_mmu_has_feature(device, KGSL_MMU_IO_COHERENT)) {
		flags &= ~((uint64_t) KGSL_MEMFLAGS_IOCOHERENT);

		WARN_ONCE(IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT),
			"I/O coherency is not supported on this target\n");
	} else if (IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT))
		flags |= KGSL_MEMFLAGS_IOCOHERENT;

	/*
	 * We can't enable I/O coherency on uncached surfaces because of
	 * situations where hardware might snoop the cpu caches which can
	 * have stale data. This happens primarily due to the limitations
	 * of dma caching APIs available on arm64
	 */
	if (!kgsl_cachemode_is_cached(flags))
		flags &= ~((u64) KGSL_MEMFLAGS_IOCOHERENT);

	if (kgsl_mmu_has_feature(device, KGSL_MMU_NEED_GUARD_PAGE))
		memdesc->priv |= KGSL_MEMDESC_GUARD_PAGE;

	if (flags & KGSL_MEMFLAGS_SECURE)
		memdesc->priv |= KGSL_MEMDESC_SECURE;

	memdesc->flags = flags;
	memdesc->dev = device->dev->parent;

	align = max_t(unsigned int,
		(memdesc->flags & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT,
		ilog2(PAGE_SIZE));
	kgsl_memdesc_set_align(memdesc, align);
	spin_lock_init(&memdesc->lock);

	atomic_long_set(&memdesc->physsize, 0);
	atomic_long_set(&memdesc->mapsize, 0);
}

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	struct kgsl_pagetable *pagetable;
	struct page **pages = NULL;
	u64 gpuaddr, physsize;
	unsigned int page_count = 0;
	bool fixed;

	if (!memdesc || !memdesc->size)
		return;

	physsize = atomic_long_read(&memdesc->physsize);
	fixed = (memdesc->priv & KGSL_MEMDESC_FIXED) != 0;

	/*
	 * kgsl_mmu_put_gpuaddr clears the pagetable and gpuaddr fields of
	 * (most) memory entries so we need to cache these values here first.
	 */
	pagetable = memdesc->pagetable;
	gpuaddr = memdesc->gpuaddr;

	/* Look up the backing pages of the entry (if applicable). */
	if (!fixed && !(memdesc->flags & KGSL_MEMFLAGS_USERMEM_MASK) &&
			memdesc->ops && memdesc->ops->free)
		pages = kgsl_mmu_get_backing_pages(memdesc, &page_count);

	/* Make sure the memory object has been unmapped */
	kgsl_mmu_put_gpuaddr(memdesc);

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
	if (memdesc->priv & KGSL_MEMDESC_SECURE) {
		int ret = unlock_sgt(memdesc->sgt);

		if (ret) {
			/*
			 * Unlock of the secure buffer failed. This buffer will
			 * be stuck in secure side forever and is unrecoverable.
			 * Give up on the buffer and don't return it to the
			 * pool.
			 */
			pr_err("kgsl: secure buf unlock failed: gpuaddr: %llx size: %llx ret: %d\n",
				gpuaddr, memdesc->size, ret);
			goto error;
		}

		atomic_long_sub(physsize, &kgsl_driver.stats.secure);
	} else
#endif
	if (!fixed && !(memdesc->flags & KGSL_MEMFLAGS_USERMEM_MASK))
		atomic_long_sub(physsize, &kgsl_driver.stats.page_alloc);

	if (!IS_ERR_OR_NULL(pages))
		memdesc->ops->free(pages, page_count);

error:
	if (!IS_ERR_OR_NULL(pages))
		kfree(pages);

	/* Let go of the additional PT ref taken when attached to a process */
	if (!IS_ERR_OR_NULL(pagetable) && !kgsl_mmu_is_global_pt(pagetable) &&
			!kgsl_memdesc_is_global(memdesc))
		kgsl_mmu_putpagetable(pagetable);

	if (!IS_ERR_OR_NULL(memdesc->sgt)) {
		sg_free_table(memdesc->sgt);
		kfree(memdesc->sgt);

		memdesc->sgt = NULL;
	}

	if (!IS_ERR_OR_NULL(memdesc->pages)) {
		memdesc->page_count = 0;
		kvfree(memdesc->pages);

		memdesc->pages = NULL;
	}
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
void kgsl_free_secure_page(struct page *page)
{
	struct sg_table sgt;
	struct scatterlist sgl;

	if (!page)
		return;

	sgt.sgl = &sgl;
	sgt.nents = 1;
	sgt.orig_nents = 1;
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);

	unlock_sgt(&sgt);
	__free_page(page);
}

struct page *kgsl_alloc_secure_page(void)
{
	struct page *page;
	struct sg_table sgt;
	struct scatterlist sgl;
	int status;

	page = alloc_page(GFP_KERNEL | __GFP_ZERO |
			__GFP_NORETRY | __GFP_HIGHMEM);
	if (!page)
		return NULL;

	sgt.sgl = &sgl;
	sgt.nents = 1;
	sgt.orig_nents = 1;
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);

	status = lock_sgt(&sgt, PAGE_SIZE);
	if (status) {
		if (status == -EADDRNOTAVAIL)
			return NULL;

		__free_page(page);
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

static void kgsl_free_system_pages(struct page **pages, unsigned int page_count)
{
	unsigned int i;

	for (i = 0; i < page_count; i++) {
		struct page *p = pages[i];

		/* Skip over unmapped/missing pages. */
		if (IS_ERR_OR_NULL(p) || p == ZERO_PAGE(0))
			continue;

		__free_page(p);
	}
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static struct kgsl_memdesc_ops kgsl_secure_system_ops = {
	.free = kgsl_free_system_pages,
	/* FIXME: Make sure vmflags / vmfault does the right thing here */
};

static struct kgsl_memdesc_ops kgsl_secure_pool_ops = {
	.free = kgsl_pool_free_pages,
	/* FIXME: Make sure vmflags / vmfault does the right thing here */
};
#endif

static struct kgsl_memdesc_ops kgsl_pool_ops = {
	.free = kgsl_pool_free_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_paged_vmfault,
};

static struct kgsl_memdesc_ops kgsl_system_ops = {
	.free = kgsl_free_system_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_paged_vmfault,
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
		gfp_t gfp = __GFP_ZERO | __GFP_HIGHMEM |
			GFP_KERNEL | __GFP_NORETRY;

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

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static int kgsl_alloc_secure_pages(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	struct page **pages;
	int count;
	struct sg_table *sgt;
	int ret;

	size = PAGE_ALIGN(size);

	if (!size || size > UINT_MAX)
		return -EINVAL;

	kgsl_memdesc_init(device, memdesc, flags);
	memdesc->priv |= priv;

	if (priv & KGSL_MEMDESC_SYSMEM) {
		memdesc->ops = &kgsl_secure_system_ops;
		count = kgsl_system_alloc_pages(size, &pages, device->dev);
	} else {
		memdesc->ops = &kgsl_secure_pool_ops;
		count = kgsl_pool_alloc_pages(size, &pages, device->dev);
	}

	if (count < 0)
		return count;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		kgsl_pool_free_pages(pages, count);
		return -ENOMEM;
	}

	ret = sg_alloc_table_from_pages(sgt, pages, count, 0, size, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		kgsl_pool_free_pages(pages, count);
		return ret;
	}

	/* Now that we've moved to a sg table don't need the pages anymore */
	kvfree(pages);

	ret = lock_sgt(sgt, size);
	if (ret) {
		if (ret != -EADDRNOTAVAIL)
			kgsl_pool_free_sgt(sgt);
		sg_free_table(sgt);
		kfree(sgt);
		return ret;
	}

	memdesc->sgt = sgt;
	memdesc->size = size;
	atomic_long_add((size_t)count << PAGE_SHIFT, &memdesc->physsize);

	KGSL_STATS_ADD(size, &kgsl_driver.stats.secure,
		&kgsl_driver.stats.secure_max);

	return 0;
}
#endif

static int kgsl_alloc_pages(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	struct page **pages;
	int count;

	size = PAGE_ALIGN(size);

	if (!size || size > UINT_MAX)
		return -EINVAL;

	kgsl_memdesc_init(device, memdesc, flags);
	memdesc->priv |= priv;

	if (priv & KGSL_MEMDESC_SYSMEM) {
		memdesc->ops = &kgsl_system_ops;
		count = kgsl_system_alloc_pages(size, &pages, device->dev);
	} else {
		memdesc->ops = &kgsl_pool_ops;
		count = kgsl_pool_alloc_pages(size, &pages, device->dev);
	}

	if (count < 0)
		return count;

	memdesc->pages = pages;
	memdesc->size = size;
	memdesc->page_count = count;
	atomic_long_add((size_t)count << PAGE_SHIFT, &memdesc->physsize);

	KGSL_STATS_ADD(size, &kgsl_driver.stats.page_alloc,
		&kgsl_driver.stats.page_alloc_max);

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

	kgsl_memdesc_init(device, &md->memdesc, 0);
	md->memdesc.priv = KGSL_MEMDESC_GLOBAL | KGSL_MEMDESC_FIXED;
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

	/* Allow kernel write access to global buffers by default. */
	priv |= KGSL_MEMDESC_KERNEL_RW;

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
	unsigned int i;

	/*
	 * vm_map_ram unfortunately can sleep, so don't use this in an atomic
	 * context or you might cause deadlocks.
	 */
	might_sleep();

	/* Error out if the entry does not allow write access to the kernel. */
	if (!(memdesc->priv & KGSL_MEMDESC_KERNEL_RW))
		return ERR_PTR(-EPERM);

	pages = kgsl_mmu_find_mapped_page_range(memdesc, offset, size,
			page_count);
	if (IS_ERR_OR_NULL(pages))
		return (void *)pages;

	/* Ensure that the full requested mapping is valid and writable. */
	for (i = 0; i < *page_count; i++) {
		if (IS_ERR_OR_NULL(pages[i]) || pages[i] == ZERO_PAGE(0)) {
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
