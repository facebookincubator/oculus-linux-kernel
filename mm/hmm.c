/*
 * Copyright 2013 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Authors: JÃ©rÃ´me Glisse <jglisse@redhat.com>
 */
/*
 * Refer to include/linux/hmm.h for information about heterogeneous memory
 * management or HMM for short.
 */
#include <linux/mm.h>
#include <linux/hmm.h>
#include <linux/init.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mmzone.h>
#include <linux/pagemap.h>
#include <linux/swapops.h>
#include <linux/hugetlb.h>
#include <linux/memremap.h>
#include <linux/jump_label.h>
#include <linux/mmu_notifier.h>
#include <linux/memory_hotplug.h>

#define PA_SECTION_SIZE (1UL << PA_SECTION_SHIFT)

#if IS_ENABLED(CONFIG_HMM_MIRROR)
static const struct mmu_notifier_ops hmm_mmu_notifier_ops;

/*
 * struct hmm - HMM per mm struct
 *
 * @mm: mm struct this HMM struct is bound to
 * @lock: lock protecting ranges list
 * @sequence: we track updates to the CPU page table with a sequence number
 * @ranges: list of range being snapshotted
 * @mirrors: list of mirrors for this mm
 * @mmu_notifier: mmu notifier to track updates to CPU page table
 * @mirrors_sem: read/write semaphore protecting the mirrors list
 */
struct hmm {
	struct mm_struct	*mm;
	spinlock_t		lock;
	atomic_t		sequence;
	struct list_head	ranges;
	struct list_head	mirrors;
	struct mmu_notifier	mmu_notifier;
	struct rw_semaphore	mirrors_sem;
};

/*
 * hmm_register - register HMM against an mm (HMM internal)
 *
 * @mm: mm struct to attach to
 *
 * This is not intended to be used directly by device drivers. It allocates an
 * HMM struct if mm does not have one, and initializes it.
 */
static struct hmm *hmm_register(struct mm_struct *mm)
{
	struct hmm *hmm = READ_ONCE(mm->hmm);
	bool cleanup = false;

	/*
	 * The hmm struct can only be freed once the mm_struct goes away,
	 * hence we should always have pre-allocated an new hmm struct
	 * above.
	 */
	if (hmm)
		return hmm;

	hmm = kmalloc(sizeof(*hmm), GFP_KERNEL);
	if (!hmm)
		return NULL;
	INIT_LIST_HEAD(&hmm->mirrors);
	init_rwsem(&hmm->mirrors_sem);
	atomic_set(&hmm->sequence, 0);
	hmm->mmu_notifier.ops = NULL;
	INIT_LIST_HEAD(&hmm->ranges);
	spin_lock_init(&hmm->lock);
	hmm->mm = mm;

	spin_lock(&mm->page_table_lock);
	if (!mm->hmm)
		mm->hmm = hmm;
	else
		cleanup = true;
	spin_unlock(&mm->page_table_lock);

	if (cleanup)
		goto error;

	/*
	 * We should only get here if hold the mmap_sem in write mode ie on
	 * registration of first mirror through hmm_mirror_register()
	 */
	hmm->mmu_notifier.ops = &hmm_mmu_notifier_ops;
	if (__mmu_notifier_register(&hmm->mmu_notifier, mm))
		goto error_mm;

	return mm->hmm;

error_mm:
	spin_lock(&mm->page_table_lock);
	if (mm->hmm == hmm)
		mm->hmm = NULL;
	spin_unlock(&mm->page_table_lock);
error:
	kfree(hmm);
	return NULL;
}

void hmm_mm_destroy(struct mm_struct *mm)
{
	kfree(mm->hmm);
}

static void hmm_invalidate_range(struct hmm *hmm,
				 enum hmm_update_type action,
				 unsigned long start,
				 unsigned long end)
{
	struct hmm_mirror *mirror;
	struct hmm_range *range;

	spin_lock(&hmm->lock);
	list_for_each_entry(range, &hmm->ranges, list) {
		unsigned long addr, idx, npages;

		if (end < range->start || start >= range->end)
			continue;

		range->valid = false;
		addr = max(start, range->start);
		idx = (addr - range->start) >> PAGE_SHIFT;
		npages = (min(range->end, end) - addr) >> PAGE_SHIFT;
		memset(&range->pfns[idx], 0, sizeof(*range->pfns) * npages);
	}
	spin_unlock(&hmm->lock);

	down_read(&hmm->mirrors_sem);
	list_for_each_entry(mirror, &hmm->mirrors, list)
		mirror->ops->sync_cpu_device_pagetables(mirror, action,
							start, end);
	up_read(&hmm->mirrors_sem);
}

static void hmm_release(struct mmu_notifier *mn, struct mm_struct *mm)
{
	struct hmm_mirror *mirror;
	struct hmm *hmm = mm->hmm;

	down_write(&hmm->mirrors_sem);
	mirror = list_first_entry_or_null(&hmm->mirrors, struct hmm_mirror,
					  list);
	while (mirror) {
		list_del_init(&mirror->list);
		if (mirror->ops->release) {
			/*
			 * Drop mirrors_sem so callback can wait on any pending
			 * work that might itself trigger mmu_notifier callback
			 * and thus would deadlock with us.
			 */
			up_write(&hmm->mirrors_sem);
			mirror->ops->release(mirror);
			down_write(&hmm->mirrors_sem);
		}
		mirror = list_first_entry_or_null(&hmm->mirrors,
						  struct hmm_mirror, list);
	}
	up_write(&hmm->mirrors_sem);
}

