/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping-fast.h>
#include <linux/io-pgtable-fast.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <asm/dma-iommu.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <trace/events/iommu.h>

#include <soc/qcom/secure_buffer.h>
#include <linux/arm-smmu-errata.h>

/* some redundant definitions... :( TODO: move to io-pgtable-fast.h */
#define FAST_PAGE_SHIFT		12
#define FAST_PAGE_SIZE (1UL << FAST_PAGE_SHIFT)
#define FAST_PAGE_MASK (~(PAGE_SIZE - 1))
#define FAST_PTE_ADDR_MASK		((av8l_fast_iopte)0xfffffffff000)
#define FAST_MAIR_ATTR_IDX_CACHE	1
#define FAST_PTE_ATTRINDX_SHIFT		2
#define FAST_PTE_ATTRINDX_MASK		0x7
#define FAST_PTE_SH_SHIFT		8
#define FAST_PTE_SH_MASK	   (((av8l_fast_iopte)0x3) << FAST_PTE_SH_SHIFT)
#define FAST_PTE_SH_OS             (((av8l_fast_iopte)2) << FAST_PTE_SH_SHIFT)
#define FAST_PTE_SH_IS             (((av8l_fast_iopte)3) << FAST_PTE_SH_SHIFT)

static pgprot_t __get_dma_pgprot(unsigned long attrs, pgprot_t prot,
				 bool coherent)
{
	if (attrs & DMA_ATTR_STRONGLY_ORDERED)
		return pgprot_noncached(prot);
	else if (!coherent || (attrs & DMA_ATTR_WRITE_COMBINE))
		return pgprot_writecombine(prot);
	return prot;
}

static int __get_iommu_pgprot(unsigned long attrs, int prot,
			      bool coherent)
{
	if (!(attrs & DMA_ATTR_EXEC_MAPPING))
		prot |= IOMMU_NOEXEC;
	if ((attrs & DMA_ATTR_STRONGLY_ORDERED))
		prot |= IOMMU_MMIO;
	if (coherent)
		prot |= IOMMU_CACHE;

	return prot;
}

static void fast_dmac_clean_range(struct dma_fast_smmu_mapping *mapping,
				  void *start, void *end)
{
	if (!mapping->is_smmu_pt_coherent)
		dmac_clean_range(start, end);
}

static bool __fast_is_pte_coherent(av8l_fast_iopte *ptep)
{
	int attr_idx = (*ptep & (FAST_PTE_ATTRINDX_MASK <<
			FAST_PTE_ATTRINDX_SHIFT)) >>
			FAST_PTE_ATTRINDX_SHIFT;

	if ((attr_idx == FAST_MAIR_ATTR_IDX_CACHE) &&
		(((*ptep & FAST_PTE_SH_MASK) == FAST_PTE_SH_IS) ||
		  (*ptep & FAST_PTE_SH_MASK) == FAST_PTE_SH_OS))
		return true;

	return false;
}

static bool is_dma_coherent(struct device *dev, unsigned long attrs)
{
	bool is_coherent;

	if (attrs & DMA_ATTR_FORCE_COHERENT)
		is_coherent = true;
	else if (attrs & DMA_ATTR_FORCE_NON_COHERENT)
		is_coherent = false;
	else if (is_device_dma_coherent(dev))
		is_coherent = true;
	else
		is_coherent = false;

	return is_coherent;
}

/*
 * Checks if the allocated range (ending at @end) covered the upcoming
 * stale bit.  We don't need to know exactly where the range starts since
 * we already know where the candidate search range started.  If, starting
 * from the beginning of the candidate search range, we had to step over
 * (or landed directly on top of) the upcoming stale bit, then we return
 * true.
 *
 * Due to wrapping, there are two scenarios we'll need to check: (1) if the
 * range [search_start, upcoming_stale] spans 0 (i.e. search_start >
 * upcoming_stale), and, (2) if the range: [search_start, upcoming_stale]
 * does *not* span 0 (i.e. search_start <= upcoming_stale).  And for each
 * of those two scenarios we need to handle three cases: (1) the bit was
 * found before wrapping or
 */
