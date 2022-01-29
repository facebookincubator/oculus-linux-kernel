// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/compat.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/of_platform.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>

#include "adreno.h"
#include "kgsl_device.h"
#include "kgsl_iommu.h"
#include "kgsl_mmu.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"

#define _IOMMU_PRIV(_mmu) (&((_mmu)->priv.iommu))

#define KGSL_IOMMU_SPLIT_TABLE_BASE 0x0001ff8000000000ULL

/*
 * Flag to set SMMU memory attributes required to
 * enable system cache for GPU transactions.
 */
#ifndef IOMMU_USE_UPSTREAM_HINT
#define IOMMU_USE_UPSTREAM_HINT 0
#endif

#define KGSL_IOMMU_IDR1_OFFSET 0x24
#define IDR1_NUMPAGENDXB GENMASK(30, 28)
#define IDR1_PAGESIZE BIT(31)

static struct kgsl_mmu_pt_ops iommu_pt_ops;

/*
 * struct kgsl_iommu_addr_entry - entry in the kgsl_iommu_pt rbtree.
 * @base: starting virtual address of the entry
 * @size: size of the entry
 * @node: the rbtree node
 *
 */
struct kgsl_iommu_addr_entry {
	uint64_t base;
	uint64_t size;
	struct rb_node node;
};

static struct kmem_cache *addr_entry_cache;

static bool kgsl_iommu_split_tables_enabled(struct kgsl_mmu *mmu)
{
	bool enabled = test_bit(KGSL_MMU_SPLIT_TABLES_GC, &mmu->features);

	if (!IS_ERR_OR_NULL(mmu->lpac_pagetable))
		enabled &= test_bit(KGSL_MMU_SPLIT_TABLES_LPAC, &mmu->features);

	return enabled;
}

static bool kgsl_iommu_addr_is_global(struct kgsl_mmu *mmu, u64 addr)
{
	if (kgsl_iommu_split_tables_enabled(mmu))
		return (addr >= KGSL_IOMMU_SPLIT_TABLE_BASE);

	return ((addr >= KGSL_IOMMU_GLOBAL_MEM_BASE(mmu)) &&
		(addr < KGSL_IOMMU_GLOBAL_MEM_BASE(mmu) +
		 KGSL_IOMMU_GLOBAL_MEM_SIZE));
}

static void __iomem *kgsl_iommu_reg(struct kgsl_iommu_context *ctx,
		u32 offset)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(&ctx->kgsldev->mmu);

	if (WARN_ON(ctx->cb_num < 0))
		return NULL;

	if (!iommu->cb0_offset) {
		u32 reg =
			readl_relaxed(iommu->regbase + KGSL_IOMMU_IDR1_OFFSET);

		iommu->pagesize =
			FIELD_GET(IDR1_PAGESIZE, reg) ? SZ_64K : SZ_4K;

		/*
		 * The number of pages in the global address space or
		 * translation bank address space is 2^(NUMPAGENDXB + 1).
		 */
		iommu->cb0_offset = iommu->pagesize *
			(1 << (FIELD_GET(IDR1_NUMPAGENDXB, reg) + 1));
	}

	return (void __iomem *) (iommu->regbase + iommu->cb0_offset +
		(ctx->cb_num * iommu->pagesize) + offset);
}

static void KGSL_IOMMU_SET_CTX_REG_Q(struct kgsl_iommu_context *ctx, u32 offset,
		u64 val)
{
	void __iomem *addr = kgsl_iommu_reg(ctx, offset);

	writeq_relaxed(val, addr);
}

static u64 KGSL_IOMMU_GET_CTX_REG_Q(struct kgsl_iommu_context *ctx, u32 offset)
{
	void __iomem *addr = kgsl_iommu_reg(ctx, offset);

	return readq_relaxed(addr);
}

static void KGSL_IOMMU_SET_CTX_REG(struct kgsl_iommu_context *ctx, u32 offset,
		u32 val)
{
	void __iomem *addr = kgsl_iommu_reg(ctx, offset);

	writel_relaxed(val, addr);
}

static u32 KGSL_IOMMU_GET_CTX_REG(struct kgsl_iommu_context *ctx, u32 offset)
{
	void __iomem *addr = kgsl_iommu_reg(ctx, offset);

	return readl_relaxed(addr);
}

static void kgsl_iommu_unmap_globals(struct kgsl_mmu *mmu,
		struct kgsl_pagetable *pagetable)
{

	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct kgsl_global_memdesc *md;

	if (IS_ERR_OR_NULL(pagetable))
		return;

	if (!kgsl_mmu_is_global_pt(pagetable)
		&& kgsl_iommu_split_tables_enabled(mmu))
		return;

	list_for_each_entry(md, &device->globals, node) {
		if (md->memdesc.flags & KGSL_MEMFLAGS_SECURE)
			continue;

		kgsl_mmu_unmap(pagetable, &md->memdesc);
	}
}

static void kgsl_iommu_map_globals(struct kgsl_mmu *mmu,
		struct kgsl_pagetable *pagetable)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct kgsl_global_memdesc *md;

	if (IS_ERR_OR_NULL(pagetable))
		return;

	if (!kgsl_mmu_is_global_pt(pagetable)
		&& kgsl_iommu_split_tables_enabled(mmu)) {
		return;
	}

	list_for_each_entry(md, &device->globals, node) {
		if (md->memdesc.flags & KGSL_MEMFLAGS_SECURE)
			continue;

		kgsl_mmu_map(pagetable, &md->memdesc);
	}
}

static int kgsl_iommu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc);

static void kgsl_iommu_map_secure_global(struct kgsl_mmu *mmu,
		struct kgsl_memdesc *memdesc)
{
	if (IS_ERR_OR_NULL(mmu->securepagetable))
		return;

	if (!memdesc->gpuaddr) {
		int ret = kgsl_iommu_get_gpuaddr(mmu->securepagetable,
			memdesc);

		if (WARN_ON(ret))
			return;
	}

	/*
	 * Make sure secure global memory entries are always associated with
	 * the secure global pagetable. This is needed for the fault routine
	 * to behave correctly, otherwise it won't know which pagetable to
	 * look up pages from.
	 */
	memdesc->pagetable = mmu->securepagetable;

	kgsl_mmu_map(mmu->securepagetable, memdesc);
}

#define KGSL_GLOBAL_MEM_PAGES (KGSL_IOMMU_GLOBAL_MEM_SIZE >> PAGE_SHIFT)

static u64 global_get_offset(struct kgsl_device *device, u64 size,
		unsigned long priv)
{
	int start = 0, bit;

	if (!device->global_map) {
		device->global_map =
			kcalloc(BITS_TO_LONGS(KGSL_GLOBAL_MEM_PAGES),
			sizeof(unsigned long), GFP_KERNEL);
		if (!device->global_map)
			return (unsigned long) -ENOMEM;
	}

	if (priv & KGSL_MEMDESC_RANDOM) {
		u32 offset = KGSL_GLOBAL_MEM_PAGES - (size >> PAGE_SHIFT);

		start = get_random_int() % offset;
	}

	while (start >= 0) {
		bit = bitmap_find_next_zero_area(device->global_map,
			KGSL_GLOBAL_MEM_PAGES, start, size >> PAGE_SHIFT, 0);

		if (bit < KGSL_GLOBAL_MEM_PAGES)
			break;

		/*
		 * Later implementations might want to randomize this to reduce
		 * predictability
		 */
		start--;
	}

	if (WARN_ON(start < 0))
		return (unsigned long) -ENOMEM;

	bitmap_set(device->global_map, bit, size >> PAGE_SHIFT);

	return bit << PAGE_SHIFT;
}

static void kgsl_iommu_map_global(struct kgsl_mmu *mmu,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);

	if (memdesc->flags & KGSL_MEMFLAGS_SECURE) {
		kgsl_iommu_map_secure_global(mmu, memdesc);
		return;
	}

	if (!memdesc->gpuaddr) {
		u64 offset;
		u64 base;

		offset = global_get_offset(device, memdesc->size,
			memdesc->priv);

		if (IS_ERR_VALUE(offset))
			return;

		if (kgsl_iommu_split_tables_enabled(mmu))
			base = KGSL_IOMMU_SPLIT_TABLE_BASE;
		else
			base = KGSL_IOMMU_GLOBAL_MEM_BASE(mmu);

		memdesc->gpuaddr = base + offset;
	}

	/*
	 * Warn if a global is added after first per-process pagetables have
	 * been created since we do not go back and retroactively add the
	 * globals to existing pages
	 */
	WARN_ON(!kgsl_iommu_split_tables_enabled(mmu) && iommu->ppt_active);

	/*
	 * Make sure nonsecure global memory entries are always associated
	 * with the default global pagetable. This is needed for the fault
	 * routine to behave correctly, otherwise it won't know which
	 * pagetable to look up pages from.
	 */
	BUG_ON(IS_ERR_OR_NULL(mmu->defaultpagetable));
	memdesc->pagetable = mmu->defaultpagetable;

	kgsl_mmu_map(mmu->defaultpagetable, memdesc);
	if (!IS_ERR_OR_NULL(mmu->lpac_pagetable))
		kgsl_mmu_map(mmu->lpac_pagetable, memdesc);
}

static void _detach_pt(struct kgsl_iommu_pt *iommu_pt,
			  struct kgsl_iommu_context *ctx)
{
	if (iommu_pt->attached) {
		iommu_detach_device(iommu_pt->domain, &ctx->pdev->dev);
		iommu_pt->attached = false;
	}
}

static int _attach_pt(struct kgsl_iommu_pt *iommu_pt,
			struct kgsl_iommu_context *ctx)
{
	int ret;

	if (iommu_pt->attached)
		return 0;

	ret = iommu_attach_device(iommu_pt->domain, &ctx->pdev->dev);

	if (ret == 0)
		iommu_pt->attached = true;

	return ret;
}