static int hmm_invalidate_range_start(struct mmu_notifier *mn,
				       struct mm_struct *mm,
				       unsigned long start,
				       unsigned long end,
				       bool blockable)
{
	struct hmm *hmm = mm->hmm;

	VM_BUG_ON(!hmm);

	atomic_inc(&hmm->sequence);

	return 0;
}

static void hmm_invalidate_range_end(struct mmu_notifier *mn,
				     struct mm_struct *mm,
				     unsigned long start,
				     unsigned long end)
{
	struct hmm *hmm = mm->hmm;

	VM_BUG_ON(!hmm);

	hmm_invalidate_range(mm->hmm, HMM_UPDATE_INVALIDATE, start, end);
}

static const struct mmu_notifier_ops hmm_mmu_notifier_ops = {
	.release		= hmm_release,
	.invalidate_range_start	= hmm_invalidate_range_start,
	.invalidate_range_end	= hmm_invalidate_range_end,
};

/*
 * hmm_mirror_register() - register a mirror against an mm
 *
 * @mirror: new mirror struct to register
 * @mm: mm to register against
 *
 * To start mirroring a process address space, the device driver must register
 * an HMM mirror struct.
 *
 * THE mm->mmap_sem MUST BE HELD IN WRITE MODE !
 */
int hmm_mirror_register(struct hmm_mirror *mirror, struct mm_struct *mm)
{
	/* Sanity check */
	if (!mm || !mirror || !mirror->ops)
		return -EINVAL;

again:
	mirror->hmm = hmm_register(mm);
	if (!mirror->hmm)
		return -ENOMEM;

	down_write(&mirror->hmm->mirrors_sem);
	if (mirror->hmm->mm == NULL) {
		/*
		 * A racing hmm_mirror_unregister() is about to destroy the hmm
		 * struct. Try again to allocate a new one.
		 */
		up_write(&mirror->hmm->mirrors_sem);
		mirror->hmm = NULL;
		goto again;
	} else {
		list_add(&mirror->list, &mirror->hmm->mirrors);
		up_write(&mirror->hmm->mirrors_sem);
	}

	return 0;
}
EXPORT_SYMBOL(hmm_mirror_register);

/*
 * hmm_mirror_unregister() - unregister a mirror
 *
 * @mirror: new mirror struct to register
 *
 * Stop mirroring a process address space, and cleanup.
 */
void hmm_mirror_unregister(struct hmm_mirror *mirror)
{
	bool should_unregister = false;
	struct mm_struct *mm;
	struct hmm *hmm;

	if (mirror->hmm == NULL)
		return;

	hmm = mirror->hmm;
	down_write(&hmm->mirrors_sem);
	list_del_init(&mirror->list);
	should_unregister = list_empty(&hmm->mirrors);
	mirror->hmm = NULL;
	mm = hmm->mm;
	hmm->mm = NULL;
	up_write(&hmm->mirrors_sem);

	if (!should_unregister || mm == NULL)
		return;

	mmu_notifier_unregister_no_release(&hmm->mmu_notifier, mm);

	spin_lock(&mm->page_table_lock);
	if (mm->hmm == hmm)
		mm->hmm = NULL;
	spin_unlock(&mm->page_table_lock);

	kfree(hmm);
}
EXPORT_SYMBOL(hmm_mirror_unregister);

struct hmm_vma_walk {
	struct hmm_range	*range;
	unsigned long		last;
	bool			fault;
	bool			block;
};

static int hmm_vma_do_fault(struct mm_walk *walk, unsigned long addr,
			    bool write_fault, uint64_t *pfn)
{
	unsigned int flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_REMOTE;
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	struct vm_area_struct *vma = walk->vma;
	vm_fault_t ret;

	flags |= hmm_vma_walk->block ? 0 : FAULT_FLAG_ALLOW_RETRY;
	flags |= write_fault ? FAULT_FLAG_WRITE : 0;
	ret = handle_mm_fault(vma, addr, flags);
	if (ret & VM_FAULT_RETRY)
		return -EBUSY;
	if (ret & VM_FAULT_ERROR) {
		*pfn = range->values[HMM_PFN_ERROR];
		return -EFAULT;
	}

	return -EAGAIN;
}

static int hmm_pfns_bad(unsigned long addr,
			unsigned long end,
			struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	uint64_t *pfns = range->pfns;
	unsigned long i;

	i = (addr - range->start) >> PAGE_SHIFT;
	for (; addr < end; addr += PAGE_SIZE, i++)
		pfns[i] = range->values[HMM_PFN_ERROR];

	return 0;
}

/*
 * hmm_vma_walk_hole() - handle a range lacking valid pmd or pte(s)
 * @start: range virtual start address (inclusive)
 * @end: range virtual end address (exclusive)
 * @fault: should we fault or not ?
 * @write_fault: write fault ?
 * @walk: mm_walk structure
 * Returns: 0 on success, -EAGAIN after page fault, or page fault error
 *
 * This function will be called whenever pmd_none() or pte_none() returns true,
 * or whenever there is no page directory covering the virtual address range.
 */
static int hmm_vma_walk_hole_(unsigned long addr, unsigned long end,
			      bool fault, bool write_fault,
			      struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	uint64_t *pfns = range->pfns;
	unsigned long i;

	hmm_vma_walk->last = addr;
	i = (addr - range->start) >> PAGE_SHIFT;
	for (; addr < end; addr += PAGE_SIZE, i++) {
		pfns[i] = range->values[HMM_PFN_NONE];
		if (fault || write_fault) {
			int ret;

			ret = hmm_vma_do_fault(walk, addr, write_fault,
					       &pfns[i]);
			if (ret != -EAGAIN)
				return ret;
		}
	}

	return (fault || write_fault) ? -EAGAIN : 0;
}