static bool __bit_covered_stale(unsigned long upcoming_stale,
				unsigned long search_start,
				unsigned long end)
{
	if (search_start > upcoming_stale) {
		if (end >= search_start) {
			/*
			 * We started searching above upcoming_stale and we
			 * didn't wrap, so we couldn't have crossed
			 * upcoming_stale.
			 */
			return false;
		}
		/*
		 * We wrapped. Did we cross (or land on top of)
		 * upcoming_stale?
		 */
		return end >= upcoming_stale;
	}

	if (search_start <= upcoming_stale) {
		if (end >= search_start) {
			/*
			 * We didn't wrap.  Did we cross (or land on top
			 * of) upcoming_stale?
			 */
			return end >= upcoming_stale;
		}
		/*
		 * We wrapped. So we must have crossed upcoming_stale
		 * (since we started searching below it).
		 */
		return true;
	}

	/* we should have covered all logical combinations... */
	WARN_ON(1);
	return true;
}

static dma_addr_t __fast_smmu_alloc_iova(struct dma_fast_smmu_mapping *mapping,
					 unsigned long attrs,
					 size_t size)
{
	unsigned long bit, prev_search_start, nbits;
	unsigned long align;
	unsigned long guard_len;
	dma_addr_t iova;

	if (mapping->min_iova_align)
		guard_len = ALIGN(size + mapping->force_guard_page_len,
				  mapping->min_iova_align) - size;
	else
		guard_len = 0;

	nbits = (size + guard_len) >> FAST_PAGE_SHIFT;
	align = (1 << get_order(size + guard_len)) - 1;
	bit = bitmap_find_next_zero_area(
		mapping->bitmap, mapping->num_4k_pages, mapping->next_start,
		nbits, align);
	if (unlikely(bit > mapping->num_4k_pages)) {
		/* try wrapping */
		mapping->next_start = 0; /* TODO: SHOULD I REALLY DO THIS?!? */
		bit = bitmap_find_next_zero_area(
			mapping->bitmap, mapping->num_4k_pages, 0, nbits,
			align);
		if (unlikely(bit > mapping->num_4k_pages))
			return DMA_ERROR_CODE;
	}

	bitmap_set(mapping->bitmap, bit, nbits);
	prev_search_start = mapping->next_start;
	mapping->next_start = bit + nbits;
	if (unlikely(mapping->next_start >= mapping->num_4k_pages))
		mapping->next_start = 0;

	/*
	 * If we just re-allocated a VA whose TLB hasn't been invalidated
	 * since it was last used and unmapped, we need to invalidate it
	 * here.  We actually invalidate the entire TLB so that we don't
	 * have to invalidate the TLB again until we wrap back around.
	 */
	if (mapping->have_stale_tlbs &&
	    __bit_covered_stale(mapping->upcoming_stale_bit,
				prev_search_start,
				bit + nbits - 1)) {
		bool skip_sync = (attrs & DMA_ATTR_SKIP_CPU_SYNC);

		iommu_tlbiall(mapping->domain);
		mapping->have_stale_tlbs = false;
		av8l_fast_clear_stale_ptes(mapping->pgtbl_pmds, skip_sync);
	}

	iova =  (bit << FAST_PAGE_SHIFT) + mapping->base;
	if (guard_len &&
		iommu_map(mapping->domain, iova + size,
			page_to_phys(mapping->guard_page),
			guard_len, ARM_SMMU_GUARD_PROT)) {

		bitmap_clear(mapping->bitmap, bit, nbits);
		return DMA_ERROR_CODE;
	}
	return iova;
}

/*
 * Checks whether the candidate bit will be allocated sooner than the
 * current upcoming stale bit.  We can say candidate will be upcoming
 * sooner than the current upcoming stale bit if it lies between the
 * starting bit of the next search range and the upcoming stale bit
 * (allowing for wrap-around).
 *
 * Stated differently, we're checking the relative ordering of three
 * unsigned numbers.  So we need to check all 6 (i.e. 3!) permutations,
 * namely:
 *
 *     0 |---A---B---C---| TOP (Case 1)
 *     0 |---A---C---B---| TOP (Case 2)
 *     0 |---B---A---C---| TOP (Case 3)
 *     0 |---B---C---A---| TOP (Case 4)
 *     0 |---C---A---B---| TOP (Case 5)
 *     0 |---C---B---A---| TOP (Case 6)
 *
 * Note that since we're allowing numbers to wrap, the following three
 * scenarios are all equivalent for Case 1:
 *
 *     0 |---A---B---C---| TOP
 *     0 |---C---A---B---| TOP (C has wrapped. This is Case 5.)
 *     0 |---B---C---A---| TOP (C and B have wrapped. This is Case 4.)
 *
 * In any of these cases, if we start searching from A, we will find B
 * before we find C.
 *
 * We can also find two equivalent cases for Case 2:
 *
 *     0 |---A---C---B---| TOP
 *     0 |---B---A---C---| TOP (B has wrapped. This is Case 3.)
 *     0 |---C---B---A---| TOP (B and C have wrapped. This is Case 6.)
 *
 * In any of these cases, if we start searching from A, we will find C
 * before we find B.
 */
