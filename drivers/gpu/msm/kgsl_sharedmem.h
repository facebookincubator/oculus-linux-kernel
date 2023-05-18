/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002, 2007-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_SHAREDMEM_H
#define __KGSL_SHAREDMEM_H

#include <linux/bitfield.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "kgsl.h"
#include "kgsl_mmu.h"

struct kgsl_device;
struct kgsl_process_private;

#define KGSL_CACHE_OP_INV       0x01
#define KGSL_CACHE_OP_FLUSH     0x02
#define KGSL_CACHE_OP_CLEAN     0x03

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc);

int kgsl_cache_range_op(struct kgsl_memdesc *memdesc,
			uint64_t offset, uint64_t size,
			unsigned int op);

void kgsl_memdesc_init(struct kgsl_device *device, struct kgsl_memdesc *memdesc,
		uint64_t flags, uint32_t priv);

void kgsl_process_init_sysfs(struct kgsl_device *device,
		struct kgsl_process_private *private);
void kgsl_process_uninit_sysfs(struct kgsl_process_private *private);

int kgsl_sharedmem_init_sysfs(void);
void kgsl_sharedmem_uninit_sysfs(void);

void kgsl_get_memory_usage(char *str, size_t len, uint64_t memflags);

void kgsl_free_secure_page(struct page *page);

struct page *kgsl_alloc_secure_page(void);

int kgsl_lock_page(struct page *page);
int kgsl_unlock_page(struct page *page);

/**
 * kgsl_allocate_user - Allocate user visible GPU memory
 * @device: A GPU device handle
 * @memdesc: Memory descriptor for the object
 * @size: Size of the allocation in bytes
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 *
 * Allocate GPU memory on behalf of the user.
 * Return: 0 on success or negative on failure.
 */
int kgsl_allocate_user(struct kgsl_device *device, struct kgsl_memdesc *memdesc,
		u64 size, u64 flags, u32 priv);

/**
 * kgsl_allocate_global - Allocate kernel visible GPU memory
 * @device: A GPU device handle
 * @size: Size of the allocation in bytes
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 *
 * Allocate GPU memory on for use by the kernel. Kernel objects are
 * automatically mapped into the kernel address space (except for secure).
 * Return: The memory descriptor on success or a ERR_PTR encoded error on
 * failure.
 */
struct kgsl_memdesc *kgsl_allocate_global(struct kgsl_device *device,
		u64 size, u64 flags, u32 priv, const char *name);

/**
 * kgsl_allocate_global_fixed - Allocate a global GPU memory object from a fixed
 * region defined in the device tree
 * @device: A GPU device handle
 * @size: Size of the allocation in bytes
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 *
 * Allocate a global GPU object for use by all processes. The buffer is
 * added to the list of global buffers that get mapped into each newly created
 * pagetable.
 *
 * Return: The memory descriptor on success or a ERR_PTR encoded error on
 * failure.
 */
struct kgsl_memdesc *kgsl_allocate_global_fixed(struct kgsl_device *device,
		const char *resource, const char *name);

/**
 * kgsl_free_globals - Free all global objects
 * @device: A GPU device handle
 *
 * Free all the global buffer objects. Should only be called during shutdown
 * after the pagetables have been freed
 */
void kgsl_free_globals(struct kgsl_device *device);

/**
 * kgsl_sharedmem_vm_map_readonly - Map a range from an entry into VM
 * @memdesc: Memory descriptor for the object
 * @offset: Offset from the start of the entry in bytes
 * @size: Size of the mapping in bytes
 * @page_count: Returns the number of pages mapped
 *
 * Maps a range from the object associated with memdesc into virtual memory
 * using vm_map_ram. NULL or otherwise invalid pages from the entry will
 * automatically be handled as pointing to the zero page to provide a legal
 * mapping across the full requested range.
 *
 * Return: The requested mapping on success or an ERR_PTR encoded error on
 * failure.
 */
void *kgsl_sharedmem_vm_map_readonly(struct kgsl_memdesc *memdesc, u64 offset,
		u64 size, unsigned int *page_count);

/**
 * kgsl_sharedmem_vm_map - Map a range from an entry into VM
 * @memdesc: Memory descriptor for the object
 * @offset: Offset from the start of the entry in bytes
 * @size: Size of the mapping in bytes
 * @page_count: Returns the number of pages mapped
 *
 * Maps a range from the object associated with memdesc into virtual memory
 * using vm_map_ram if the kernel has write permissions to the entry. NULL or
 * otherwise invalid pages from the entry within the requested range will cause
 * this function to error out.
 *
 * Return: The requested mapping on success or an ERR_PTR encoded error on
 * failure.
 */