static inline void hmm_pte_need_fault(const struct hmm_vma_walk *hmm_vma_walk,
				      uint64_t pfns, uint64_t cpu_flags,
				      bool *fault, bool *write_fault)
{
	struct hmm_range *range = hmm_vma_walk->range;

	*fault = *write_fault = false;
	if (!hmm_vma_walk->fault)
		return;

	/* We aren't ask to do anything ... */
	if (!(pfns & range->flags[HMM_PFN_VALID]))
		return;
	/* If this is device memory than only fault if explicitly requested */
	if ((cpu_flags & range->flags[HMM_PFN_DEVICE_PRIVATE])) {
		/* Do we fault on device memory ? */
		if (pfns & range->flags[HMM_PFN_DEVICE_PRIVATE]) {
			*write_fault = pfns & range->flags[HMM_PFN_WRITE];
			*fault = true;
		}
		return;
	}

	/* If CPU page table is not valid then we need to fault */
	*fault = !(cpu_flags & range->flags[HMM_PFN_VALID]);
	/* Need to write fault ? */
	if ((pfns & range->flags[HMM_PFN_WRITE]) &&
	    !(cpu_flags & range->flags[HMM_PFN_WRITE])) {
		*write_fault = true;
		*fault = true;
	}
}

static void hmm_range_need_fault(const struct hmm_vma_walk *hmm_vma_walk,
				 const uint64_t *pfns, unsigned long npages,
				 uint64_t cpu_flags, bool *fault,
				 bool *write_fault)
{
	unsigned long i;

	if (!hmm_vma_walk->fault) {
		*fault = *write_fault = false;
		return;
	}

	for (i = 0; i < npages; ++i) {
		hmm_pte_need_fault(hmm_vma_walk, pfns[i], cpu_flags,
				   fault, write_fault);
		if ((*fault) || (*write_fault))
			return;
	}
}

static int hmm_vma_walk_hole(unsigned long addr, unsigned long end,
			     struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	bool fault, write_fault;
	unsigned long i, npages;
	uint64_t *pfns;

	i = (addr - range->start) >> PAGE_SHIFT;
	npages = (end - addr) >> PAGE_SHIFT;
	pfns = &range->pfns[i];
	hmm_range_need_fault(hmm_vma_walk, pfns, npages,
			     0, &fault, &write_fault);
	return hmm_vma_walk_hole_(addr, end, fault, write_fault, walk);
}

static inline uint64_t pmd_to_hmm_pfn_flags(struct hmm_range *range, pmd_t pmd)
{
	if (pmd_protnone(pmd))
		return 0;
	return pmd_write(pmd) ? range->flags[HMM_PFN_VALID] |
				range->flags[HMM_PFN_WRITE] :
				range->flags[HMM_PFN_VALID];
}

static int hmm_vma_handle_pmd(struct mm_walk *walk,
			      unsigned long addr,
			      unsigned long end,
			      uint64_t *pfns,
			      pmd_t pmd)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	unsigned long pfn, npages, i;
	bool fault, write_fault;
	uint64_t cpu_flags;

	npages = (end - addr) >> PAGE_SHIFT;
	cpu_flags = pmd_to_hmm_pfn_flags(range, pmd);
	hmm_range_need_fault(hmm_vma_walk, pfns, npages, cpu_flags,
			     &fault, &write_fault);

	if (pmd_protnone(pmd) || fault || write_fault)
		return hmm_vma_walk_hole_(addr, end, fault, write_fault, walk);

	pfn = pmd_pfn(pmd) + pte_index(addr);
	for (i = 0; addr < end; addr += PAGE_SIZE, i++, pfn++)
		pfns[i] = hmm_pfn_from_pfn(range, pfn) | cpu_flags;
	hmm_vma_walk->last = end;
	return 0;
}

static inline uint64_t pte_to_hmm_pfn_flags(struct hmm_range *range, pte_t pte)
{
	if (pte_none(pte) || !pte_present(pte))
		return 0;
	return pte_write(pte) ? range->flags[HMM_PFN_VALID] |
				range->flags[HMM_PFN_WRITE] :
				range->flags[HMM_PFN_VALID];
}

static int hmm_vma_handle_pte(struct mm_walk *walk, unsigned long addr,
			      unsigned long end, pmd_t *pmdp, pte_t *ptep,
			      uint64_t *pfn)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	struct vm_area_struct *vma = walk->vma;
	bool fault, write_fault;
	uint64_t cpu_flags;
	pte_t pte = *ptep;
	uint64_t orig_pfn = *pfn;

	*pfn = range->values[HMM_PFN_NONE];
	cpu_flags = pte_to_hmm_pfn_flags(range, pte);
	hmm_pte_need_fault(hmm_vma_walk, orig_pfn, cpu_flags,
			   &fault, &write_fault);

	if (pte_none(pte)) {
		if (fault || write_fault)
			goto fault;
		return 0;
	}

	if (!pte_present(pte)) {
		swp_entry_t entry = pte_to_swp_entry(pte);

		if (!non_swap_entry(entry)) {
			if (fault || write_fault)
				goto fault;
			return 0;
		}

		/*
		 * This is a special swap entry, ignore migration, use
		 * device and report anything else as error.
		 */
		if (is_device_private_entry(entry)) {
			cpu_flags = range->flags[HMM_PFN_VALID] |
				range->flags[HMM_PFN_DEVICE_PRIVATE];
			cpu_flags |= is_write_device_private_entry(entry) ?
				range->flags[HMM_PFN_WRITE] : 0;
			hmm_pte_need_fault(hmm_vma_walk, orig_pfn, cpu_flags,
					   &fault, &write_fault);
			if (fault || write_fault)
				goto fault;
			*pfn = hmm_pfn_from_pfn(range, swp_offset(entry));
			*pfn |= cpu_flags;
			return 0;
		}

		if (is_migration_entry(entry)) {
			if (fault || write_fault) {
				pte_unmap(ptep);
				hmm_vma_walk->last = addr;
				migration_entry_wait(vma->vm_mm,
						     pmdp, addr);
				return -EAGAIN;
			}
			return 0;
		}

		/* Report error for everything else */
		*pfn = range->values[HMM_PFN_ERROR];
		return -EFAULT;
	}

	if (fault || write_fault)
		goto fault;

	*pfn = hmm_pfn_from_pfn(range, pte_pfn(pte)) | cpu_flags;
	return 0;