static bool __bit_is_sooner(unsigned long candidate,
			    struct dma_fast_smmu_mapping *mapping)
{
	unsigned long A = mapping->next_start;
	unsigned long B = candidate;
	unsigned long C = mapping->upcoming_stale_bit;

	if ((A < B && B < C) ||	/* Case 1 */
	    (C < A && A < B) ||	/* Case 5 */
	    (B < C && C < A))	/* Case 4 */
		return true;

	if ((A < C && C < B) ||	/* Case 2 */
	    (B < A && A < C) ||	/* Case 3 */
	    (C < B && B < A))	/* Case 6 */
		return false;

	/*
	 * For simplicity, we've been ignoring the possibility of any of
	 * our three numbers being equal.  Handle those cases here (they
	 * shouldn't happen very often, (I think?)).
	 */

	/*
	 * If candidate is the next bit to be searched then it's definitely
	 * sooner.
	 */
	if (A == B)
		return true;

	/*
	 * If candidate is the next upcoming stale bit we'll return false
	 * to avoid doing `upcoming = candidate' in the caller (which would
	 * be useless since they're already equal)
	 */
	if (B == C)
		return false;

	/*
	 * If next start is the upcoming stale bit then candidate can't
	 * possibly be sooner.  The "soonest" bit is already selected.
	 */
	if (A == C)
		return false;

	/* We should have covered all logical combinations. */
	WARN(1, "Well, that's awkward. A=%ld, B=%ld, C=%ld\n", A, B, C);
	return true;
}

static void __fast_smmu_free_iova(struct dma_fast_smmu_mapping *mapping,
				  dma_addr_t iova, size_t size)
{
	unsigned long start_bit = (iova - mapping->base) >> FAST_PAGE_SHIFT;
	unsigned long nbits;
	unsigned long guard_len;

	if (mapping->min_iova_align)
		guard_len = ALIGN(size + mapping->force_guard_page_len,
				  mapping->min_iova_align) - size;
	else
		guard_len = 0;

	if (guard_len)
		iommu_unmap(mapping->domain, iova + size, guard_len);

	nbits = (size + guard_len) >> FAST_PAGE_SHIFT;


	/*
	 * We don't invalidate TLBs on unmap.  We invalidate TLBs on map
	 * when we're about to re-allocate a VA that was previously
	 * unmapped but hasn't yet been invalidated.  So we need to keep
	 * track of which bit is the closest to being re-allocated here.
	 */
	if (__bit_is_sooner(start_bit, mapping))
		mapping->upcoming_stale_bit = start_bit;

	bitmap_clear(mapping->bitmap, start_bit, nbits);
	mapping->have_stale_tlbs = true;
}


static void __fast_dma_page_cpu_to_dev(struct page *page, unsigned long off,
				       size_t size, enum dma_data_direction dir)
{
	__dma_map_area(page_address(page) + off, size, dir);
}

static void __fast_dma_page_dev_to_cpu(struct page *page, unsigned long off,
				       size_t size, enum dma_data_direction dir)
{
	__dma_unmap_area(page_address(page) + off, size, dir);

	/* TODO: WHAT IS THIS? */
	/*
	 * Mark the D-cache clean for this page to avoid extra flushing.
	 */
	if (dir != DMA_TO_DEVICE && off == 0 && size >= PAGE_SIZE)
		set_bit(PG_dcache_clean, &page->flags);
}

static int __fast_dma_direction_to_prot(enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_BIDIRECTIONAL:
		return IOMMU_READ | IOMMU_WRITE;
	case DMA_TO_DEVICE:
		return IOMMU_READ;
	case DMA_FROM_DEVICE:
		return IOMMU_WRITE;
	default:
		return 0;
	}
}