void *kgsl_sharedmem_vm_map_readwrite(struct kgsl_memdesc *memdesc, u64 offset,
		u64 size, unsigned int *page_count);

#define MEMFLAGS(_flags, _mask, _shift) \
	((unsigned int) (((_flags) & (_mask)) >> (_shift)))

/**
 * kgsl_memdesc_get_*size - Get relevant parameters for memdescs
 * @memdesc: Memory descriptor for the object
 *
 * Return: The relevant size for the requested parameters.
 */
uint64_t kgsl_memdesc_get_physsize(struct kgsl_memdesc *memdesc);
uint64_t kgsl_memdesc_get_swapsize(struct kgsl_memdesc *memdesc);
uint64_t kgsl_memdesc_get_mapsize(struct kgsl_memdesc *memdesc);

/*
 * kgsl_memdesc_get_align - Get alignment flags from a memdesc
 * @memdesc - the memdesc
 *
 * Returns the alignment requested, as power of 2 exponent.
 */
static inline int
kgsl_memdesc_get_align(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_MEMALIGN_MASK,
		KGSL_MEMALIGN_SHIFT);
}

/*
 * kgsl_memdesc_get_pagesize - Get pagesize based on alignment
 * @memdesc - the memdesc
 *
 * Returns the pagesize based on memdesc alignment
 */
static inline int
kgsl_memdesc_get_pagesize(const struct kgsl_memdesc *memdesc)
{
	return (1 << kgsl_memdesc_get_align(memdesc));
}

/*
 * kgsl_memdesc_get_cachemode - Get cache mode of a memdesc
 * @memdesc: the memdesc
 *
 * Returns a KGSL_CACHEMODE* value.
 */
static inline int
kgsl_memdesc_get_cachemode(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_CACHEMODE_MASK,
		KGSL_CACHEMODE_SHIFT);
}

static inline unsigned int
kgsl_memdesc_get_memtype(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_MEMTYPE_MASK,
		KGSL_MEMTYPE_SHIFT);
}
/*
 * kgsl_memdesc_set_align - Set alignment flags of a memdesc
 * @memdesc - the memdesc
 * @align - alignment requested, as a power of 2 exponent.
 */
static inline int
kgsl_memdesc_set_align(struct kgsl_memdesc *memdesc, unsigned int align)
{
	if (align > 32)
		align = 32;

	memdesc->flags &= ~(uint64_t)KGSL_MEMALIGN_MASK;
	memdesc->flags |= (uint64_t)((align << KGSL_MEMALIGN_SHIFT) &
					KGSL_MEMALIGN_MASK);
	return 0;
}

/**
 * kgsl_memdesc_usermem_type - return buffer type
 * @memdesc - the memdesc
 *
 * Returns a KGSL_MEM_ENTRY_* value for this buffer, which
 * identifies if was allocated by us, or imported from
 * another allocator.
 */
static inline unsigned int
kgsl_memdesc_usermem_type(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_MEMFLAGS_USERMEM_MASK,
		KGSL_MEMFLAGS_USERMEM_SHIFT);
}

/*
 * kgsl_memdesc_is_global - is this a globally mapped buffer?
 * @memdesc: the memdesc
 *
 * Returns nonzero if this is a global mapping, 0 otherwise
 */
static inline int kgsl_memdesc_is_global(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->priv & KGSL_MEMDESC_GLOBAL) != 0;
}

/*
 * kgsl_memdesc_is_secured - is this a secure buffer?
 * @memdesc: the memdesc
 *
 * Returns true if this is a secure mapping, false otherwise
 */
static inline bool kgsl_memdesc_is_secured(const struct kgsl_memdesc *memdesc)
{
	return memdesc && (memdesc->priv & KGSL_MEMDESC_SECURE);
}

/*
 * kgsl_memdesc_has_guard_page - is the last page a guard page?
 * @memdesc - the memdesc
 *
 * Returns nonzero if there is a guard page, 0 otherwise
 */
static inline int
kgsl_memdesc_has_guard_page(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->priv & KGSL_MEMDESC_GUARD_PAGE) != 0;
}

/**
 * kgsl_memdesc_is_reclaimed - check if a buffer is reclaimed
 * @memdesc: the memdesc
 *
 * Return: true if the memdesc pages were reclaimed, false otherwise
 */
static inline bool kgsl_memdesc_is_reclaimed(const struct kgsl_memdesc *memdesc)
{
	return memdesc && (memdesc->priv & KGSL_MEMDESC_RECLAIMED);
}

/*
 * kgsl_memdesc_guard_page_size - returns guard page size
 * @memdesc - the memdesc
 *
 * Returns guard page size
 */