static int _iommu_map_single_page(struct kgsl_pagetable *pt,
		uint64_t gpuaddr, phys_addr_t physaddr, int times,
		unsigned int flags)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(pt->mmu);
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	size_t mapped = 0;
	int i;
	int ret = 0;

	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (gpuaddr & (1ULL << 48))
		gpuaddr |= 0xffff000000000000;

	for (i = 0; i < times; i++) {
		ret = iommu_map(iommu_pt->domain, gpuaddr + mapped,
				physaddr, PAGE_SIZE, flags);
		if (ret)
			break;
		mapped += PAGE_SIZE;
	}

	if (ret)
		iommu_unmap(iommu_pt->domain, gpuaddr, mapped);

	if (ret) {
		dev_err(device->dev, "map err: 0x%016llX, 0x%lx, 0x%x, %d\n",
			gpuaddr, PAGE_SIZE * times, flags, ret);
		return -ENODEV;
	}

	return 0;
}

static int _iommu_unmap(struct kgsl_pagetable *pt,
		uint64_t addr, uint64_t size)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(pt->mmu);
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	size_t unmapped = 0;

	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (addr & (1ULL << 48))
		addr |= 0xffff000000000000;

	unmapped = iommu_unmap(iommu_pt->domain, addr, size);

	if (unmapped != size) {
		dev_err(device->dev, "unmap err: 0x%016llx, 0x%llx, %zd\n",
			addr, size, unmapped);
		return -ENODEV;
	}

	return 0;
}

static int _iommu_map_sg(struct kgsl_pagetable *pt,
		uint64_t addr, struct scatterlist *sg, int nents,
		unsigned int flags)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(pt->mmu);
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	size_t mapped;

	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (addr & (1ULL << 48))
		addr |= 0xffff000000000000;

	mapped = iommu_map_sg(iommu_pt->domain, addr, sg, nents, flags);

	if (mapped == 0) {
		dev_err(device->dev, "map sg err: 0x%016llX, %d, %x, %zd\n",
			addr, nents, flags, mapped);
		return  -ENODEV;
	}

	return 0;
}

/*
 * One page allocation for a guard region to protect against over-zealous
 * GPU pre-fetch
 */

static struct page *kgsl_secure_guard_page;

/* These functions help find the nearest allocated memory entries on either side
 * of a faulting address. If we know the nearby allocations memory we can
 * get a better determination of what we think should have been located in the
 * faulting region
 */

/*
 * A local structure to make it easy to store the interesting bits for the
 * memory entries on either side of the faulting address
 */

struct _mem_entry {
	uint64_t gpuaddr;
	uint64_t size;
	uint64_t flags;
	unsigned int priv;
	int pending_free;
	pid_t pid;
	char name[32];
};

void __kgsl_get_memory_usage(struct _mem_entry *entry)
{
	kgsl_get_memory_usage(entry->name, sizeof(entry->name), entry->flags);
}

static void _get_entries(struct kgsl_process_private *private,
		uint64_t faultaddr, struct _mem_entry *prev,
		struct _mem_entry *next)
{
	int id;
	struct kgsl_mem_entry *entry;

	uint64_t prevaddr = 0;
	struct kgsl_mem_entry *p = NULL;

	uint64_t nextaddr = (uint64_t) -1;
	struct kgsl_mem_entry *n = NULL;

	idr_for_each_entry(&private->mem_idr, entry, id) {
		uint64_t addr = entry->memdesc.gpuaddr;

		if ((addr < faultaddr) && (addr > prevaddr)) {
			prevaddr = addr;
			p = entry;
		}

		if ((addr > faultaddr) && (addr < nextaddr)) {
			nextaddr = addr;
			n = entry;
		}
	}

	if (p != NULL) {
		prev->gpuaddr = p->memdesc.gpuaddr;
		prev->size = p->memdesc.size;
		prev->flags = p->memdesc.flags;
		prev->priv = p->memdesc.priv;
		prev->pending_free = p->pending_free;
		prev->pid = pid_nr(private->pid);
		__kgsl_get_memory_usage(prev);
	}

	if (n != NULL) {
		next->gpuaddr = n->memdesc.gpuaddr;
		next->size = n->memdesc.size;
		next->flags = n->memdesc.flags;
		next->priv = n->memdesc.priv;
		next->pending_free = n->pending_free;
		next->pid = pid_nr(private->pid);
		__kgsl_get_memory_usage(next);
	}
}

static void _find_mem_entries(struct kgsl_mmu *mmu, uint64_t faultaddr,
		struct _mem_entry *preventry, struct _mem_entry *nextentry,
		struct kgsl_process_private *private)
{
	memset(preventry, 0, sizeof(*preventry));
	memset(nextentry, 0, sizeof(*nextentry));

	/* Set the maximum possible size as an initial value */
	nextentry->gpuaddr = (uint64_t) -1;

	if (private) {
		spin_lock(&private->mem_lock);
		_get_entries(private, faultaddr, preventry, nextentry);
		spin_unlock(&private->mem_lock);
	}
}

static void _print_entry(struct kgsl_device *device, struct _mem_entry *entry)
{
	dev_err(device->dev,
		      "[%016llX - %016llX] %s %s (pid = %d) (%s)\n",
		      entry->gpuaddr,
		      entry->gpuaddr + entry->size,
		      entry->priv & KGSL_MEMDESC_GUARD_PAGE ? "(+guard)" : "",
		      entry->pending_free ? "(pending free)" : "",
		      entry->pid, entry->name);
}

static void _check_if_freed(struct kgsl_iommu_context *ctx,
	uint64_t addr, pid_t ptname)
{
	uint64_t gpuaddr = addr;
	uint64_t size = 0;
	uint64_t flags = 0;
	pid_t pid;

	char name[32];

	memset(name, 0, sizeof(name));

	if (kgsl_memfree_find_entry(ptname, &gpuaddr, &size, &flags, &pid)) {
		kgsl_get_memory_usage(name, sizeof(name) - 1, flags);
		dev_err(ctx->kgsldev->dev, "---- premature free ----\n");
		dev_err(ctx->kgsldev->dev,
			      "[%8.8llX-%8.8llX] (%s) was already freed by pid %d\n",
			      gpuaddr, gpuaddr + size, name, pid);
	}
}

static struct kgsl_process_private *kgsl_iommu_get_process(u64 ptbase)
{
	struct kgsl_process_private *p;
	struct kgsl_iommu_pt *iommu_pt;

	spin_lock(&kgsl_driver.proclist_lock);

	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		iommu_pt = p->pagetable->priv;
		if (iommu_pt->ttbr0 == ptbase) {
			if (!kgsl_process_private_get(p))
				p = NULL;

			spin_unlock(&kgsl_driver.proclist_lock);
			return p;
		}
	}

	spin_unlock(&kgsl_driver.proclist_lock);

	return NULL;
}

static struct kgsl_iommu_context *
iommu_context_by_name(struct kgsl_iommu *iommu, u32 name)
{
	if (name == KGSL_MMU_SECURE_PT)
		return &iommu->secure_context;

	return &iommu->user_context;
}

static int kgsl_iommu_lpac_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags, void *token)
{
	struct kgsl_pagetable *pt = token;
	struct kgsl_mmu *mmu = pt->mmu;
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->lpac_context;
	u32 fsynr0, fsynr1;

	fsynr0 = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSYNR0);
	fsynr1 = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSYNR1);

	dev_crit(device->dev,
		"LPAC PAGE FAULT iova=0x%16lx, fsynr0=0x%x, fsynr1=0x%x\n",
		addr, fsynr0, fsynr1);

	return 0;
}

struct kgsl_pagefault_log {
	struct work_struct work;
	struct kgsl_mmu *mmu;
	struct kgsl_iommu_context *ctx;
	struct kgsl_process_private *private;
	unsigned long addr;
	int flags;
	pid_t ptname;
	u64 ptbase;
	u32 contextidr;
	char *fault_block;
};

static void _print_pagefault_log(struct work_struct *work)
{
	struct kgsl_pagefault_log *log = container_of(work,
			struct kgsl_pagefault_log, work);
	struct kgsl_device *device;
	struct kgsl_context *context;
	struct _mem_entry prev, next;
	const char *fault_type = "unknown";
	const char *comm = "unknown";
	int write;

	device = log->ctx->kgsldev;
	context = kgsl_context_get(device, log->contextidr);

	write = (log->flags & IOMMU_FAULT_WRITE) ? 1 : 0;
	if (log->flags & IOMMU_FAULT_TRANSLATION)
		fault_type = "translation";
	else if (log->flags & IOMMU_FAULT_PERMISSION)
		fault_type = "permission";
	else if (log->flags & IOMMU_FAULT_EXTERNAL)
		fault_type = "external";
	else if (log->flags & IOMMU_FAULT_TRANSACTION_STALLED)
		fault_type = "transaction stalled";

	if (log->private)
		comm = log->private->comm;

	console_lock();

	dev_crit(device->dev, "GPU PAGE FAULT: addr = %lX pid= %d name=%s drawctxt=%d context pid = %d\n",
			log->addr, log->ptname, comm, log->contextidr,
			context ? context->tid : 0);
	dev_crit(device->dev, "context=%s TTBR0=0x%llx (%s %s fault)\n",
			log->ctx->name, log->ptbase, write ? "write" : "read",
			fault_type);

	if (log->fault_block)
		dev_crit(device->dev, "FAULTING BLOCK: %s\n", log->fault_block);

	/* Don't print the debug if this is a permissions fault */
	if (!(log->flags & IOMMU_FAULT_PERMISSION)) {
		_check_if_freed(log->ctx, log->addr, log->ptname);

		/*
		 * Don't print any debug information if the address is
		 * in the global region. These are rare and nobody needs
		 * to know the addresses that are in here
		 */
		if (kgsl_iommu_addr_is_global(log->mmu, log->addr)) {
			dev_err(device->dev, "Fault in global memory\n");
		} else {
			_find_mem_entries(log->mmu, log->addr, &prev, &next,
					log->private);

			dev_err(device->dev, "---- nearby memory ----\n");

			if (prev.gpuaddr)
				_print_entry(device, &prev);
			else
				dev_err(device->dev, "*EMPTY*\n");

			dev_err(device->dev, " <- fault @ %8.8lX\n", log->addr);

			if (next.gpuaddr != (uint64_t) -1)
				_print_entry(device, &next);
			else
				dev_err(device->dev, "*EMPTY*\n");
		}
	}

	console_unlock();

	kgsl_process_private_put(log->private);
	kgsl_context_put(context);
	kfree(log->fault_block);
	kfree(log);
}