fault:
	pte_unmap(ptep);
	/* Fault any virtual address we were asked to fault */
	return hmm_vma_walk_hole_(addr, end, fault, write_fault, walk);
}

static int hmm_vma_walk_pmd(pmd_t *pmdp,
			    unsigned long start,
			    unsigned long end,
			    struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	uint64_t *pfns = range->pfns;
	unsigned long addr = start, i;
	pte_t *ptep;

	i = (addr - range->start) >> PAGE_SHIFT;

again:
	if (pmd_none(*pmdp))
		return hmm_vma_walk_hole(start, end, walk);

	if (pmd_huge(*pmdp) && (range->vma->vm_flags & VM_HUGETLB))
		return hmm_pfns_bad(start, end, walk);

	if (pmd_devmap(*pmdp) || pmd_trans_huge(*pmdp)) {
		pmd_t pmd;

		/*
		 * No need to take pmd_lock here, even if some other threads
		 * is splitting the huge pmd we will get that event through
		 * mmu_notifier callback.
		 *
		 * So just read pmd value and check again its a transparent
		 * huge or device mapping one and compute corresponding pfn
		 * values.
		 */
		pmd = pmd_read_atomic(pmdp);
		barrier();
		if (!pmd_devmap(pmd) && !pmd_trans_huge(pmd))
			goto again;

		return hmm_vma_handle_pmd(walk, addr, end, &pfns[i], pmd);
	}

	if (pmd_bad(*pmdp))
		return hmm_pfns_bad(start, end, walk);

	ptep = pte_offset_map(pmdp, addr);
	for (; addr < end; addr += PAGE_SIZE, ptep++, i++) {
		int r;

		r = hmm_vma_handle_pte(walk, addr, end, pmdp, ptep, &pfns[i]);
		if (r) {
			/* hmm_vma_handle_pte() did unmap pte directory */
			hmm_vma_walk->last = addr;
			return r;
		}
	}
	pte_unmap(ptep - 1);

	hmm_vma_walk->last = addr;
	return 0;
}

static void hmm_pfns_clear(struct hmm_range *range,
			   uint64_t *pfns,
			   unsigned long addr,
			   unsigned long end)
{
	for (; addr < end; addr += PAGE_SIZE, pfns++)
		*pfns = range->values[HMM_PFN_NONE];
}

static void hmm_pfns_special(struct hmm_range *range)
{
	unsigned long addr = range->start, i = 0;

	for (; addr < range->end; addr += PAGE_SIZE, i++)
		range->pfns[i] = range->values[HMM_PFN_SPECIAL];
}

/*
 * hmm_vma_get_pfns() - snapshot CPU page table for a range of virtual addresses
 * @range: range being snapshotted
 * Returns: -EINVAL if invalid argument, -ENOMEM out of memory, -EPERM invalid
 *          vma permission, 0 success
 *
 * This snapshots the CPU page table for a range of virtual addresses. Snapshot
 * validity is tracked by range struct. See hmm_vma_range_done() for further
 * information.
 *
 * The range struct is initialized here. It tracks the CPU page table, but only
 * if the function returns success (0), in which case the caller must then call
 * hmm_vma_range_done() to stop CPU page table update tracking on this range.
 *
 * NOT CALLING hmm_vma_range_done() IF FUNCTION RETURNS 0 WILL LEAD TO SERIOUS
 * MEMORY CORRUPTION ! YOU HAVE BEEN WARNED !
 */
int hmm_vma_get_pfns(struct hmm_range *range)
{
	struct vm_area_struct *vma = range->vma;
	struct hmm_vma_walk hmm_vma_walk;
	struct mm_walk mm_walk;
	struct hmm *hmm;

	/* Sanity check, this really should not happen ! */
	if (range->start < vma->vm_start || range->start >= vma->vm_end)
		return -EINVAL;
	if (range->end < vma->vm_start || range->end > vma->vm_end)
		return -EINVAL;

	hmm = hmm_register(vma->vm_mm);
	if (!hmm)
		return -ENOMEM;
	/* Caller must have registered a mirror, via hmm_mirror_register() ! */
	if (!hmm->mmu_notifier.ops)
		return -EINVAL;

	/* FIXME support hugetlb fs */
	if (is_vm_hugetlb_page(vma) || (vma->vm_flags & VM_SPECIAL) ||
			vma_is_dax(vma)) {
		hmm_pfns_special(range);
		return -EINVAL;
	}

	if (!(vma->vm_flags & VM_READ)) {
		/*
		 * If vma do not allow read access, then assume that it does
		 * not allow write access, either. Architecture that allow
		 * write without read access are not supported by HMM, because
		 * operations such has atomic access would not work.
		 */
		hmm_pfns_clear(range, range->pfns, range->start, range->end);
		return -EPERM;
	}

	/* Initialize range to track CPU page table update */
	spin_lock(&hmm->lock);
	range->valid = true;
	list_add_rcu(&range->list, &hmm->ranges);
	spin_unlock(&hmm->lock);

	hmm_vma_walk.fault = false;
	hmm_vma_walk.range = range;
	mm_walk.private = &hmm_vma_walk;

	mm_walk.vma = vma;
	mm_walk.mm = vma->vm_mm;
	mm_walk.pte_entry = NULL;
	mm_walk.test_walk = NULL;
	mm_walk.hugetlb_entry = NULL;
	mm_walk.pmd_entry = hmm_vma_walk_pmd;
	mm_walk.pte_hole = hmm_vma_walk_hole;

	walk_page_range(range->start, range->end, &mm_walk);
	return 0;
}
EXPORT_SYMBOL(hmm_vma_get_pfns);