static dma_addr_t fast_smmu_map_page(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   unsigned long attrs)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	dma_addr_t iova;
	unsigned long flags;
	av8l_fast_iopte *pmd;
	phys_addr_t phys_plus_off = page_to_phys(page) + offset;
	phys_addr_t phys_to_map = round_down(phys_plus_off, FAST_PAGE_SIZE);
	unsigned long offset_from_phys_to_map = phys_plus_off & ~FAST_PAGE_MASK;
	size_t len = ALIGN(size + offset_from_phys_to_map, FAST_PAGE_SIZE);
	int nptes = len >> FAST_PAGE_SHIFT;
	bool skip_sync = (attrs & DMA_ATTR_SKIP_CPU_SYNC);
	int prot = __fast_dma_direction_to_prot(dir);
	bool is_coherent = is_dma_coherent(dev, attrs);

	prot = __get_iommu_pgprot(attrs, prot, is_coherent);

	if (!skip_sync && !is_coherent)
		__fast_dma_page_cpu_to_dev(phys_to_page(phys_to_map),
					   offset_from_phys_to_map, size, dir);

	spin_lock_irqsave(&mapping->lock, flags);

	iova = __fast_smmu_alloc_iova(mapping, attrs, len);

	if (unlikely(iova == DMA_ERROR_CODE))
		goto fail;

	pmd = iopte_pmd_offset(mapping->pgtbl_pmds, iova);

	if (unlikely(av8l_fast_map_public(pmd, phys_to_map, len, prot)))
		goto fail_free_iova;

	fast_dmac_clean_range(mapping, pmd, pmd + nptes);

	spin_unlock_irqrestore(&mapping->lock, flags);

	trace_map(mapping->domain, iova, phys_to_map, len, prot);
	return iova + offset_from_phys_to_map;

fail_free_iova:
	__fast_smmu_free_iova(mapping, iova, size);
fail:
	spin_unlock_irqrestore(&mapping->lock, flags);
	return DMA_ERROR_CODE;
}

static void fast_smmu_unmap_page(struct device *dev, dma_addr_t iova,
			       size_t size, enum dma_data_direction dir,
			       unsigned long attrs)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	unsigned long flags;
	av8l_fast_iopte *pmd = iopte_pmd_offset(mapping->pgtbl_pmds, iova);
	unsigned long offset = iova & ~FAST_PAGE_MASK;
	size_t len = ALIGN(size + offset, FAST_PAGE_SIZE);
	int nptes = len >> FAST_PAGE_SHIFT;
	struct page *page = phys_to_page((*pmd & FAST_PTE_ADDR_MASK));
	bool skip_sync = (attrs & DMA_ATTR_SKIP_CPU_SYNC);
	bool is_coherent = is_dma_coherent(dev, attrs);

	if (!skip_sync && !is_coherent)
		__fast_dma_page_dev_to_cpu(page, offset, size, dir);

	spin_lock_irqsave(&mapping->lock, flags);
	av8l_fast_unmap_public(pmd, len);
	fast_dmac_clean_range(mapping, pmd, pmd + nptes);
	__fast_smmu_free_iova(mapping, iova - offset, len);
	spin_unlock_irqrestore(&mapping->lock, flags);

	trace_unmap(mapping->domain, iova - offset, len, len);
}

static void fast_smmu_sync_single_for_cpu(struct device *dev,
		dma_addr_t iova, size_t size, enum dma_data_direction dir)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	av8l_fast_iopte *pmd = iopte_pmd_offset(mapping->pgtbl_pmds, iova);
	unsigned long offset = iova & ~FAST_PAGE_MASK;
	struct page *page = phys_to_page((*pmd & FAST_PTE_ADDR_MASK));

	if (!__fast_is_pte_coherent(pmd))
		__fast_dma_page_dev_to_cpu(page, offset, size, dir);
}

static void fast_smmu_sync_single_for_device(struct device *dev,
		dma_addr_t iova, size_t size, enum dma_data_direction dir)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	av8l_fast_iopte *pmd = iopte_pmd_offset(mapping->pgtbl_pmds, iova);
	unsigned long offset = iova & ~FAST_PAGE_MASK;
	struct page *page = phys_to_page((*pmd & FAST_PTE_ADDR_MASK));

	if (!__fast_is_pte_coherent(pmd))
		__fast_dma_page_cpu_to_dev(page, offset, size, dir);
}

static int fast_smmu_map_sg(struct device *dev, struct scatterlist *sg,
			    int nents, enum dma_data_direction dir,
			    unsigned long attrs)
{
	/* 0 indicates error */
	return 0;
}

static void fast_smmu_unmap_sg(struct device *dev,
			       struct scatterlist *sg, int nents,
			       enum dma_data_direction dir,
			       unsigned long attrs)
{
	WARN_ON_ONCE(1);
}

static void fast_smmu_sync_sg_for_cpu(struct device *dev,
		struct scatterlist *sg, int nents, enum dma_data_direction dir)
{
	WARN_ON_ONCE(1);
}

static void fast_smmu_sync_sg_for_device(struct device *dev,
		struct scatterlist *sg, int nents, enum dma_data_direction dir)
{
	WARN_ON_ONCE(1);
}

static void __fast_smmu_free_pages(struct page **pages, int count)
{
	int i;

	for (i = 0; i < count; i++)
		__free_page(pages[i]);
	kvfree(pages);
}