static int kgsl_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags, void *token)
{
	int ret = 0;
	struct kgsl_pagetable *pt = token;
	struct kgsl_mmu *mmu = pt->mmu;
	struct kgsl_iommu_context *ctx;
	u64 ptbase;
	u32 contextidr;
	pid_t pid = 0;
	pid_t ptname;
	int write;
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	struct adreno_gpudev *gpudev;
	unsigned int no_page_fault_log = 0;
	char *comm = "unknown";
	struct kgsl_process_private *private;

	if (mmu == NULL)
		return ret;

	device = KGSL_MMU_DEVICE(mmu);
	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	write = (flags & IOMMU_FAULT_WRITE) ? 1 : 0;

	ctx = iommu_context_by_name(_IOMMU_PRIV(mmu), pt->name);

	ptbase = KGSL_IOMMU_GET_CTX_REG_Q(ctx, KGSL_IOMMU_CTX_TTBR0);
	private = kgsl_iommu_get_process(ptbase);

	if (private) {
		pid = pid_nr(private->pid);
		comm = private->comm;
	}

	if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
		&adreno_dev->ft_pf_policy) &&
		(flags & IOMMU_FAULT_TRANSACTION_STALLED)) {
		/*
		 * Turn off GPU IRQ so we don't get faults from it too.
		 * The device mutex must be held to change power state
		 */
		mutex_lock(&device->mutex);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
		mutex_unlock(&device->mutex);
	}

	contextidr = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_CONTEXTIDR);
	ptname = test_bit(KGSL_MMU_GLOBAL_PAGETABLE, &mmu->features) ?
		KGSL_MMU_GLOBAL_PT : pid;
	/*
	 * Trace needs to be logged before searching the faulting
	 * address in free list as it takes quite long time in
	 * search and delays the trace unnecessarily.
	 */
	trace_kgsl_mmu_pagefault(ctx->kgsldev, addr,
			ptname, comm, write ? "write" : "read");

	if (test_bit(KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE,
		&adreno_dev->ft_pf_policy))
		no_page_fault_log = kgsl_mmu_log_fault_addr(mmu, ptbase, addr);

	/*
	 * We do not want the h/w to resume fetching data from an iommu
	 * that has faulted, this is better for debugging as it will stall
	 * the GPU and trigger a snapshot. Return EBUSY error.
	 */
	if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
		&adreno_dev->ft_pf_policy) &&
		(flags & IOMMU_FAULT_TRANSACTION_STALLED)) {
		uint32_t sctlr_val;

		ret = -EBUSY;
		/*
		 * Disable context fault interrupts
		 * as we do not clear FSR in the ISR.
		 * Will be re-enabled after FSR is cleared.
		 */
		sctlr_val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);
		sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFIE_SHIFT);
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr_val);

		/* This is used by reset/recovery path */
		ctx->stalled_on_fault = true;

		adreno_set_gpu_fault(adreno_dev, ADRENO_IOMMU_PAGE_FAULT);
		/* Go ahead with recovery*/
		adreno_dispatcher_schedule(device);
	}

	if (!no_page_fault_log) {
		struct kgsl_pagefault_log *log;

		log = kzalloc(sizeof(*log), GFP_KERNEL);
		if (log == NULL)
			goto log_failed;

		log->mmu = mmu;
		log->ctx = ctx;
		log->addr = addr;
		log->flags = flags;
		log->ptname = ptname;
		log->ptbase = ptbase;
		log->contextidr = contextidr;

		/* Need an extra reference to the private kobj (if it exists) */
		if (kgsl_process_private_get(private))
			log->private = private;

		if (gpudev->iommu_fault_block) {
			unsigned int fsynr1;

			fsynr1 = KGSL_IOMMU_GET_CTX_REG(ctx,
				KGSL_IOMMU_CTX_FSYNR1);
			log->fault_block = kstrdup(
				gpudev->iommu_fault_block(device, fsynr1),
				GFP_KERNEL);
		}

		INIT_WORK(&log->work, _print_pagefault_log);
		queue_work(system_unbound_wq, &log->work);
	}

log_failed:
	kgsl_process_private_put(private);

	return ret;
}

/*
 * kgsl_iommu_disable_clk() - Disable iommu clocks
 * Disable IOMMU clocks
 */
static void kgsl_iommu_disable_clk(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	int j;

	atomic_dec(&iommu->clk_enable_count);

	/*
	 * Make sure the clk refcounts are good. An unbalance may
	 * cause the clocks to be off when we need them on.
	 */
	WARN_ON(atomic_read(&iommu->clk_enable_count) < 0);

	for (j = (KGSL_IOMMU_MAX_CLKS - 1); j >= 0; j--)
		if (iommu->clks[j])
			clk_disable_unprepare(iommu->clks[j]);
}

/*
 * kgsl_iommu_enable_clk_prepare_enable - Enable the specified IOMMU clock
 * Try 4 times to enable it and then BUG() for debug
 */
static void kgsl_iommu_clk_prepare_enable(struct clk *clk)
{
	int num_retries = 4;

	while (num_retries--) {
		if (!clk_prepare_enable(clk))
			return;
	}

	WARN(1, "IOMMU clock enable failed\n");
}

/*
 * kgsl_iommu_enable_clk - Enable iommu clocks
 * Enable all the IOMMU clocks
 */
static void kgsl_iommu_enable_clk(struct kgsl_mmu *mmu)
{
	int j;
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);

	for (j = 0; j < KGSL_IOMMU_MAX_CLKS; j++) {
		if (iommu->clks[j])
			kgsl_iommu_clk_prepare_enable(iommu->clks[j]);
	}
	atomic_inc(&iommu->clk_enable_count);
}

/* kgsl_iommu_get_ttbr0 - Get TTBR0 setting for a pagetable */
static u64 kgsl_iommu_get_ttbr0(struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt = pt ? pt->priv : NULL;

	if (WARN_ON(!iommu_pt))
		return 0;

	return iommu_pt->ttbr0;
}

static bool kgsl_iommu_pt_equal(struct kgsl_mmu *mmu,
				struct kgsl_pagetable *pt,
				u64 ttbr0)
{
	struct kgsl_iommu_pt *iommu_pt = pt ? pt->priv : NULL;
	u64 domain_ttbr0;

	if (iommu_pt == NULL)
		return false;

	domain_ttbr0 = kgsl_iommu_get_ttbr0(pt);

	return (domain_ttbr0 == ttbr0);
}

/* kgsl_iommu_get_contextidr - query CONTEXTIDR setting for a pagetable */
static u32 kgsl_iommu_get_contextidr(struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt = pt ? pt->priv : NULL;

	if (WARN_ON(!iommu_pt))
		return 0;

	return iommu_pt->contextidr;
}

static int kgsl_iommu_get_context_bank(struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt;
	u32 cb_num;
	int ret;

	if (!pt)
		return -EINVAL;

	iommu_pt = pt->priv;

	ret = iommu_domain_get_attr(iommu_pt->domain,
				DOMAIN_ATTR_CONTEXT_BANK, &cb_num);
	if (ret)
		return ret;

	return (int) cb_num;
}

/*
 * kgsl_iommu_destroy_pagetable - Free up reaources help by a pagetable
 * @mmu_specific_pt - Pointer to pagetable which is to be freed
 *
 * Return - void
 */
static void kgsl_iommu_destroy_pagetable(struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	struct kgsl_mmu *mmu = pt->mmu;
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct kgsl_iommu_context  *ctx;

	/*
	 * Make sure all allocations are unmapped before destroying
	 * the pagetable
	 */
	WARN_ON(!list_empty(&pt->list));

	ctx = iommu_context_by_name(_IOMMU_PRIV(mmu), pt->name);

	if (pt->name == KGSL_MMU_SECURE_PT) {
		struct kgsl_global_memdesc *md;

		/* Unmap any pending secure global buffers */
		list_for_each_entry(md, &device->globals, node) {
			if (md->memdesc.flags & KGSL_MEMFLAGS_SECURE)
				kgsl_mmu_unmap(pt, &md->memdesc);
		}
	} else {
		kgsl_iommu_unmap_globals(mmu, pt);
	}

	if (iommu_pt->domain) {
		trace_kgsl_pagetable_destroy(iommu_pt->ttbr0, pt->name);

		_detach_pt(iommu_pt, ctx);

		iommu_domain_free(iommu_pt->domain);
	}

	kfree(iommu_pt);
}

static void setup_64bit_pagetable(struct kgsl_mmu *mmu,
		struct kgsl_pagetable *pagetable,
		struct kgsl_iommu_pt *pt)
{
	if (mmu->secured && pagetable->name == KGSL_MMU_SECURE_PT) {
		pt->compat_va_start = KGSL_IOMMU_SECURE_BASE(mmu);
		pt->compat_va_end = KGSL_IOMMU_SECURE_END(mmu);
		pt->va_start = KGSL_IOMMU_SECURE_BASE(mmu);
		pt->va_end = KGSL_IOMMU_SECURE_END(mmu);
	} else {
		pt->compat_va_start = KGSL_IOMMU_SVM_BASE32;
		pt->compat_va_end = KGSL_IOMMU_SECURE_BASE(mmu);
		pt->va_start = KGSL_IOMMU_VA_BASE64;
		pt->va_end = KGSL_IOMMU_VA_END64;
	}

	if (pagetable->name != KGSL_MMU_GLOBAL_PT &&
		pagetable->name != KGSL_MMU_SECURE_PT) {
		if (kgsl_is_compat_task()) {
			pt->svm_start = KGSL_IOMMU_SVM_BASE32;
			pt->svm_end = KGSL_IOMMU_SECURE_BASE(mmu);
		} else {
			pt->svm_start = KGSL_IOMMU_SVM_BASE64;
			pt->svm_end = KGSL_IOMMU_SVM_END64;
		}
	}
}