/*
 * hmm_vma_range_done() - stop tracking change to CPU page table over a range
 * @range: range being tracked
 * Returns: false if range data has been invalidated, true otherwise
 *
 * Range struct is used to track updates to the CPU page table after a call to
 * either hmm_vma_get_pfns() or hmm_vma_fault(). Once the device driver is done
 * using the data,  or wants to lock updates to the data it got from those
 * functions, it must call the hmm_vma_range_done() function, which will then
 * stop tracking CPU page table updates.
 *
 * Note that device driver must still implement general CPU page table update
 * tracking either by using hmm_mirror (see hmm_mirror_register()) or by using
 * the mmu_notifier API directly.
 *
 * CPU page table update tracking done through hmm_range is only temporary and
 * to be used while trying to duplicate CPU page table contents for a range of
 * virtual addresses.
 *
 * There are two ways to use this :
 * again:
 *   hmm_vma_get_pfns(range); or hmm_vma_fault(...);
 *   trans = device_build_page_table_update_transaction(pfns);
 *   device_page_table_lock();
 *   if (!hmm_vma_range_done(range)) {
 *     device_page_table_unlock();
 *     goto again;
 *   }
 *   device_commit_transaction(trans);
 *   device_page_table_unlock();
 *
 * Or:
 *   hmm_vma_get_pfns(range); or hmm_vma_fault(...);
 *   device_page_table_lock();
 *   hmm_vma_range_done(range);
 *   device_update_page_table(range->pfns);
 *   device_page_table_unlock();
 */
bool hmm_vma_range_done(struct hmm_range *range)
{
	unsigned long npages = (range->end - range->start) >> PAGE_SHIFT;
	struct hmm *hmm;

	if (range->end <= range->start) {
		BUG();
		return false;
	}

	hmm = hmm_register(range->vma->vm_mm);
	if (!hmm) {
		memset(range->pfns, 0, sizeof(*range->pfns) * npages);
		return false;
	}

	spin_lock(&hmm->lock);
	list_del_rcu(&range->list);
	spin_unlock(&hmm->lock);

	return range->valid;
}
EXPORT_SYMBOL(hmm_vma_range_done);

/*
 * hmm_vma_fault() - try to fault some address in a virtual address range
 * @range: range being faulted
 * @block: allow blocking on fault (if true it sleeps and do not drop mmap_sem)
 * Returns: 0 success, error otherwise (-EAGAIN means mmap_sem have been drop)
 *
 * This is similar to a regular CPU page fault except that it will not trigger
 * any memory migration if the memory being faulted is not accessible by CPUs.
 *
 * On error, for one virtual address in the range, the function will mark the
 * corresponding HMM pfn entry with an error flag.
 *
 * Expected use pattern:
 * retry:
 *   down_read(&mm->mmap_sem);
 *   // Find vma and address device wants to fault, initialize hmm_pfn_t
 *   // array accordingly
 *   ret = hmm_vma_fault(range, write, block);
 *   switch (ret) {
 *   case -EAGAIN:
 *     hmm_vma_range_done(range);
 *     // You might want to rate limit or yield to play nicely, you may
 *     // also commit any valid pfn in the array assuming that you are
 *     // getting true from hmm_vma_range_monitor_end()
 *     goto retry;
 *   case 0:
 *     break;
 *   case -ENOMEM:
 *   case -EINVAL:
 *   case -EPERM:
 *   default:
 *     // Handle error !
 *     up_read(&mm->mmap_sem)
 *     return;
 *   }
 *   // Take device driver lock that serialize device page table update
 *   driver_lock_device_page_table_update();
 *   hmm_vma_range_done(range);
 *   // Commit pfns we got from hmm_vma_fault()
 *   driver_unlock_device_page_table_update();
 *   up_read(&mm->mmap_sem)
 *
 * YOU MUST CALL hmm_vma_range_done() AFTER THIS FUNCTION RETURN SUCCESS (0)
 * BEFORE FREEING THE range struct OR YOU WILL HAVE SERIOUS MEMORY CORRUPTION !
 *
 * YOU HAVE BEEN WARNED !
 */