static struct page **__fast_smmu_alloc_pages(unsigned int count, gfp_t gfp)
{
	struct page **pages;
	unsigned int i = 0, array_size = count * sizeof(*pages);

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	/* IOMMU can map any pages, so himem can also be used here */
	gfp |= __GFP_NOWARN | __GFP_HIGHMEM;

	for (i = 0; i < count; ++i) {
		struct page *page = alloc_page(gfp);

		if (!page) {
			__fast_smmu_free_pages(pages, i);
			return NULL;
		}
		pages[i] = page;
	}
	return pages;
}

static void *fast_smmu_alloc(struct device *dev, size_t size,
			     dma_addr_t *handle, gfp_t gfp,
			     unsigned long attrs)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	struct sg_table sgt;
	dma_addr_t dma_addr, iova_iter;
	void *addr;
	av8l_fast_iopte *ptep;
	unsigned long flags;
	struct sg_mapping_iter miter;
	size_t count = ALIGN(size, SZ_4K) >> PAGE_SHIFT;
	int prot = IOMMU_READ | IOMMU_WRITE; /* TODO: extract from attrs */
	bool is_coherent = is_dma_coherent(dev, attrs);
	pgprot_t remap_prot = __get_dma_pgprot(attrs, PAGE_KERNEL, is_coherent);
	struct page **pages;

	/*
	 * sg_alloc_table_from_pages accepts unsigned int value for count
	 * so check count doesn't exceed UINT_MAX.
	 */

	if (count > UINT_MAX) {
		dev_err(dev, "count: %zx exceeds UNIT_MAX\n", count);
		return NULL;
	}

	prot = __get_iommu_pgprot(attrs, prot, is_coherent);

	*handle = DMA_ERROR_CODE;

	pages = __fast_smmu_alloc_pages(count, gfp);
	if (!pages) {
		dev_err(dev, "no pages\n");
		return NULL;
	}

	size = ALIGN(size, SZ_4K);
	if (sg_alloc_table_from_pages(&sgt, pages, count, 0, size, gfp)) {
		dev_err(dev, "no sg tablen\n");
		goto out_free_pages;
	}

	if (!is_coherent) {
		/*
		 * The CPU-centric flushing implied by SG_MITER_TO_SG isn't
		 * sufficient here, so skip it by using the "wrong" direction.
		 */
		sg_miter_start(&miter, sgt.sgl, sgt.orig_nents,
			       SG_MITER_FROM_SG);
		while (sg_miter_next(&miter))
			__dma_flush_area(miter.addr, miter.length);
		sg_miter_stop(&miter);
	}

	spin_lock_irqsave(&mapping->lock, flags);
	dma_addr = __fast_smmu_alloc_iova(mapping, attrs, size);
	if (dma_addr == DMA_ERROR_CODE) {
		dev_err(dev, "no iova\n");
		spin_unlock_irqrestore(&mapping->lock, flags);
		goto out_free_sg;
	}
	iova_iter = dma_addr;
	sg_miter_start(&miter, sgt.sgl, sgt.orig_nents,
		       SG_MITER_FROM_SG | SG_MITER_ATOMIC);
	while (sg_miter_next(&miter)) {
		int nptes = miter.length >> FAST_PAGE_SHIFT;

		ptep = iopte_pmd_offset(mapping->pgtbl_pmds, iova_iter);
		if (unlikely(av8l_fast_map_public(
				     ptep, page_to_phys(miter.page),
				     miter.length, prot))) {
			dev_err(dev, "no map public\n");
			/* TODO: unwind previously successful mappings */
			goto out_free_iova;
		}
		fast_dmac_clean_range(mapping, ptep, ptep + nptes);
		iova_iter += miter.length;
	}
	sg_miter_stop(&miter);
	spin_unlock_irqrestore(&mapping->lock, flags);

	addr = dma_common_pages_remap(pages, size, VM_USERMAP, remap_prot,
				      __builtin_return_address(0));
	if (!addr) {
		dev_err(dev, "no common pages\n");
		goto out_unmap;
	}

	*handle = dma_addr;
	sg_free_table(&sgt);
	return addr;

out_unmap:
	/* need to take the lock again for page tables and iova */
	spin_lock_irqsave(&mapping->lock, flags);
	ptep = iopte_pmd_offset(mapping->pgtbl_pmds, dma_addr);
	av8l_fast_unmap_public(ptep, size);
	fast_dmac_clean_range(mapping, ptep, ptep + count);