static void setup_32bit_pagetable(struct kgsl_mmu *mmu,
		struct kgsl_pagetable *pagetable,
		struct kgsl_iommu_pt *pt)
{
	if (mmu->secured) {
		if (pagetable->name == KGSL_MMU_SECURE_PT) {
			pt->compat_va_start = KGSL_IOMMU_SECURE_BASE(mmu);
			pt->compat_va_end = KGSL_IOMMU_SECURE_END(mmu);
			pt->va_start = KGSL_IOMMU_SECURE_BASE(mmu);
			pt->va_end = KGSL_IOMMU_SECURE_END(mmu);
		} else {
			pt->va_start = KGSL_IOMMU_SVM_BASE32;
			pt->va_end = KGSL_IOMMU_SECURE_BASE(mmu);
			pt->compat_va_start = pt->va_start;
			pt->compat_va_end = pt->va_end;
		}
	} else {
		pt->va_start = KGSL_IOMMU_SVM_BASE32;
		pt->va_end = KGSL_IOMMU_GLOBAL_MEM_BASE(mmu);
		pt->compat_va_start = pt->va_start;
		pt->compat_va_end = pt->va_end;
	}

	if (pagetable->name != KGSL_MMU_GLOBAL_PT &&
		pagetable->name != KGSL_MMU_SECURE_PT) {
		pt->svm_start = KGSL_IOMMU_SVM_BASE32;
		pt->svm_end = KGSL_IOMMU_SVM_END32;
	}
}


static struct kgsl_iommu_pt *
_alloc_pt(struct device *dev, struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt;
	struct bus_type *bus = kgsl_mmu_get_bus(dev);

	if (bus == NULL)
		return ERR_PTR(-ENODEV);

	iommu_pt = kzalloc(sizeof(struct kgsl_iommu_pt), GFP_KERNEL);
	if (iommu_pt == NULL)
		return ERR_PTR(-ENOMEM);

	iommu_pt->domain = iommu_domain_alloc(bus);
	if (iommu_pt->domain == NULL) {
		kfree(iommu_pt);
		return ERR_PTR(-ENODEV);
	}

	pt->pt_ops = &iommu_pt_ops;
	pt->priv = iommu_pt;
	pt->fault_addr = ~0ULL;
	iommu_pt->rbtree = RB_ROOT;

	if (test_bit(KGSL_MMU_64BIT, &mmu->features))
		setup_64bit_pagetable(mmu, pt, iommu_pt);
	else
		setup_32bit_pagetable(mmu, pt, iommu_pt);


	return iommu_pt;
}

static void _free_pt(struct kgsl_iommu_context *ctx, struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt = pt->priv;

	pt->pt_ops = NULL;
	pt->priv = NULL;

	if (iommu_pt == NULL)
		return;

	_detach_pt(iommu_pt, ctx);

	if (iommu_pt->domain != NULL)
		iommu_domain_free(iommu_pt->domain);
	kfree(iommu_pt);
}

void _enable_gpuhtw_llc(struct kgsl_mmu *mmu, struct iommu_domain *domain)
{
	int attr, ret;
	u32 val = 1;

	if (!test_bit(KGSL_MMU_LLCC_ENABLE, &mmu->features))
		return;

	if (mmu->subtype == KGSL_IOMMU_SMMU_V500)
		attr = DOMAIN_ATTR_USE_LLC_NWA;
	else
		attr = DOMAIN_ATTR_USE_UPSTREAM_HINT;

	ret = iommu_domain_set_attr(domain, attr, &val);

	/*
	 * Warn once if the system cache will not be used for GPU
	 * pagetable walks. This is not a fatal error.
	 */
	WARN_ONCE(ret, "System cache not enabled for GPU pagetable walks: %d\n",
		ret);
}

static bool check_split_tables(struct kgsl_iommu_pt *iommu_pt)
{
	int val, ret;

	ret = iommu_domain_get_attr(iommu_pt->domain,
		DOMAIN_ATTR_SPLIT_TABLES, &val);

	return (!ret && val == 1);
}

static int _init_global_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	int ret = 0, val = 1;
	struct kgsl_iommu_pt *iommu_pt = NULL;
	unsigned int cb_num;
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->user_context;

	iommu_pt = _alloc_pt(&ctx->pdev->dev, mmu, pt);

	if (IS_ERR(iommu_pt))
		return PTR_ERR(iommu_pt);

	if (kgsl_mmu_is_perprocess(mmu)) {
		ret = iommu_domain_set_attr(iommu_pt->domain,
				DOMAIN_ATTR_PROCID, &pt->name);
		if (ret) {
			dev_err(device->dev,
				"%s: set DOMAIN_ATTR_PROCID failed: %d\n",
				ctx->name, ret);
			goto done;
		}
	}

	_enable_gpuhtw_llc(mmu, iommu_pt->domain);

	if (test_bit(KGSL_MMU_64BIT, &mmu->features))
		iommu_domain_set_attr(iommu_pt->domain,
			DOMAIN_ATTR_SPLIT_TABLES, &val);

	ret = _attach_pt(iommu_pt, ctx);
	if (ret)
		goto done;

	iommu_set_fault_handler(iommu_pt->domain,
				kgsl_iommu_fault_handler, pt);

	ret = iommu_domain_get_attr(iommu_pt->domain,
				DOMAIN_ATTR_CONTEXT_BANK, &cb_num);
	if (ret) {
		dev_err(device->dev,
			"%s: get DOMAIN_ATTR_CONTEXT_BANK failed: %d\n",
			ctx->name, ret);
		goto done;
	}

	ctx->cb_num = (int) cb_num;

	if (kgsl_mmu_is_perprocess(mmu) && kgsl_mmu_has_feature(device,
				KGSL_MMU_SMMU_APERTURE)) {
		struct scm_desc desc = {0};

		desc.args[0] = 0xFFFF0000 | ((CP_APERTURE_REG & 0xff) << 8) |
			(cb_num & 0xff);

		desc.args[1] = 0xFFFFFFFF;
		desc.args[2] = 0xFFFFFFFF;
		desc.args[3] = 0xFFFFFFFF;
		desc.arginfo = SCM_ARGS(4);

		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP, CP_SMMU_APERTURE_ID),
			&desc);

		if (ret) {
			dev_err(device->dev,
				"SMMU aperture programming call failed with error %d\n",
				ret);
			goto done;
		}
	}

	ret = iommu_domain_get_attr(iommu_pt->domain,
			DOMAIN_ATTR_TTBR0, &iommu_pt->ttbr0);
	if (ret) {
		dev_err(device->dev, "%s: get DOMAIN_ATTR_TTBR0 failed: %d\n",
			ctx->name, ret);
		goto done;
	}
	ret = iommu_domain_get_attr(iommu_pt->domain,
			DOMAIN_ATTR_CONTEXTIDR, &iommu_pt->contextidr);
	if (ret) {
		dev_err(device->dev, "%s: get DOMAIN_ATTR_CONTEXTIDR failed: %d\n",
			ctx->name, ret);
		goto done;
	}

	if (check_split_tables(iommu_pt))
		set_bit(KGSL_MMU_SPLIT_TABLES_GC, &mmu->features);

done:
	if (ret)
		_free_pt(ctx, pt);

	return ret;
}

static int _init_global_lpac_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	int ret = 0, val = 1;
	struct kgsl_iommu_pt *iommu_pt = NULL;
	unsigned int cb_num;
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->lpac_context;

	iommu_pt = _alloc_pt(&ctx->pdev->dev, mmu, pt);

	if (IS_ERR(iommu_pt))
		return PTR_ERR(iommu_pt);

	_enable_gpuhtw_llc(mmu, iommu_pt->domain);

	if (test_bit(KGSL_MMU_64BIT, &mmu->features))
		iommu_domain_set_attr(iommu_pt->domain,
			DOMAIN_ATTR_SPLIT_TABLES, &val);

	ret = _attach_pt(iommu_pt, ctx);
	if (ret)
		goto done;

	iommu_set_fault_handler(iommu_pt->domain,
			kgsl_iommu_lpac_fault_handler, pt);

	/* Try to get the ID of the context bank */
	ret = iommu_domain_get_attr(iommu_pt->domain, DOMAIN_ATTR_CONTEXT_BANK,
		&cb_num);

	/*
	 * If this returns -ENODEV then we can't figure out what the current
	 * context bank is. This isn't fatal, but it does limit our ability to
	 * debug in a pagefault.
	 */
	if (ret) {
		if (ret != -ENODEV) {
			dev_err(device->dev,
				"%s: Unable to get DOMAIN_ATTR_CONTEXT_BANK: %d\n",
				ctx->name, ret);
			goto done;
		}

	}

	ctx->cb_num = (int) cb_num;

	if (check_split_tables(iommu_pt))
		set_bit(KGSL_MMU_SPLIT_TABLES_LPAC, &mmu->features);

done:
	if (ret)
		_free_pt(ctx, pt);

	return ret;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static int _init_secure_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	int ret = 0;
	struct kgsl_iommu_pt *iommu_pt = NULL;
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->secure_context;
	int secure_vmid = VMID_CP_PIXEL;

	if (!mmu->secured)
		return -EPERM;

	iommu_pt = _alloc_pt(&ctx->pdev->dev, mmu, pt);

	if (IS_ERR(iommu_pt))
		return PTR_ERR(iommu_pt);

	ret = iommu_domain_set_attr(iommu_pt->domain,
				    DOMAIN_ATTR_SECURE_VMID, &secure_vmid);
	if (ret) {
		dev_err(device->dev, "set DOMAIN_ATTR_SECURE_VMID failed: %d\n",
			ret);
		goto done;
	}

	_enable_gpuhtw_llc(mmu, iommu_pt->domain);

	ret = _attach_pt(iommu_pt, ctx);
	if (ret)
		goto done;

	iommu_set_fault_handler(iommu_pt->domain,
				kgsl_iommu_fault_handler, pt);

done:
	if (ret)
		_free_pt(ctx, pt);
	return ret;
}
#else
static int _init_secure_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	return -EPERM;
}
#endif