int hmm_vma_fault(struct hmm_range *range, bool block)
{
	struct vm_area_struct *vma = range->vma;
	unsigned long start = range->start;
	struct hmm_vma_walk hmm_vma_walk;
	struct mm_walk mm_walk;
	struct hmm *hmm;
	int ret;

	/* Sanity check, this really should not happen ! */
	if (range->start < vma->vm_start || range->start >= vma->vm_end)
		return -EINVAL;
	if (range->end < vma->vm_start || range->end > vma->vm_end)
		return -EINVAL;

	hmm = hmm_register(vma->vm_mm);
	if (!hmm) {
		hmm_pfns_clear(range, range->pfns, range->start, range->end);
		return -ENOMEM;
	}
	/* Caller must have registered a mirror using hmm_mirror_register() */
	if (!hmm->mmu_notifier.ops)
		return -EINVAL;

	/* FIXME support hugetlb fs */
	if (is_vm_hugetlb_page(vma) || (vma->vm_flags & VM_SPECIAL) ||
			vma_is_dax(vma)) {
		hmm_pfns_special(range);
		return -EINVAL;
	}

	if (!(vma->vm_flags & VM_READ)) {
		/*
		 * If vma do not allow read access, then assume that it does
		 * not allow write access, either. Architecture that allow
		 * write without read access are not supported by HMM, because
		 * operations such has atomic access would not work.
		 */
		hmm_pfns_clear(range, range->pfns, range->start, range->end);
		return -EPERM;
	}

	/* Initialize range to track CPU page table update */
	spin_lock(&hmm->lock);
	range->valid = true;
	list_add_rcu(&range->list, &hmm->ranges);
	spin_unlock(&hmm->lock);

	hmm_vma_walk.fault = true;
	hmm_vma_walk.block = block;
	hmm_vma_walk.range = range;
	mm_walk.private = &hmm_vma_walk;
	hmm_vma_walk.last = range->start;

	mm_walk.vma = vma;
	mm_walk.mm = vma->vm_mm;
	mm_walk.pte_entry = NULL;
	mm_walk.test_walk = NULL;
	mm_walk.hugetlb_entry = NULL;
	mm_walk.pmd_entry = hmm_vma_walk_pmd;
	mm_walk.pte_hole = hmm_vma_walk_hole;

	do {
		ret = walk_page_range(start, range->end, &mm_walk);
		start = hmm_vma_walk.last;
	} while (ret == -EAGAIN);

	if (ret) {
		unsigned long i;

		i = (hmm_vma_walk.last - range->start) >> PAGE_SHIFT;
		hmm_pfns_clear(range, &range->pfns[i], hmm_vma_walk.last,
			       range->end);
		hmm_vma_range_done(range);
	}
	return ret;
}
EXPORT_SYMBOL(hmm_vma_fault);
#endif /* IS_ENABLED(CONFIG_HMM_MIRROR) */


#if IS_ENABLED(CONFIG_DEVICE_PRIVATE) ||  IS_ENABLED(CONFIG_DEVICE_PUBLIC)
struct page *hmm_vma_alloc_locked_page(struct vm_area_struct *vma,
				       unsigned long addr)
{
	struct page *page;

	page = alloc_page_vma(GFP_HIGHUSER, vma, addr);
	if (!page)
		return NULL;
	lock_page(page);
	return page;
}
EXPORT_SYMBOL(hmm_vma_alloc_locked_page);


static void hmm_devmem_ref_release(struct percpu_ref *ref)
{
	struct hmm_devmem *devmem;

	devmem = container_of(ref, struct hmm_devmem, ref);
	complete(&devmem->completion);
}

static void hmm_devmem_ref_exit(void *data)
{
	struct percpu_ref *ref = data;
	struct hmm_devmem *devmem;

	devmem = container_of(ref, struct hmm_devmem, ref);
	percpu_ref_exit(ref);
}

static void hmm_devmem_ref_kill(void *data)
{
	struct percpu_ref *ref = data;
	struct hmm_devmem *devmem;

	devmem = container_of(ref, struct hmm_devmem, ref);
	percpu_ref_kill(ref);
	wait_for_completion(&devmem->completion);
}

static int hmm_devmem_fault(struct vm_area_struct *vma,
			    unsigned long addr,
			    const struct page *page,
			    unsigned int flags,
			    pmd_t *pmdp)
{
	struct hmm_devmem *devmem = page->pgmap->data;

	return devmem->ops->fault(devmem, vma, addr, page, flags, pmdp);
}

static void hmm_devmem_free(struct page *page, void *data)
{
	struct hmm_devmem *devmem = data;

	page->mapping = NULL;

	devmem->ops->free(devmem, page);
}

static DEFINE_MUTEX(hmm_devmem_lock);
static RADIX_TREE(hmm_devmem_radix, GFP_KERNEL);

static void hmm_devmem_radix_release(struct resource *resource)
{
	resource_size_t key;

	mutex_lock(&hmm_devmem_lock);
	for (key = resource->start;
	     key <= resource->end;
	     key += PA_SECTION_SIZE)
		radix_tree_delete(&hmm_devmem_radix, key >> PA_SECTION_SHIFT);
	mutex_unlock(&hmm_devmem_lock);
}

static void hmm_devmem_release(void *data)
{
	struct hmm_devmem *devmem = data;
	struct resource *resource = devmem->resource;
	unsigned long start_pfn, npages;
	struct page *page;
	int nid;

	/* pages are dead and unused, undo the arch mapping */
	start_pfn = (resource->start & ~(PA_SECTION_SIZE - 1)) >> PAGE_SHIFT;
	npages = ALIGN(resource_size(resource), PA_SECTION_SIZE) >> PAGE_SHIFT;

	page = pfn_to_page(start_pfn);
	nid = page_to_nid(page);

	mem_hotplug_begin();
	if (resource->desc == IORES_DESC_DEVICE_PRIVATE_MEMORY)
		__remove_pages(start_pfn, npages, NULL);
	else
		arch_remove_memory(nid, start_pfn << PAGE_SHIFT,
				   npages << PAGE_SHIFT, NULL);
	mem_hotplug_done();

	hmm_devmem_radix_release(resource);
}