out_free_iova:
	__fast_smmu_free_iova(mapping, dma_addr, size);
	spin_unlock_irqrestore(&mapping->lock, flags);
out_free_sg:
	sg_free_table(&sgt);
out_free_pages:
	__fast_smmu_free_pages(pages, count);
	return NULL;
}

static void fast_smmu_free(struct device *dev, size_t size,
			   void *vaddr, dma_addr_t dma_handle,
			   unsigned long attrs)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	struct vm_struct *area;
	struct page **pages;
	size_t count = ALIGN(size, SZ_4K) >> FAST_PAGE_SHIFT;
	av8l_fast_iopte *ptep;
	unsigned long flags;

	size = ALIGN(size, SZ_4K);

	area = find_vm_area(vaddr);
	if (WARN_ON_ONCE(!area))
		return;

	pages = area->pages;
	dma_common_free_remap(vaddr, size, VM_USERMAP, false);
	ptep = iopte_pmd_offset(mapping->pgtbl_pmds, dma_handle);
	spin_lock_irqsave(&mapping->lock, flags);
	av8l_fast_unmap_public(ptep, size);
	fast_dmac_clean_range(mapping, ptep, ptep + count);
	__fast_smmu_free_iova(mapping, dma_handle, size);
	spin_unlock_irqrestore(&mapping->lock, flags);
	__fast_smmu_free_pages(pages, count);
}

static int fast_smmu_mmap_attrs(struct device *dev, struct vm_area_struct *vma,
				void *cpu_addr, dma_addr_t dma_addr,
				size_t size, unsigned long attrs)
{
	struct vm_struct *area;
	unsigned long uaddr = vma->vm_start;
	struct page **pages;
	int i, nr_pages, ret = 0;
	bool coherent = is_dma_coherent(dev, attrs);

	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot,
					     coherent);
	area = find_vm_area(cpu_addr);
	if (!area)
		return -EINVAL;

	pages = area->pages;
	nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	for (i = vma->vm_pgoff; i < nr_pages && uaddr < vma->vm_end; i++) {
		ret = vm_insert_page(vma, uaddr, pages[i]);
		if (ret)
			break;
		uaddr += PAGE_SIZE;
	}

	return ret;
}

static int fast_smmu_get_sgtable(struct device *dev, struct sg_table *sgt,
				void *cpu_addr, dma_addr_t dma_addr,
				size_t size, unsigned long attrs)
{
	unsigned int n_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct vm_struct *area;

	area = find_vm_area(cpu_addr);
	if (!area || !area->pages)
		return -EINVAL;

	return sg_alloc_table_from_pages(sgt, area->pages, n_pages, 0, size,
					GFP_KERNEL);
}

static dma_addr_t fast_smmu_dma_map_resource(
			struct device *dev, phys_addr_t phys_addr,
			size_t size, enum dma_data_direction dir,
			unsigned long attrs)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	size_t offset = phys_addr & ~FAST_PAGE_MASK;
	size_t len = round_up(size + offset, FAST_PAGE_SIZE);
	dma_addr_t dma_addr;
	int prot;
	unsigned long flags;

	spin_lock_irqsave(&mapping->lock, flags);
	dma_addr = __fast_smmu_alloc_iova(mapping, attrs, len);
	spin_unlock_irqrestore(&mapping->lock, flags);

	if (dma_addr == DMA_ERROR_CODE)
		return dma_addr;

	prot = __fast_dma_direction_to_prot(dir);
	prot |= IOMMU_MMIO;

	if (iommu_map(mapping->domain, dma_addr, phys_addr - offset,
			len, prot)) {
		spin_lock_irqsave(&mapping->lock, flags);
		__fast_smmu_free_iova(mapping, dma_addr, len);
		spin_unlock_irqrestore(&mapping->lock, flags);
		return DMA_ERROR_CODE;
	}
	return dma_addr + offset;
}

static void fast_smmu_dma_unmap_resource(
			struct device *dev, dma_addr_t addr,
			size_t size, enum dma_data_direction dir,
			unsigned long attrs)
{
	struct dma_fast_smmu_mapping *mapping = dev->archdata.mapping->fast;
	size_t offset = addr & ~FAST_PAGE_MASK;
	size_t len = round_up(size + offset, FAST_PAGE_SIZE);
	unsigned long flags;

	iommu_unmap(mapping->domain, addr - offset, len);
	spin_lock_irqsave(&mapping->lock, flags);
	__fast_smmu_free_iova(mapping, addr - offset, len);
	spin_unlock_irqrestore(&mapping->lock, flags);
}