static int _init_per_process_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	int ret = 0;
	struct kgsl_iommu_pt *iommu_pt = NULL;
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->user_context;
	int dynamic = 1;
	unsigned int cb_num;

	if (ctx->cb_num < 0)
		return -EINVAL;

	iommu_pt = _alloc_pt(&ctx->pdev->dev, mmu, pt);

	if (IS_ERR(iommu_pt))
		return PTR_ERR(iommu_pt);

	ret = iommu_domain_set_attr(iommu_pt->domain,
				DOMAIN_ATTR_DYNAMIC, &dynamic);
	if (ret) {
		dev_err(device->dev,
			"set DOMAIN_ATTR_DYNAMIC failed: %d\n", ret);
		goto done;
	}

	cb_num = (unsigned int) ctx->cb_num;

	ret = iommu_domain_set_attr(iommu_pt->domain,
				DOMAIN_ATTR_CONTEXT_BANK, &cb_num);
	if (ret) {
		dev_err(device->dev,
			"set DOMAIN_ATTR_CONTEXT_BANK failed: %d\n", ret);
		goto done;
	}

	ret = iommu_domain_set_attr(iommu_pt->domain,
				DOMAIN_ATTR_PROCID, &pt->name);
	if (ret) {
		dev_err(device->dev,
			"set DOMAIN_ATTR_PROCID failed: %d\n", ret);
		goto done;
	}

	_enable_gpuhtw_llc(mmu, iommu_pt->domain);

	ret = _attach_pt(iommu_pt, ctx);
	if (ret)
		goto done;

	/* now read back the attributes needed for self programming */
	ret = iommu_domain_get_attr(iommu_pt->domain,
				DOMAIN_ATTR_TTBR0, &iommu_pt->ttbr0);
	if (ret) {
		dev_err(device->dev, "get DOMAIN_ATTR_TTBR0 failed: %d\n", ret);
		goto done;
	}

	ret = iommu_domain_get_attr(iommu_pt->domain,
				DOMAIN_ATTR_CONTEXTIDR, &iommu_pt->contextidr);
	if (ret) {
		dev_err(device->dev,
			"get DOMAIN_ATTR_CONTEXTIDR failed: %d\n", ret);
		goto done;
	}

	iommu->ppt_active = true;

	kgsl_iommu_map_globals(mmu, pt);

done:
	if (ret)
		_free_pt(ctx, pt);

	return ret;
}

/* kgsl_iommu_init_pt - Set up an IOMMU pagetable */
static int kgsl_iommu_init_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	if (pt == NULL)
		return -EINVAL;

	switch (pt->name) {
	case KGSL_MMU_GLOBAL_PT:
		return _init_global_pt(mmu, pt);

	case KGSL_MMU_SECURE_PT:
		return _init_secure_pt(mmu, pt);

	case KGSL_MMU_GLOBAL_LPAC_PT:
		return _init_global_lpac_pt(mmu, pt);

	default:
		return _init_per_process_pt(mmu, pt);
	}
}

static struct kgsl_pagetable *kgsl_iommu_getpagetable(struct kgsl_mmu *mmu,
		unsigned long name)
{
	struct kgsl_pagetable *pt;

	if (!kgsl_mmu_is_perprocess(mmu) && (name != KGSL_MMU_SECURE_PT) &&
		(name != KGSL_MMU_GLOBAL_LPAC_PT)) {
		name = KGSL_MMU_GLOBAL_PT;
		if (mmu->defaultpagetable != NULL)
			return mmu->defaultpagetable;
	}

	pt = kgsl_get_pagetable(name);
	if (pt == NULL)
		pt = kgsl_mmu_createpagetableobject(mmu, name);

	return pt;
}

static void _detach_context(struct kgsl_iommu_context *ctx)
{
	struct kgsl_iommu_pt *iommu_pt;

	if (ctx->default_pt == NULL)
		return;

	iommu_pt = ctx->default_pt->priv;

	_detach_pt(iommu_pt, ctx);

	ctx->default_pt = NULL;
	platform_device_put(ctx->pdev);

	ctx->pdev = NULL;
}

static void kgsl_iommu_close(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);

	_detach_context(&iommu->user_context);
	_detach_context(&iommu->lpac_context);
	_detach_context(&iommu->secure_context);

	kgsl_mmu_putpagetable(mmu->defaultpagetable);
	mmu->defaultpagetable = NULL;

	kgsl_mmu_putpagetable(mmu->lpac_pagetable);
	mmu->lpac_pagetable = NULL;

	kgsl_mmu_putpagetable(mmu->securepagetable);
	mmu->securepagetable = NULL;

	kgsl_free_secure_page(kgsl_secure_guard_page);
	kgsl_secure_guard_page = NULL;

	of_platform_depopulate(&iommu->pdev->dev);
	platform_device_put(iommu->pdev);

	kmem_cache_destroy(addr_entry_cache);
	addr_entry_cache = NULL;
}

static int _setup_user_context(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->user_context;
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_iommu_pt *iommu_pt;
	unsigned int  sctlr_val;

	if (IS_ERR_OR_NULL(mmu->defaultpagetable))
		return -ENODEV;

	iommu_pt = mmu->defaultpagetable->priv;
	if (WARN_ON(!iommu_pt->attached))
		return -ENODEV;

	ctx->default_pt = mmu->defaultpagetable;

	kgsl_iommu_enable_clk(mmu);

	sctlr_val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);

	/*
	 * If pagefault policy is GPUHALT_ENABLE,
	 * 1) Program CFCFG to 1 to enable STALL mode
	 * 2) Program HUPCF to 0 (Stall or terminate subsequent
	 *    transactions in the presence of an outstanding fault)
	 * else
	 * 1) Program CFCFG to 0 to disable STALL mode (0=Terminate)
	 * 2) Program HUPCF to 1 (Process subsequent transactions
	 *    independently of any outstanding fault)
	 */

	if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
				&adreno_dev->ft_pf_policy)) {
		sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
		sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
	} else {
		sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
		sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
	}
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr_val);
	kgsl_iommu_disable_clk(mmu);

	return 0;
}

static int _setup_lpac_context(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->lpac_context;
	struct kgsl_iommu_pt *iommu_pt;

	/* Sometimes LPAC doesn't exist and that's okay */
	if (IS_ERR_OR_NULL(mmu->lpac_pagetable))
		return 0;

	iommu_pt = mmu->lpac_pagetable->priv;
	if (WARN_ON(!iommu_pt->attached))
		return -ENODEV;

	ctx->default_pt = mmu->lpac_pagetable;

	return 0;
}

static int _setup_secure_context(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->secure_context;

	struct kgsl_iommu_pt *iommu_pt;

	if (!mmu->secured)
		return 0;

	if (IS_ERR_OR_NULL(mmu->securepagetable))
		return -ENODEV;

	iommu_pt = mmu->securepagetable->priv;
	if (WARN_ON(!iommu_pt->attached))
		return -ENODEV;

	ctx->default_pt = mmu->securepagetable;
	return 0;
}

static int kgsl_iommu_set_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt);

static int kgsl_iommu_start(struct kgsl_mmu *mmu)
{
	int status;
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);

	/* Set the following registers only when the MMU type is QSMMU */
	if (mmu->subtype != KGSL_IOMMU_SMMU_V500) {
		kgsl_iommu_enable_clk(mmu);

		/* Enable hazard check from GPU_SMMU_HUM_CFG */
		writel_relaxed(0x02, iommu->regbase + 0x6800);

		/* Write to GPU_SMMU_DORA_ORDERING to disable reordering */
		writel_relaxed(0x01, iommu->regbase + 0x64a0);

		/* make sure register write committed */
		wmb();

		kgsl_iommu_disable_clk(mmu);
	}

	status = _setup_user_context(mmu);
	if (status)
		return status;

	_setup_lpac_context(mmu);

	status = _setup_secure_context(mmu);
	if (status)
		return status;

	/* Make sure the hardware is programmed to the default pagetable */
	kgsl_iommu_set_pt(mmu, mmu->defaultpagetable);
	set_bit(KGSL_MMU_STARTED, &mmu->flags);
	return 0;
}

static int
kgsl_iommu_unmap(struct kgsl_pagetable *pt, struct kgsl_memdesc *memdesc)
{
	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return -EINVAL;

	return _iommu_unmap(pt, memdesc->gpuaddr,
			kgsl_memdesc_footprint(memdesc));
}

/**
 * _iommu_map_guard_page - Map iommu guard page
 * @pt - Pointer to kgsl pagetable structure
 * @memdesc - memdesc to add guard page
 * @gpuaddr - GPU addr of guard page
 * @protflags - flags for mapping
 *
 * Return 0 on success, error on map fail
 */
static int _iommu_map_guard_page(struct kgsl_pagetable *pt,
				   struct kgsl_memdesc *memdesc,
				   uint64_t gpuaddr,
				   unsigned int protflags)
{
	uint64_t pad_size;
	phys_addr_t physaddr;

	pad_size = kgsl_memdesc_footprint(memdesc) - memdesc->size;
	if (!pad_size)
		return 0;

	/*
	 * Allocate guard page for secure buffers.
	 * This has to be done after we attach a smmu pagetable.
	 * Allocate the guard page when first secure buffer is.
	 * mapped to save 1MB of memory if CPZ is not used.
	 */
	if (kgsl_memdesc_is_secured(memdesc)) {
		if (!kgsl_secure_guard_page) {
			kgsl_secure_guard_page = kgsl_alloc_secure_page();
			if (!kgsl_secure_guard_page) {
				dev_err(KGSL_MMU_DEVICE(pt->mmu)->dev,
					"Secure guard page alloc failed\n");
				return -ENOMEM;
			}
		}

		physaddr = page_to_phys(kgsl_secure_guard_page);
	} else
		physaddr = page_to_phys(ZERO_PAGE(0));

	protflags &= ~IOMMU_WRITE;

	return _iommu_map_single_page(pt, gpuaddr, physaddr,
			pad_size >> PAGE_SHIFT, protflags);
}

static unsigned int _get_protection_flags(struct kgsl_pagetable *pt,
	struct kgsl_memdesc *memdesc)
{
	unsigned int flags = IOMMU_READ | IOMMU_WRITE |
		IOMMU_NOEXEC;
	int ret, llc_nwa = 0, upstream_hint = 0;
	struct kgsl_iommu_pt *iommu_pt = pt->priv;