static int hmm_devmem_pages_create(struct hmm_devmem *devmem)
{
	resource_size_t key, align_start, align_size, align_end;
	struct device *device = devmem->device;
	int ret, nid, is_ram;
	unsigned long pfn;

	align_start = devmem->resource->start & ~(PA_SECTION_SIZE - 1);
	align_size = ALIGN(devmem->resource->start +
			   resource_size(devmem->resource),
			   PA_SECTION_SIZE) - align_start;

	is_ram = region_intersects(align_start, align_size,
				   IORESOURCE_SYSTEM_RAM,
				   IORES_DESC_NONE);
	if (is_ram == REGION_MIXED) {
		WARN_ONCE(1, "%s attempted on mixed region %pr\n",
				__func__, devmem->resource);
		return -ENXIO;
	}
	if (is_ram == REGION_INTERSECTS)
		return -ENXIO;

	if (devmem->resource->desc == IORES_DESC_DEVICE_PUBLIC_MEMORY)
		devmem->pagemap.type = MEMORY_DEVICE_PUBLIC;
	else
		devmem->pagemap.type = MEMORY_DEVICE_PRIVATE;

	devmem->pagemap.res = *devmem->resource;
	devmem->pagemap.page_fault = hmm_devmem_fault;
	devmem->pagemap.page_free = hmm_devmem_free;
	devmem->pagemap.dev = devmem->device;
	devmem->pagemap.ref = &devmem->ref;
	devmem->pagemap.data = devmem;

	mutex_lock(&hmm_devmem_lock);
	align_end = align_start + align_size - 1;
	for (key = align_start; key <= align_end; key += PA_SECTION_SIZE) {
		struct hmm_devmem *dup;

		dup = radix_tree_lookup(&hmm_devmem_radix,
					key >> PA_SECTION_SHIFT);
		if (dup) {
			dev_err(device, "%s: collides with mapping for %s\n",
				__func__, dev_name(dup->device));
			mutex_unlock(&hmm_devmem_lock);
			ret = -EBUSY;
			goto error;
		}
		ret = radix_tree_insert(&hmm_devmem_radix,
					key >> PA_SECTION_SHIFT,
					devmem);
		if (ret) {
			dev_err(device, "%s: failed: %d\n", __func__, ret);
			mutex_unlock(&hmm_devmem_lock);
			goto error_radix;
		}
	}
	mutex_unlock(&hmm_devmem_lock);

	nid = dev_to_node(device);
	if (nid < 0)
		nid = numa_mem_id();

	mem_hotplug_begin();
	/*
	 * For device private memory we call add_pages() as we only need to
	 * allocate and initialize struct page for the device memory. More-
	 * over the device memory is un-accessible thus we do not want to
	 * create a linear mapping for the memory like arch_add_memory()
	 * would do.
	 *
	 * For device public memory, which is accesible by the CPU, we do
	 * want the linear mapping and thus use arch_add_memory().
	 */
	if (devmem->pagemap.type == MEMORY_DEVICE_PUBLIC)
		ret = arch_add_memory(nid, align_start, align_size, NULL,
				false);
	else
		ret = add_pages(nid, align_start >> PAGE_SHIFT,
				align_size >> PAGE_SHIFT, NULL, false);
	if (ret) {
		mem_hotplug_done();
		goto error_add_memory;
	}
	move_pfn_range_to_zone(&NODE_DATA(nid)->node_zones[ZONE_DEVICE],
				align_start >> PAGE_SHIFT,
				align_size >> PAGE_SHIFT, NULL);
	mem_hotplug_done();

	for (pfn = devmem->pfn_first; pfn < devmem->pfn_last; pfn++) {
		struct page *page = pfn_to_page(pfn);

		page->pgmap = &devmem->pagemap;
	}
	return 0;

error_add_memory:
	untrack_pfn(NULL, PHYS_PFN(align_start), align_size);
error_radix:
	hmm_devmem_radix_release(devmem->resource);
error:
	return ret;
}

/*
 * hmm_devmem_add() - hotplug ZONE_DEVICE memory for device memory
 *
 * @ops: memory event device driver callback (see struct hmm_devmem_ops)
 * @device: device struct to bind the resource too
 * @size: size in bytes of the device memory to add
 * Returns: pointer to new hmm_devmem struct ERR_PTR otherwise
 *
 * This function first finds an empty range of physical address big enough to
 * contain the new resource, and then hotplugs it as ZONE_DEVICE memory, which
 * in turn allocates struct pages. It does not do anything beyond that; all
 * events affecting the memory will go through the various callbacks provided
 * by hmm_devmem_ops struct.
 *
 * Device driver should call this function during device initialization and
 * is then responsible of memory management. HMM only provides helpers.
 */
struct hmm_devmem *hmm_devmem_add(const struct hmm_devmem_ops *ops,
				  struct device *device,
				  unsigned long size)
{
	struct hmm_devmem *devmem;
	resource_size_t addr;
	int ret;

	dev_pagemap_get_ops();

	devmem = devm_kzalloc(device, sizeof(*devmem), GFP_KERNEL);
	if (!devmem)
		return ERR_PTR(-ENOMEM);

	init_completion(&devmem->completion);
	devmem->pfn_first = -1UL;
	devmem->pfn_last = -1UL;
	devmem->resource = NULL;
	devmem->device = device;
	devmem->ops = ops;

	ret = percpu_ref_init(&devmem->ref, &hmm_devmem_ref_release,
			      0, GFP_KERNEL);
	if (ret)
		return ERR_PTR(ret);

	ret = devm_add_action_or_reset(device, hmm_devmem_ref_exit, &devmem->ref);
	if (ret)
		return ERR_PTR(ret);

	size = ALIGN(size, PA_SECTION_SIZE);
	addr = min((unsigned long)iomem_resource.end,
		   (1UL << MAX_PHYSMEM_BITS) - 1);
	addr = addr - size + 1UL;

	/*
	 * FIXME add a new helper to quickly walk resource tree and find free
	 * range
	 *
	 * FIXME what about ioport_resource resource ?
	 */
	for (; addr > size && addr >= iomem_resource.start; addr -= size) {
		ret = region_intersects(addr, size, 0, IORES_DESC_NONE);
		if (ret != REGION_DISJOINT)
			continue;

		devmem->resource = devm_request_mem_region(device, addr, size,
							   dev_name(device));
		if (!devmem->resource)
			return ERR_PTR(-ENOMEM);
		break;
	}
	if (!devmem->resource)
		return ERR_PTR(-ERANGE);

	devmem->resource->desc = IORES_DESC_DEVICE_PRIVATE_MEMORY;
	devmem->pfn_first = devmem->resource->start >> PAGE_SHIFT;
	devmem->pfn_last = devmem->pfn_first +
			   (resource_size(devmem->resource) >> PAGE_SHIFT);

	ret = hmm_devmem_pages_create(devmem);
	if (ret)
		return ERR_PTR(ret);

	ret = devm_add_action_or_reset(device, hmm_devmem_release, devmem);
	if (ret)
		return ERR_PTR(ret);

	return devmem;
}
EXPORT_SYMBOL_GPL(hmm_devmem_add);