static int fast_smmu_mapping_error(struct device *dev,
				   dma_addr_t dma_addr)
{
	return dma_addr == DMA_ERROR_CODE;
}

static void __fast_smmu_mapped_over_stale(struct dma_fast_smmu_mapping *fast,
					  void *data)
{
	av8l_fast_iopte *ptep = data;
	dma_addr_t iova;
	unsigned long bitmap_idx;

	bitmap_idx = (unsigned long)(ptep - fast->pgtbl_pmds);
	iova = bitmap_idx << FAST_PAGE_SHIFT;
	dev_err(fast->dev, "Mapped over stale tlb at %pa\n", &iova);
	dev_err(fast->dev, "bitmap (failure at idx %lu):\n", bitmap_idx);
	dev_err(fast->dev, "ptep: %p pmds: %p diff: %lu\n", ptep,
		fast->pgtbl_pmds, bitmap_idx);
	print_hex_dump(KERN_ERR, "bmap: ", DUMP_PREFIX_ADDRESS,
		       32, 8, fast->bitmap, fast->bitmap_size, false);
}

static int fast_smmu_notify(struct notifier_block *self,
			    unsigned long action, void *data)
{
	struct dma_fast_smmu_mapping *fast = container_of(
		self, struct dma_fast_smmu_mapping, notifier);

	switch (action) {
	case MAPPED_OVER_STALE_TLB:
		__fast_smmu_mapped_over_stale(fast, data);
		return NOTIFY_OK;
	default:
		WARN(1, "Unhandled notifier action");
		return NOTIFY_DONE;
	}
}

static const struct dma_map_ops fast_smmu_dma_ops = {
	.alloc = fast_smmu_alloc,
	.free = fast_smmu_free,
	.mmap = fast_smmu_mmap_attrs,
	.get_sgtable = fast_smmu_get_sgtable,
	.map_page = fast_smmu_map_page,
	.unmap_page = fast_smmu_unmap_page,
	.sync_single_for_cpu = fast_smmu_sync_single_for_cpu,
	.sync_single_for_device = fast_smmu_sync_single_for_device,
	.map_sg = fast_smmu_map_sg,
	.unmap_sg = fast_smmu_unmap_sg,
	.sync_sg_for_cpu = fast_smmu_sync_sg_for_cpu,
	.sync_sg_for_device = fast_smmu_sync_sg_for_device,
	.map_resource = fast_smmu_dma_map_resource,
	.unmap_resource = fast_smmu_dma_unmap_resource,
	.mapping_error = fast_smmu_mapping_error,
};

/**
 * __fast_smmu_create_mapping_sized
 * @base: bottom of the VA range
 * @size: size of the VA range in bytes
 *
 * Creates a mapping structure which holds information about used/unused IO
 * address ranges, which is required to perform mapping with IOMMU aware
 * functions.  The only VA range supported is [0, 4GB).
 *
 * The client device need to be attached to the mapping with
 * fast_smmu_attach_device function.
 */
static struct dma_fast_smmu_mapping *__fast_smmu_create_mapping_sized(
	dma_addr_t base, u64 size)
{
	struct dma_fast_smmu_mapping *fast;

	fast = kzalloc(sizeof(struct dma_fast_smmu_mapping), GFP_KERNEL);
	if (!fast)
		goto err;

	fast->base = base;
	fast->size = size;
	fast->num_4k_pages = size >> FAST_PAGE_SHIFT;
	fast->bitmap_size = BITS_TO_LONGS(fast->num_4k_pages) * sizeof(long);

	fast->bitmap = kzalloc(fast->bitmap_size, GFP_KERNEL | __GFP_NOWARN |
								__GFP_NORETRY);
	if (!fast->bitmap)
		fast->bitmap = vzalloc(fast->bitmap_size);

	if (!fast->bitmap)
		goto err2;

	spin_lock_init(&fast->lock);

	return fast;
err2:
	kfree(fast);
err:
	return ERR_PTR(-ENOMEM);
}

/*
 * Based off of similar code from dma-iommu.c, but modified to use a different
 * iova allocator
 */
static void fast_smmu_reserve_pci_windows(struct device *dev,
			    struct dma_fast_smmu_mapping *mapping)
{
	struct pci_host_bridge *bridge;
	struct resource_entry *window;
	phys_addr_t start, end;
	struct pci_dev *pci_dev;
	unsigned long flags;

	if (!dev_is_pci(dev))
		return;

	pci_dev = to_pci_dev(dev);
	bridge = pci_find_host_bridge(pci_dev->bus);