	ret = iommu_domain_get_attr(iommu_pt->domain,
				DOMAIN_ATTR_USE_UPSTREAM_HINT, &upstream_hint);

	if (!ret && upstream_hint)
		flags |= IOMMU_USE_UPSTREAM_HINT;

	ret = iommu_domain_get_attr(iommu_pt->domain,
				DOMAIN_ATTR_USE_LLC_NWA, &llc_nwa);

	if (!ret && llc_nwa)
		flags |= IOMMU_USE_LLC_NWA;

	if (memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY)
		flags &= ~IOMMU_WRITE;

	if (memdesc->priv & KGSL_MEMDESC_PRIVILEGED)
		flags |= IOMMU_PRIV;

	if (memdesc->flags & KGSL_MEMFLAGS_IOCOHERENT)
		flags |= IOMMU_CACHE;

	if (memdesc->priv & KGSL_MEMDESC_UCODE)
		flags &= ~IOMMU_NOEXEC;

	return flags;
}

static int
kgsl_iommu_map(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc)
{
	int ret;
	uint64_t addr = memdesc->gpuaddr;
	uint64_t size = memdesc->size;
	unsigned int flags = _get_protection_flags(pt, memdesc);
	struct sg_table *sgt = NULL;

	/*
	 * For paged memory allocated through kgsl, memdesc->pages is not NULL.
	 * Allocate sgt here just for its map operation. Contiguous memory
	 * already has its sgt, so no need to allocate it here.
	 */
	if (memdesc->pages != NULL)
		sgt = kgsl_alloc_sgt_from_pages(memdesc);
	else
		sgt = memdesc->sgt;

	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	ret = _iommu_map_sg(pt, addr, sgt->sgl, sgt->nents, flags);
	if (ret)
		goto done;

	ret = _iommu_map_guard_page(pt, memdesc, addr + size, flags);
	if (ret)
		_iommu_unmap(pt, addr, size);

done:
	if (memdesc->pages != NULL)
		kgsl_free_sgt(sgt);

	return ret;
}

static int kgsl_iommu_sparse_dummy_map(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, uint64_t offset, uint64_t size)
{
	int ret = 0, i;
	struct page **pages = NULL;
	struct sg_table sgt;
	int count = size >> PAGE_SHIFT;
	unsigned int map_flags;

	/* verify the offset is within our range */
	if (size + offset > kgsl_memdesc_footprint(memdesc))
		return -EINVAL;

	map_flags = IOMMU_READ | IOMMU_NOEXEC;

	pages = kcalloc(count, sizeof(struct page *), GFP_KERNEL);
	if (pages == NULL)
		return -ENOMEM;

	for (i = 0; i < count; i++)
		pages[i] = ZERO_PAGE(0);

	ret = sg_alloc_table_from_pages(&sgt, pages, count,
			0, size, GFP_KERNEL);
	if (ret == 0) {
		ret = _iommu_map_sg(pt, memdesc->gpuaddr + offset,
				sgt.sgl, sgt.nents, map_flags);
		sg_free_table(&sgt);
	}

	kfree(pages);

	return ret;
}

static struct page *kgsl_iommu_find_mapped_page(struct kgsl_memdesc *memdesc,
		uint64_t offset)
{
	struct kgsl_iommu_pt *iommu_pt = memdesc->pagetable->priv;
	phys_addr_t phys_addr;

	if (IS_ERR_OR_NULL(iommu_pt))
		return ERR_PTR(-ENODEV);

	phys_addr = iommu_iova_to_phys(iommu_pt->domain,
			memdesc->gpuaddr + offset);
	if (unlikely(!phys_addr) || IS_ERR_VALUE(phys_addr))
		return ERR_PTR(phys_addr);

	return phys_to_page(phys_addr);
}

static int kgsl_iommu_remap_page_range(struct kgsl_memdesc *memdesc,
		uint64_t offset, struct page **pages, unsigned int page_count)
{
	struct kgsl_pagetable *pagetable = memdesc->pagetable;
	struct kgsl_iommu_pt *iommu_pt = pagetable->priv;
	struct sg_table *sgt;
	uint64_t addr = memdesc->gpuaddr + offset;
	size_t unmapped, mapped;
	size_t size = (size_t)page_count << PAGE_SHIFT;
	unsigned int protflags = _get_protection_flags(pagetable, memdesc);
	int ret;

	if (size == 0 || (size + offset) > kgsl_memdesc_footprint(memdesc))
		return -EINVAL;

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (sgt == NULL)
		return -ENOMEM;

	ret = sg_alloc_table_from_pages(sgt, pages, page_count, 0, size,
			GFP_KERNEL);
	if (ret)
		goto fail_alloc;

	unmapped = iommu_unmap(iommu_pt->domain, addr, size);
	if (unmapped != size) {
		dev_err(memdesc->dev, "unmap err: 0x%016llx, %zd/%zd\n",
			addr, unmapped, size);
		ret = -EINVAL;
		goto fail_unmap;
	}

	mapped = iommu_map_sg(iommu_pt->domain, addr, sgt->sgl, sgt->nents,
			protflags);
	if (mapped != size) {
		dev_err(memdesc->dev, "map sg err: 0x%016llX, %d, %x, %zd/%zd\n",
			addr, sgt->nents, protflags, mapped, size);
		ret = -EINVAL;
	}

fail_unmap:
	sg_free_table(sgt);
fail_alloc:
	kfree(sgt);

	return ret;
}

static int kgsl_iommu_remap_page(struct kgsl_memdesc *memdesc, uint64_t offset,
		struct page *page)
{
	struct kgsl_pagetable *pagetable = memdesc->pagetable;
	struct kgsl_iommu_pt *iommu_pt = pagetable->priv;
	uint64_t addr = memdesc->gpuaddr + offset;
	size_t unmapped;
	unsigned int protflags = _get_protection_flags(pagetable, memdesc);
	int ret;

	if ((PAGE_SIZE + offset) > kgsl_memdesc_footprint(memdesc))
		return -EINVAL;

	unmapped = iommu_unmap(iommu_pt->domain, addr, PAGE_SIZE);
	if (unmapped != PAGE_SIZE) {
		dev_err(memdesc->dev, "unmap err: 0x%016llx, %zd/%zd\n",
			addr, unmapped, PAGE_SIZE);
		return -EINVAL;
	}

	ret = iommu_map(iommu_pt->domain, addr, page_to_phys(page), PAGE_SIZE,
			protflags);

	return ret;
}

/* This function must be called with context bank attached */
static void kgsl_iommu_clear_fsr(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context  *ctx = &iommu->user_context;
	unsigned int sctlr_val;

	if (ctx->default_pt != NULL && ctx->stalled_on_fault) {
		kgsl_iommu_enable_clk(mmu);
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSR, 0xffffffff);
		/*
		 * Re-enable context fault interrupts after clearing
		 * FSR to prevent the interrupt from firing repeatedly
		 */
		sctlr_val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);
		sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_CFIE_SHIFT);
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr_val);
		/*
		 * Make sure the above register writes
		 * are not reordered across the barrier
		 * as we use writel_relaxed to write them
		 */
		wmb();
		kgsl_iommu_disable_clk(mmu);
		ctx->stalled_on_fault = false;
	}
}

static void kgsl_iommu_pagefault_resume(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->user_context;

	if (ctx->default_pt != NULL && ctx->stalled_on_fault) {
		/*
		 * This will only clear fault bits in FSR. FSR.SS will still
		 * be set. Writing to RESUME (below) is the only way to clear
		 * FSR.SS bit.
		 */
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSR, 0xffffffff);
		/*
		 * Make sure the above register write is not reordered across
		 * the barrier as we use writel_relaxed to write it.
		 */
		wmb();

		/*
		 * Write 1 to RESUME.TnR to terminate the stalled transaction.
		 * This will also allow the SMMU to process new transactions.
		 */
		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_RESUME, 1);
		/*
		 * Make sure the above register writes are not reordered across
		 * the barrier as we use writel_relaxed to write them.
		 */
		wmb();
	}
}

static void kgsl_iommu_stop(struct kgsl_mmu *mmu)
{
	clear_bit(KGSL_MMU_STARTED, &mmu->flags);
}

static u64
kgsl_iommu_get_current_ttbr0(struct kgsl_mmu *mmu)
{
	u64 val;
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->user_context;

	/*
	 * We cannot enable or disable the clocks in interrupt context, this
	 * function is called from interrupt context if there is an axi error
	 */
	if (in_interrupt())
		return 0;

	if (ctx->cb_num < 0)
		return 0;

	kgsl_iommu_enable_clk(mmu);
	val = KGSL_IOMMU_GET_CTX_REG_Q(ctx, KGSL_IOMMU_CTX_TTBR0);
	kgsl_iommu_disable_clk(mmu);
	return val;
}

/*
 * kgsl_iommu_set_pt - Change the IOMMU pagetable of the primary context bank
 * @mmu - Pointer to mmu structure
 * @pt - Pagetable to switch to
 *
 * Set the new pagetable for the IOMMU by doing direct register writes
 * to the IOMMU registers through the cpu
 *
 * Return - void
 */
static int kgsl_iommu_set_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->user_context;
	uint64_t ttbr0, temp;
	unsigned int contextidr;
	unsigned long wait_for_flush;

	/* Not needed if split tables are enabled */
	if (kgsl_iommu_split_tables_enabled(mmu))
		return 0;

	if ((pt != mmu->defaultpagetable) && !kgsl_mmu_is_perprocess(mmu))
		return 0;

	kgsl_iommu_enable_clk(mmu);

	ttbr0 = kgsl_mmu_pagetable_get_ttbr0(pt);
	contextidr = kgsl_mmu_pagetable_get_contextidr(pt);

	KGSL_IOMMU_SET_CTX_REG_Q(ctx, KGSL_IOMMU_CTX_TTBR0, ttbr0);
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_CONTEXTIDR, contextidr);

	/* memory barrier before reading TTBR0 register */
	mb();
	temp = KGSL_IOMMU_GET_CTX_REG_Q(ctx, KGSL_IOMMU_CTX_TTBR0);

	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_TLBIALL, 1);
	/* make sure the TBLI write completes before we wait */
	mb();
	/*
	 * Wait for flush to complete by polling the flush
	 * status bit of TLBSTATUS register for not more than
	 * 2 s. After 2s just exit, at that point the SMMU h/w
	 * may be stuck and will eventually cause GPU to hang
	 * or bring the system down.
	 */
	wait_for_flush = jiffies + msecs_to_jiffies(2000);
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_TLBSYNC, 0);
	while (KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_TLBSTATUS) &
		(KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE)) {
		if (time_after(jiffies, wait_for_flush)) {
			dev_warn(KGSL_MMU_DEVICE(mmu)->dev,
				      "Wait limit reached for IOMMU tlb flush\n");
			break;
		}
		cpu_relax();
	}

	kgsl_iommu_disable_clk(mmu);
	return 0;
}