struct hmm_devmem *hmm_devmem_add_resource(const struct hmm_devmem_ops *ops,
					   struct device *device,
					   struct resource *res)
{
	struct hmm_devmem *devmem;
	int ret;

	if (res->desc != IORES_DESC_DEVICE_PUBLIC_MEMORY)
		return ERR_PTR(-EINVAL);

	dev_pagemap_get_ops();

	devmem = devm_kzalloc(device, sizeof(*devmem), GFP_KERNEL);
	if (!devmem)
		return ERR_PTR(-ENOMEM);

	init_completion(&devmem->completion);
	devmem->pfn_first = -1UL;
	devmem->pfn_last = -1UL;
	devmem->resource = res;
	devmem->device = device;
	devmem->ops = ops;

	ret = percpu_ref_init(&devmem->ref, &hmm_devmem_ref_release,
			      0, GFP_KERNEL);
	if (ret)
		return ERR_PTR(ret);

	ret = devm_add_action_or_reset(device, hmm_devmem_ref_exit,
			&devmem->ref);
	if (ret)
		return ERR_PTR(ret);

	devmem->pfn_first = devmem->resource->start >> PAGE_SHIFT;
	devmem->pfn_last = devmem->pfn_first +
			   (resource_size(devmem->resource) >> PAGE_SHIFT);

	ret = hmm_devmem_pages_create(devmem);
	if (ret)
		return ERR_PTR(ret);

	ret = devm_add_action_or_reset(device, hmm_devmem_release, devmem);
	if (ret)
		return ERR_PTR(ret);

	ret = devm_add_action_or_reset(device, hmm_devmem_ref_kill,
			&devmem->ref);
	if (ret)
		return ERR_PTR(ret);

	return devmem;
}
EXPORT_SYMBOL_GPL(hmm_devmem_add_resource);

/*
 * A device driver that wants to handle multiple devices memory through a
 * single fake device can use hmm_device to do so. This is purely a helper
 * and it is not needed to make use of any HMM functionality.
 */
#define HMM_DEVICE_MAX 256

static DECLARE_BITMAP(hmm_device_mask, HMM_DEVICE_MAX);
static DEFINE_SPINLOCK(hmm_device_lock);
static struct class *hmm_device_class;
static dev_t hmm_device_devt;

static void hmm_device_release(struct device *device)
{
	struct hmm_device *hmm_device;

	hmm_device = container_of(device, struct hmm_device, device);
	spin_lock(&hmm_device_lock);
	clear_bit(hmm_device->minor, hmm_device_mask);
	spin_unlock(&hmm_device_lock);

	kfree(hmm_device);
}

struct hmm_device *hmm_device_new(void *drvdata)
{
	struct hmm_device *hmm_device;

	hmm_device = kzalloc(sizeof(*hmm_device), GFP_KERNEL);
	if (!hmm_device)
		return ERR_PTR(-ENOMEM);

	spin_lock(&hmm_device_lock);
	hmm_device->minor = find_first_zero_bit(hmm_device_mask, HMM_DEVICE_MAX);
	if (hmm_device->minor >= HMM_DEVICE_MAX) {
		spin_unlock(&hmm_device_lock);
		kfree(hmm_device);
		return ERR_PTR(-EBUSY);
	}
	set_bit(hmm_device->minor, hmm_device_mask);
	spin_unlock(&hmm_device_lock);

	dev_set_name(&hmm_device->device, "hmm_device%d", hmm_device->minor);
	hmm_device->device.devt = MKDEV(MAJOR(hmm_device_devt),
					hmm_device->minor);
	hmm_device->device.release = hmm_device_release;
	dev_set_drvdata(&hmm_device->device, drvdata);
	hmm_device->device.class = hmm_device_class;
	device_initialize(&hmm_device->device);

	return hmm_device;
}
EXPORT_SYMBOL(hmm_device_new);

void hmm_device_put(struct hmm_device *hmm_device)
{
	put_device(&hmm_device->device);
}
EXPORT_SYMBOL(hmm_device_put);

static int __init hmm_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&hmm_device_devt, 0,
				  HMM_DEVICE_MAX,
				  "hmm_device");
	if (ret)
		return ret;

	hmm_device_class = class_create(THIS_MODULE, "hmm_device");
	if (IS_ERR(hmm_device_class)) {
		unregister_chrdev_region(hmm_device_devt, HMM_DEVICE_MAX);
		return PTR_ERR(hmm_device_class);
	}
	return 0;
}

device_initcall(hmm_init);
#endif /* CONFIG_DEVICE_PRIVATE || CONFIG_DEVICE_PUBLIC */