	spin_lock_irqsave(&mapping->lock, flags);
	resource_list_for_each_entry(window, &bridge->windows) {
		if (resource_type(window->res) != IORESOURCE_MEM &&
		    resource_type(window->res) != IORESOURCE_IO)
			continue;

		start = round_down(window->res->start - window->offset,
				FAST_PAGE_SIZE);
		end = round_up(window->res->end - window->offset,
				FAST_PAGE_SIZE);
		start = max_t(unsigned long, mapping->base, start);
		end = min_t(unsigned long, mapping->base + mapping->size, end);
		if (start >= end)
			continue;

		dev_dbg(dev, "iova allocator reserved 0x%pa-0x%pa\n",
				&start, &end);

		start = (start - mapping->base) >> FAST_PAGE_SHIFT;
		end = (end - mapping->base) >> FAST_PAGE_SHIFT;
		bitmap_set(mapping->bitmap, start, end - start);
	}
	spin_unlock_irqrestore(&mapping->lock, flags);
}

static int fast_smmu_errata_init(struct dma_iommu_mapping *mapping)
{
	struct dma_fast_smmu_mapping *fast = mapping->fast;
	int vmid = VMID_HLOS;
	int min_iova_align = 0;
	int force_iova_guard_page = 0;

	iommu_domain_get_attr(mapping->domain,
			      DOMAIN_ATTR_MMU500_ERRATA_MIN_ALIGN,
			      &min_iova_align);
	iommu_domain_get_attr(mapping->domain, DOMAIN_ATTR_SECURE_VMID, &vmid);
	iommu_domain_get_attr(mapping->domain,
			      DOMAIN_ATTR_FORCE_IOVA_GUARD_PAGE,
			      &force_iova_guard_page);

	if (vmid >= VMID_LAST || vmid < 0)
		vmid = VMID_HLOS;

	fast->min_iova_align = (min_iova_align) ?  ARM_SMMU_MIN_IOVA_ALIGN :
		PAGE_SIZE;

	if (force_iova_guard_page)
		fast->force_guard_page_len = PAGE_SIZE;

	fast->guard_page =
		arm_smmu_errata_get_guard_page(vmid);
	if (!fast->guard_page)
		return -ENOMEM;

	return 0;
}

/**
 * fast_smmu_init_mapping
 * @dev: valid struct device pointer
 * @mapping: io address space mapping structure (returned from
 *	arm_iommu_create_mapping)
 *
 * Called the first time a device is attached to this mapping.
 * Not for dma client use.
 */
int fast_smmu_init_mapping(struct device *dev,
			    struct dma_iommu_mapping *mapping)
{
	int err = 0;
	struct iommu_domain *domain = mapping->domain;
	struct iommu_pgtbl_info info;
	u64 size = (u64)mapping->bits << PAGE_SHIFT;

	if (mapping->base + size > (SZ_1G * 4ULL)) {
		dev_err(dev, "Iova end address too large\n");
		return -EINVAL;
	}

	mapping->fast = __fast_smmu_create_mapping_sized(mapping->base, size);
	if (IS_ERR(mapping->fast))
		return -ENOMEM;
	mapping->fast->domain = domain;
	mapping->fast->dev = dev;

	if (fast_smmu_errata_init(mapping))
		goto release_mapping;

	fast_smmu_reserve_pci_windows(dev, mapping->fast);

	if (iommu_domain_get_attr(domain, DOMAIN_ATTR_PGTBL_INFO,
				  &info)) {
		dev_err(dev, "Couldn't get page table info\n");
		err = -EINVAL;
		goto release_mapping;
	}
	mapping->fast->pgtbl_pmds = info.pmds;

	if (iommu_domain_get_attr(domain, DOMAIN_ATTR_PAGE_TABLE_IS_COHERENT,
				  &mapping->fast->is_smmu_pt_coherent)) {
		err = -EINVAL;
		goto release_mapping;
	}

	mapping->fast->notifier.notifier_call = fast_smmu_notify;
	av8l_register_notify(&mapping->fast->notifier);

	mapping->ops = &fast_smmu_dma_ops;
	return 0;

release_mapping:
	kfree(mapping->fast->bitmap);
	kfree(mapping->fast);
	return err;
}

/**
 * fast_smmu_release_mapping
 * @kref: dma_iommu_mapping->kref
 *
 * Cleans up the given iommu mapping.
 */
void fast_smmu_release_mapping(struct kref *kref)
{
	struct dma_iommu_mapping *mapping =
		container_of(kref, struct dma_iommu_mapping, kref);

	kvfree(mapping->fast->bitmap);
	kfree(mapping->fast);
	iommu_domain_free(mapping->domain);
	kfree(mapping);
}