/*
 * kgsl_iommu_set_pf_policy() - Set the pagefault policy for IOMMU
 * @mmu: Pointer to mmu structure
 * @pf_policy: The pagefault polict to set
 *
 * Check if the new policy indicated by pf_policy is same as current
 * policy, if same then return else set the policy
 */
static int kgsl_iommu_set_pf_policy(struct kgsl_mmu *mmu,
				unsigned long pf_policy)
{
	struct kgsl_iommu *iommu = _IOMMU_PRIV(mmu);
	struct kgsl_iommu_context *ctx = &iommu->user_context;
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if ((adreno_dev->ft_pf_policy &
		BIT(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE)) ==
		(pf_policy & BIT(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE)))
		return 0;

	/* If not attached, policy will be updated during the next attach */
	if (ctx->default_pt != NULL) {
		unsigned int sctlr_val;

		kgsl_iommu_enable_clk(mmu);

		sctlr_val = KGSL_IOMMU_GET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR);

		if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &pf_policy)) {
			sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		} else {
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		}

		KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_SCTLR, sctlr_val);

		kgsl_iommu_disable_clk(mmu);
	}

	return 0;
}

static struct kgsl_iommu_addr_entry *_find_gpuaddr(
		struct kgsl_pagetable *pagetable, uint64_t gpuaddr)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node = pt->rbtree.rb_node;

	while (node != NULL) {
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		if (gpuaddr < entry->base)
			node = node->rb_left;
		else if (gpuaddr > entry->base)
			node = node->rb_right;
		else
			return entry;
	}

	return NULL;
}

static int _remove_gpuaddr(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct kgsl_iommu_addr_entry *entry;

	entry = _find_gpuaddr(pagetable, gpuaddr);

	if (WARN(!entry, "GPU address %llx doesn't exist\n", gpuaddr))
		return -ENOMEM;

	rb_erase(&entry->node, &pt->rbtree);
	kmem_cache_free(addr_entry_cache, entry);
	return 0;
}

static int _insert_gpuaddr(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node **node, *parent = NULL;
	struct kgsl_iommu_addr_entry *new =
		kmem_cache_alloc(addr_entry_cache, GFP_ATOMIC);

	if (new == NULL)
		return -ENOMEM;

	new->base = gpuaddr;
	new->size = size;

	node = &pt->rbtree.rb_node;

	while (*node != NULL) {
		struct kgsl_iommu_addr_entry *this;

		parent = *node;
		this = rb_entry(parent, struct kgsl_iommu_addr_entry, node);

		if (new->base < this->base)
			node = &parent->rb_left;
		else if (new->base > this->base)
			node = &parent->rb_right;
		else {
			/* Duplicate entry */
			WARN(1, "duplicate gpuaddr: 0x%llx\n", gpuaddr);
			kmem_cache_free(addr_entry_cache, new);
			return -EEXIST;
		}
	}

	rb_link_node(&new->node, parent, node);
	rb_insert_color(&new->node, &pt->rbtree);

	return 0;
}

static uint64_t _get_unmapped_area(struct kgsl_pagetable *pagetable,
		uint64_t bottom, uint64_t top, uint64_t size,
		uint64_t align)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node = rb_first(&pt->rbtree);
	uint64_t start;

	bottom = ALIGN(bottom, align);
	start = bottom;

	while (node != NULL) {
		uint64_t gap;
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		/*
		 * Skip any entries that are outside of the range, but make sure
		 * to account for some that might straddle the lower bound
		 */
		if (entry->base < bottom) {
			if (entry->base + entry->size > bottom)
				start = ALIGN(entry->base + entry->size, align);
			node = rb_next(node);
			continue;
		}

		/* Stop if we went over the top */
		if (entry->base >= top)
			break;

		/* Make sure there is a gap to consider */
		if (start < entry->base) {
			gap = entry->base - start;

			if (gap >= size)
				return start;
		}

		/* Stop if there is no more room in the region */
		if (entry->base + entry->size >= top)
			return (uint64_t) -ENOMEM;

		/* Start the next cycle at the end of the current entry */
		start = ALIGN(entry->base + entry->size, align);
		node = rb_next(node);
	}

	if (start + size <= top)
		return start;

	return (uint64_t) -ENOMEM;
}

static uint64_t _get_unmapped_area_topdown(struct kgsl_pagetable *pagetable,
		uint64_t bottom, uint64_t top, uint64_t size,
		uint64_t align)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node = rb_last(&pt->rbtree);
	uint64_t end = top;
	uint64_t mask = ~(align - 1);
	struct kgsl_iommu_addr_entry *entry;

	/* Make sure that the bottom is correctly aligned */
	bottom = ALIGN(bottom, align);

	/* Make sure the requested size will fit in the range */
	if (size > (top - bottom))
		return -ENOMEM;

	/* Walk back through the list to find the highest entry in the range */
	for (node = rb_last(&pt->rbtree); node != NULL; node = rb_prev(node)) {
		entry = rb_entry(node, struct kgsl_iommu_addr_entry, node);
		if (entry->base < top)
			break;
	}

	while (node != NULL) {
		uint64_t offset;

		entry = rb_entry(node, struct kgsl_iommu_addr_entry, node);

		/* If the entire entry is below the range the search is over */
		if ((entry->base + entry->size) < bottom)
			break;

		/* Get the top of the entry properly aligned */
		offset = ALIGN(entry->base + entry->size, align);

		/*
		 * Try to allocate the memory from the top of the gap,
		 * making sure that it fits between the top of this entry and
		 * the bottom of the previous one
		 */

		if ((end > size) && (offset < end)) {
			uint64_t chunk = (end - size) & mask;

			if (chunk >= offset)
				return chunk;
		}

		/*
		 * If we get here and the current entry is outside of the range
		 * then we are officially out of room
		 */

		if (entry->base < bottom)
			return (uint64_t) -ENOMEM;

		/* Set the top of the gap to the current entry->base */
		end = entry->base;

		/* And move on to the next lower entry */
		node = rb_prev(node);
	}

	/* If we get here then there are no more entries in the region */
	if ((end > size) && (((end - size) & mask) >= bottom))
		return (end - size) & mask;

	return (uint64_t) -ENOMEM;
}

static uint64_t kgsl_iommu_find_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t start, uint64_t end, uint64_t size,
		uint64_t alignment)
{
	uint64_t addr;

	/* Avoid black holes */
	if (WARN(end <= start, "Bad search range: 0x%llx-0x%llx", start, end))
		return (uint64_t) -EINVAL;

	spin_lock(&pagetable->lock);
	addr = _get_unmapped_area_topdown(pagetable,
			start, end, size, alignment);
	spin_unlock(&pagetable->lock);
	return addr;
}

static bool iommu_addr_in_svm_ranges(struct kgsl_iommu_pt *pt,
	u64 gpuaddr, u64 size)
{
	if ((gpuaddr >= pt->compat_va_start && gpuaddr < pt->compat_va_end) &&
		((gpuaddr + size) > pt->compat_va_start &&
			(gpuaddr + size) <= pt->compat_va_end))
		return true;

	if ((gpuaddr >= pt->svm_start && gpuaddr < pt->svm_end) &&
		((gpuaddr + size) > pt->svm_start &&
			(gpuaddr + size) <= pt->svm_end))
		return true;

	return false;
}

static int kgsl_iommu_set_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	int ret = -ENOMEM;
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node;

	/* Make sure the requested address doesn't fall out of SVM range */
	if (!iommu_addr_in_svm_ranges(pt, gpuaddr, size))
		return -ENOMEM;

	spin_lock(&pagetable->lock);
	node = pt->rbtree.rb_node;

	while (node != NULL) {
		uint64_t start, end;
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		start = entry->base;
		end = entry->base + entry->size;

		if (gpuaddr  + size <= start)
			node = node->rb_left;
		else if (end <= gpuaddr)
			node = node->rb_right;
		else
			goto out;
	}

	ret = _insert_gpuaddr(pagetable, gpuaddr, size);
out:
	spin_unlock(&pagetable->lock);
	return ret;
}

static int get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc, u64 start, u64 end,
		u64 size, unsigned int align)
{
	u64 addr;
	int ret;

	spin_lock(&pagetable->lock);
	addr = _get_unmapped_area(pagetable, start, end, size, align);
	if (addr == (u64) -ENOMEM) {
		spin_unlock(&pagetable->lock);
		return -ENOMEM;
	}

	/*
	 * This path is only called in a non-SVM path with locks so we can be
	 * sure we aren't racing with anybody so we don't need to worry about
	 * taking the lock
	 */
	ret = _insert_gpuaddr(pagetable, addr, size);
	spin_unlock(&pagetable->lock);

	if (ret == 0) {
		memdesc->gpuaddr = addr;
		memdesc->pagetable = pagetable;
	}

	return ret;
}