static inline uint64_t
kgsl_memdesc_guard_page_size(const struct kgsl_memdesc *memdesc)
{
	if (!kgsl_memdesc_has_guard_page(memdesc))
		return 0;

	if (kgsl_memdesc_is_secured(memdesc)) {
		if (memdesc->pagetable != NULL &&
				memdesc->pagetable->mmu != NULL)
			return memdesc->pagetable->mmu->secure_align_mask + 1;
	}

	return PAGE_SIZE;
}

/*
 * kgsl_memdesc_use_cpu_map - use the same virtual mapping on CPU and GPU?
 * @memdesc - the memdesc
 */
static inline int
kgsl_memdesc_use_cpu_map(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->flags & KGSL_MEMFLAGS_USE_CPU_MAP) != 0;
}

/*
 * kgsl_memdesc_footprint - get the size of the mmap region
 * @memdesc - the memdesc
 *
 * The entire memdesc must be mapped. Additionally if the
 * CPU mapping is going to be mirrored, there must be room
 * for the guard page to be mapped so that the address spaces
 * match up.
 */
static inline uint64_t
kgsl_memdesc_footprint(const struct kgsl_memdesc *memdesc)
{
	return ALIGN(memdesc->size + kgsl_memdesc_guard_page_size(memdesc),
		PAGE_SIZE);
}

void kgsl_sharedmem_set_noretry(bool val);
bool kgsl_sharedmem_get_noretry(void);

/**
 * kgsl_alloc_sgt_from_pages() - Allocate a sg table
 *
 * @pages: An array of pointers to allocated pages
 * @page_count: Total number of pages allocated
 *
 * Allocate and return pointer to a sg table
 */
static inline struct sg_table *kgsl_alloc_sgt_from_pages(struct page **pages,
		unsigned int page_count)
{
	int ret;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (sgt == NULL)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table_from_pages(sgt, pages, page_count, 0,
			(size_t)page_count << PAGE_SHIFT, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		return ERR_PTR(ret);
	}

	return sgt;
}

/**
 * kgsl_free_sgt() - Free a sg table structure
 *
 * @sgt: sg table pointer to be freed
 *
 * Free the sg table allocated using sgt and free the
 * sgt structure itself
 */
static inline void kgsl_free_sgt(struct sg_table *sgt)
{
	if (sgt != NULL) {
		sg_free_table(sgt);
		kfree(sgt);
	}
}

/**
 * kgsl_cachemode_is_cached - Return true if the passed flags indicate a cached
 * buffer
 * @flags: A bitmask of KGSL_MEMDESC_ flags
 *
 * Return: true if the flags indicate a cached buffer
 */
static inline bool kgsl_cachemode_is_cached(u64 flags)
{
	u64 mode = FIELD_GET(KGSL_CACHEMODE_MASK, flags);

	return (mode != KGSL_CACHEMODE_UNCACHED &&
		mode != KGSL_CACHEMODE_WRITECOMBINE);
}

int kgsl_alloc_page(int *page_size, struct page **pages, unsigned int pages_len,
		unsigned int *align, struct kgsl_memdesc *memdesc,
		unsigned int page_off);
void kgsl_free_page(struct kgsl_memdesc *memdesc, struct page *p);

/**
 * kgsl_gfp_mask() - get gfp_mask to be used
 * @page_order: order of the page
 *
 * Get the gfp_mask to be used for page allocation
 * based on the order of the page
 *
 * Return appropriate gfp_mask
 */
unsigned int kgsl_gfp_mask(unsigned int page_order);

/**
 * kgsl_zero_page() - zero out a page
 * @p: pointer to the struct page
 * @order: order of the page
 *
 * Map a page into kernel and zero it out
 */
void kgsl_zero_page(struct page *page, unsigned int order);

/**
 * kgsl_flush_page - flush a page
 * @page: pointer to the struct page
 *
 * Map a page into kernel and flush it
 */
void kgsl_flush_page(struct page *page);

/**
 * struct kgsl_process_attribute - basic attribute for a process
 * @attr: Underlying struct attribute
 * @show: Attribute show function
 * @store: Attribute store function
 */
struct kgsl_process_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj,
			struct kgsl_process_attribute *attr, char *buf);
	ssize_t (*store)(struct kobject *kobj,
		struct kgsl_process_attribute *attr, const char *buf,
		ssize_t count);
};

#define PROCESS_ATTR(_name, _mode, _show, _store) \
	static struct kgsl_process_attribute attr_##_name = \
			__ATTR(_name, _mode, _show, _store)

#endif /* __KGSL_SHAREDMEM_H */