static int kgsl_iommu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	int ret = 0;
	u64 start, end, size;
	unsigned int align;

	if (WARN_ON(kgsl_memdesc_use_cpu_map(memdesc)))
		return -EINVAL;

	if (memdesc->flags & KGSL_MEMFLAGS_SECURE &&
			pagetable->name != KGSL_MMU_SECURE_PT)
		return -EINVAL;

	size = kgsl_memdesc_footprint(memdesc);

	align = max_t(uint64_t, 1 << kgsl_memdesc_get_align(memdesc),
			PAGE_SIZE);

	if (memdesc->flags & KGSL_MEMFLAGS_FORCE_32BIT) {
		start = pt->compat_va_start;
		end = pt->compat_va_end;
	} else {
		start = pt->va_start;
		end = pt->va_end;
	}

	ret = get_gpuaddr(pagetable, memdesc, start, end, size, align);
	/* if OoM, retry once after flushing mem_wq */
	if (ret == -ENOMEM) {
		flush_workqueue(kgsl_driver.mem_workqueue);
		ret = get_gpuaddr(pagetable, memdesc, start, end, size, align);
	}

	return ret;
}

static void kgsl_iommu_put_gpuaddr(struct kgsl_memdesc *memdesc)
{
	if (memdesc->pagetable == NULL)
		return;

	spin_lock(&memdesc->pagetable->lock);

	_remove_gpuaddr(memdesc->pagetable, memdesc->gpuaddr);

	spin_unlock(&memdesc->pagetable->lock);
}

static int kgsl_iommu_svm_range(struct kgsl_pagetable *pagetable,
		uint64_t *lo, uint64_t *hi, uint64_t memflags)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	bool gpu_compat = (memflags & KGSL_MEMFLAGS_FORCE_32BIT) != 0;

	if (lo != NULL)
		*lo = gpu_compat ? pt->compat_va_start : pt->svm_start;
	if (hi != NULL)
		*hi = gpu_compat ? pt->compat_va_end : pt->svm_end;

	return 0;
}

static bool kgsl_iommu_addr_in_range(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;

	if (gpuaddr == 0)
		return false;

	if (gpuaddr >= pt->va_start && gpuaddr < pt->va_end)
		return true;

	if (gpuaddr >= pt->compat_va_start && gpuaddr < pt->compat_va_end)
		return true;

	if (gpuaddr >= pt->svm_start && gpuaddr < pt->svm_end)
		return true;

	return false;
}

static int kgsl_iommu_probe_child(struct kgsl_device *device,
		struct device_node *parent, struct kgsl_iommu_context *context,
		const char *name)
{
	struct device_node *node = of_find_node_by_name(parent, name);
	struct platform_device *pdev;
	struct device_node *phandle;

	if (!node)
		return -ENOENT;

	pdev = of_find_device_by_node(node);

	context->cb_num = -1;
	context->name = name;
	context->kgsldev = device;
	context->pdev = pdev;

	phandle = of_parse_phandle(node, "iommus", 0);

	if (phandle && of_device_is_compatible(phandle, "qcom,qsmmu-v500"))
		device->mmu.subtype = KGSL_IOMMU_SMMU_V500;

	of_node_put(phandle);

	of_dma_configure(&pdev->dev, node, true);

	of_node_put(node);
	return 0;
}

static void iommu_probe_lpac_context(struct kgsl_device *device,
		struct device_node *node)
{
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct kgsl_mmu *mmu = &device->mmu;
	int ret;

	/* Get the sub device for the IOMMU context */
	ret = kgsl_iommu_probe_child(device, node, &iommu->lpac_context,
		"gfx3d_lpac");
	if (ret)
		return;

	mmu->lpac_pagetable = kgsl_mmu_getpagetable(mmu,
		KGSL_MMU_GLOBAL_LPAC_PT);
}

static int iommu_probe_user_context(struct kgsl_device *device,
		struct device_node *node)
{
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct kgsl_mmu *mmu = &device->mmu;
	int ret;

	/* Get the sub device for the IOMMU context */
	ret = kgsl_iommu_probe_child(device, node, &iommu->user_context,
		"gfx3d_user");
	if (ret)
		return ret;

	mmu->defaultpagetable = kgsl_mmu_getpagetable(mmu, KGSL_MMU_GLOBAL_PT);

	return PTR_ERR_OR_ZERO(mmu->defaultpagetable);
}

static void iommu_probe_secure_context(struct kgsl_device *device,
		struct device_node *node)
{
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct kgsl_mmu *mmu = &device->mmu;
	const char *name = "gfx3d_secure";

	if (!mmu->secured)
		return;

	if (test_bit(KGSL_MMU_SECURE_CB_ALT, &mmu->features))
		name = "gfx3d_secure_alt";

	if (kgsl_iommu_probe_child(device, node, &iommu->secure_context,
		name)) {
		mmu->secured = false;
		return;
	}

	mmu->securepagetable = kgsl_mmu_getpagetable(mmu, KGSL_MMU_SECURE_PT);
	if (IS_ERR(mmu->securepagetable))
		mmu->secured = false;
}

static struct kgsl_mmu_ops kgsl_iommu_ops;

int kgsl_iommu_probe(struct kgsl_device *device)
{
	const char *cname;
	struct property *prop;
	u32 val[2];
	int i = 0, ret;
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct platform_device *pdev;
	struct kgsl_mmu *mmu = &device->mmu;
	struct device_node *node;

	node = of_find_compatible_node(device->pdev->dev.of_node, NULL,
			"qcom,kgsl-smmu-v2");
	if (IS_ERR_OR_NULL(node))
		return -ENODEV;

	/* Create a kmem cache for the pagetable address objects */
	if (!addr_entry_cache) {
		addr_entry_cache = KMEM_CACHE(kgsl_iommu_addr_entry, 0);
		if (!addr_entry_cache) {
			ret = -ENOMEM;
			goto err;
		}
	}

	ret = of_property_read_u32_array(node, "reg", val, 2);
	if (ret) {
		dev_err(device->dev,
			"%pOF: Unable to read KGSL IOMMU register range\n",
			node);
		goto err;
	}

	iommu->regbase = devm_ioremap(&device->pdev->dev, val[0], val[1]);
	if (!iommu->regbase) {
		dev_err(&device->pdev->dev, "Couldn't map IOMMU registers\n");
		ret = -ENOMEM;
		goto err;
	}

	pdev = of_find_device_by_node(node);
	iommu->pdev = pdev;

	of_property_for_each_string(node, "clock-names", prop, cname) {
		struct clk *c = devm_clk_get(&pdev->dev, cname);

		if (IS_ERR(c)) {
			dev_err(device->dev,
				"dt: Couldn't get clock: %s\n", cname);
			platform_device_put(pdev);
			goto err;
		}
		if (i >= KGSL_IOMMU_MAX_CLKS) {
			dev_err(device->dev, "dt: too many clocks defined.\n");
			platform_device_put(pdev);
			goto err;
		}

		iommu->clks[i] = c;
		++i;
	}

	set_bit(KGSL_MMU_PAGED, &mmu->features);

	mmu->mmu_ops = &kgsl_iommu_ops;

	if (of_property_read_bool(node, "qcom,global_pt"))
		set_bit(KGSL_MMU_GLOBAL_PAGETABLE, &mmu->features);

	if (of_property_read_u32(node, "qcom,secure_align_mask",
		&device->mmu.secure_align_mask))
		device->mmu.secure_align_mask = 0xfff;

	/* Fill out the rest of the devices in the node */
	of_platform_populate(node, NULL, NULL, &pdev->dev);

	/* Probe the default pagetable */
	ret = iommu_probe_user_context(device, node);
	if (ret) {
		of_platform_depopulate(&pdev->dev);
		platform_device_put(pdev);
		goto err;
	}

	/* Probe LPAC (this is optional) */
	iommu_probe_lpac_context(device, node);

	/* Probe the secure pagetable (this is optional) */
	iommu_probe_secure_context(device, node);

	of_node_put(node);

#ifdef CONFIG_MSM_REMOTEQDSS
	device->qdss_desc = kgsl_allocate_global_fixed(device,
		"qcom,gpu-qdss-stm", "gpu-qdss");
#endif

	device->qtimer_desc = kgsl_allocate_global_fixed(device,
		"qcom,gpu-timer", "gpu-qtimer");

	return 0;

err:
	kmem_cache_destroy(addr_entry_cache);
	addr_entry_cache = NULL;

	of_node_put(node);
	return ret;
}

static struct kgsl_mmu_ops kgsl_iommu_ops = {
	.mmu_close = kgsl_iommu_close,
	.mmu_start = kgsl_iommu_start,
	.mmu_stop = kgsl_iommu_stop,
	.mmu_set_pt = kgsl_iommu_set_pt,
	.mmu_clear_fsr = kgsl_iommu_clear_fsr,
	.mmu_get_current_ttbr0 = kgsl_iommu_get_current_ttbr0,
	.mmu_enable_clk = kgsl_iommu_enable_clk,
	.mmu_disable_clk = kgsl_iommu_disable_clk,
	.mmu_pt_equal = kgsl_iommu_pt_equal,
	.mmu_set_pf_policy = kgsl_iommu_set_pf_policy,
	.mmu_pagefault_resume = kgsl_iommu_pagefault_resume,
	.mmu_init_pt = kgsl_iommu_init_pt,
	.mmu_getpagetable = kgsl_iommu_getpagetable,
	.mmu_map_global = kgsl_iommu_map_global,
};

static struct kgsl_mmu_pt_ops iommu_pt_ops = {
	.mmu_map = kgsl_iommu_map,
	.mmu_unmap = kgsl_iommu_unmap,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_pagetable,
	.get_ttbr0 = kgsl_iommu_get_ttbr0,
	.get_contextidr = kgsl_iommu_get_contextidr,
	.get_context_bank = kgsl_iommu_get_context_bank,
	.get_gpuaddr = kgsl_iommu_get_gpuaddr,
	.put_gpuaddr = kgsl_iommu_put_gpuaddr,
	.set_svm_region = kgsl_iommu_set_svm_region,
	.find_svm_region = kgsl_iommu_find_svm_region,
	.svm_range = kgsl_iommu_svm_range,
	.addr_in_range = kgsl_iommu_addr_in_range,
	.mmu_sparse_dummy_map = kgsl_iommu_sparse_dummy_map,
	.mmu_find_mapped_page = kgsl_iommu_find_mapped_page,
	.mmu_remap_page_range = kgsl_iommu_remap_page_range,
	.mmu_remap_page = kgsl_iommu_remap_page,
};
