// SPDX-License-Identifier: GPL+
//
// Copyright Meta Platforms, Inc. and its affiliates.
//

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/file.h>
#include <linux/bit_spinlock.h>
#include <linux/rwlock.h>
#include <linux/pinctrl/consumer.h>
#include <linux/soc/qcom/llcc-qcom.h>

// amem_kmd.h interface is optimized for use by the client (which needs to be
// able to do atomic operations on the shared data structures).
// The kernel can't trust the data in these same structures anyway, so atomic
// operations are not needed.
#define atomic_uint_fast8_t uint8_t
#include <linux/amem_kmd.h>

#include "tcm_heap.h"

/**
 * @file amem_kmd.c
 *
 * @brief Contains implementation of "Adaptive Memory" amem character device.
 *
 * @details
 *
 */

#ifndef CONFIG_ARM64
#define __phys_to_pte(phys, prot) pfn_pte((phys) >> PAGE_SHIFT, prot)
#else // CONFIG_ARM64
#define __phys_to_pte(phys, prot) \
	__pte(__phys_to_pte_val(phys) | pgprot_val(prot))

// Optimized and (8x) unrolled L1-3 cache invalidation.
// This is needed when memory is evicted from TCM. The cache needs to be
// cleaned to ensure reads from the kernel virtual address are correct.
// The 'invalidate' solves the case where an mmap operation happens to use the
// same memory address that was used before.
// Linux doesn't seem to provide a cross-platform function to do this.
static __always_inline void clean_invalidate_cache_range(unsigned long addr,
							 size_t size)
{
	uintptr_t end = (uintptr_t)(addr + size);

	asm volatile (
	"1:	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	dc civac, %[crt]\n"
	"	add %[crt], %[crt], #64\n"
	"	cmp %[crt], %[end]\n"
	"	blo 1b\n"
	"	dsb sy\n"
		: [crt] "+r" (addr)
		: [end] "r" (end)
		: "memory"
	);
}

// 4x unrolled MEMCPY. Assumes size is PAGE_SIZE
static __always_inline void fast_copy_page(void *dst, void *src)
{
	uint64_t *end = (uint64_t *)(src + PAGE_SIZE);
	unsigned long word1, word2;

	asm volatile (
	"1:	ldp %0, %1, [%2], #16\n"
	"	stp %0, %1, [%3], #16\n"
	"	ldp %0, %1, [%2], #16\n"
	"	stp %0, %1, [%3], #16\n"
	"	ldp %0, %1, [%2], #16\n"
	"	stp %0, %1, [%3], #16\n"
	"	ldp %0, %1, [%2], #16\n"
	"	stp %0, %1, [%3], #16\n"
	"	cmp %2, %4\n"
	"	bne 1b\n"
		: "=&r" (word1), "=&r" (word2)
		: "r" (src), "r" (dst), "r" (end)
	);
}

// 8x unrolled optimized (16-byte per cycle) clear.
// Does this exist in a header somewhere already?
// Using memset crashes, and this is faster anyway.
static __always_inline void fast_clear_page(void *addr)
{
	uint64_t *end = (uint64_t *)(addr + PAGE_SIZE);

	asm volatile (
	"1:	stp xzr, xzr, [%0], #16\n"
	"	stp xzr, xzr, [%0], #16\n"
	"	stp xzr, xzr, [%0], #16\n"
	"	stp xzr, xzr, [%0], #16\n"
	"	stp xzr, xzr, [%0], #16\n"
	"	stp xzr, xzr, [%0], #16\n"
	"	stp xzr, xzr, [%0], #16\n"
	"	stp xzr, xzr, [%0], #16\n"
	"	cmp %0, %[end]\n"
	"	blo 1b\n"
		: [crt] "+r" (addr)
		: [end] "r" (end)
		: "memory"
	);
}

#endif // CONFIG_ARM64

enum AMEM_BUF_TYPE {

	// These buffers are not physically contiguous:
	//  Allocations can succeed even when Fragmented
	//  Allocations are typically fast
	//  Making room for LARGE_DMA_BUF requires clearing out
	//  of the entire pool until there's enough space at the top
	AMEM_SCATTER_BUF,

	// These buffers are physically contiguous:
	//  Fragmentation can block allocations
	//  Making room for LARGE_DMA_BUF requires waiting for AMEM_DMA_BUF
	//  to unpin by the end of the frame.
	AMEM_DMA_BUF,

	// These buffers are physically contiguous and large:
	//  Allocated in the top portion of a pool
	//  Only one LARGE_DMA_BUF per pool
	//  If they're larger than the largest TCM pool, it goes in DRAM
	AMEM_LARGE_DMA_BUF,
};

// Only SCATTER_BUF allocations can be larger than a pool.
// Every SCATTER_BUF is placed in a list grouped by its pool(s) and
// its accessExponent. This makes it possible to evict buffers based
// on the accessExponent. Higher accessExponent will be evicted first.

// A buffer with accessExponent larger than this will never be placed into TCM.
// 2^(-3) seconds = 125 milliseconds
#define MAX_TCM_ACCESS_EXPONENT -3
// We don't track accessExponent less than -18 --> 2^(-20) = ~1 microseconds
#define MIN_TCM_ACCESS_EXPONENT -20
#define TCM_ACCESS_EXPONENT_BIAS (-MIN_TCM_ACCESS_EXPONENT)
#define TCM_ACCESS_EXPONENT_SLOTS \
	(MAX_TCM_ACCESS_EXPONENT - MIN_TCM_ACCESS_EXPONENT)


// TCM Pool configuration
// The configuration of pools will initially be hard-coded, but is better to
// eventually be moved to the device tree.
struct TCM_POOL {

	// Heap of free tcm_pfn_t values.
	// This is a min-heap, so allocations will allocate prioritizing
	// lower tcm_pfn_t values (lower physical address).
	// This makes it easier to compact TCM so we can allocate large
	// physically contiguous in the (high) portions of this pool.
	// Do not access directly! Use accessor functions in tcm_heap.h
	// Accessor functions are thread-safe
	struct tcm_heap *heap;

	spinlock_t lock;

	// Protected by lock
	unchar state;

	// Count of free TCM pages.
	// If a load operation brings this down to zero, (or it's already
	// zero) this may force an evict. The free_count shouldn't
	// go above zero until an evict occurs that isn't being immediately
	// replaced by a load operation.
	// This is contrasted to heap.free_size which may go above zero
	// with intermediate forced evictions.
	// This allows the driver to be able to commit to a load operation
	// without heap.free_size changing underneath it.
	// Protected by lock
	tcm_pfn_t free_count;

	// Lists of amem objects that have TCM allocated.
	// These are indexed by (accessExponent - TCM_ACCESS_EXPONENT_BIAS)
	// Protected by lock
	struct list_head amem_head[TCM_ACCESS_EXPONENT_SLOTS];
};

// Having more pools allows more AMEM_LARGE_DMA_BUF allocations
#define NUM_TCM_POOLS 2
struct TCM_POOL tcm_pools[NUM_TCM_POOLS] = {0};

static DEFINE_SPINLOCK(tcm_lock);

// Protected by tcm_lock
static unchar tcm_state;

struct llcc_tcm_data *llcc_tcm_data;

// The base virtual address of TCM
static void __iomem *tcm_virt_base;

// The physical address extents of TCM
static phys_addr_t tcm_phys_addr_base;
static phys_addr_t tcm_phys_addr_end;

// The physical address extents of RAM
static phys_addr_t min_ram_phys_addr;
// TODO: T187806292 - This assumes it doesn't grow, is this a valid assumption?
static phys_addr_t max_ram_phys_addr;


inline void *tcm_virt_from_pfn(tcm_pfn_t tcm_pfn)
{
	ulong offset = ((ulong)tcm_pfn) << PAGE_SHIFT;

	return tcm_virt_base + offset;
}

inline phys_addr_t tcm_phys_from_pfn(tcm_pfn_t tcm_pfn)
{
	ulong offset = ((ulong)tcm_pfn) << PAGE_SHIFT;

	return tcm_phys_addr_base + offset;
}

#define AMEM_TCM_DEACTIVATED 0
#define AMEM_TCM_ACTIVATED   1
#define AMEM_TCM_TEST_MODE   2

struct amem_context {
	struct mutex lock;
	struct class *class;
	struct device *sysfs_device;
	dev_t devno;
	struct cdev cdev;
	struct llcc_tcm_data *test_tcm_data;

	atomic_t page_count;
};

// amem is a singleton
struct amem_context *amem_context;

struct amem_manager {
	union {
		char client_tag[8];
		u64 client_tag_as_u64;
	};

	rwlock_t lock;

	u16 amem_id_count;
	u16 amem_id_highest;

	// These are the same pointer, it's just that the first one in the
	// array of amem_data is actually an amem_manager_data.
	// For example, dereference the manager fd: "mgr_client->fd"
	// and dereference an amem fd: "amem_clients[client_num]", where
	// client_num is non-zero.
	// This is done because it's a shared memory area with the client
	// process. We only need one pointer to the memory.
	union {
		struct amem_manager_data *mgr_client;
		struct amem_data *amem_clients;
	};
};

// bits 0-3 (values 0-16)
// DISTANCE refers to how 'far' the memory is from the CPU.
// Lower numbers (other than 0) are closer, and higher numbers are farther away
// For example, TCM is very close in terms of power and latency, and NVM
// flash is very far away in terms of higher power and latency.
enum AMEM_DISTANCE {
	AMEM_DISTANCE_UNINITIALIZED	= 0,
	AMEM_DISTANCE_TCM		= 2,
	AMEM_DISTANCE_FASTRAM		= 4,
	AMEM_DISTANCE_RAM		= 6,
	AMEM_DISTANCE_SLOWRAM		= 8,
	AMEM_DISTANCE_ZRAM		= 10,
	AMEM_DISTANCE_NVM		= 12,
};

#define AMEM_DISTANCE_MASK 0xF

#ifdef CONFIG_AMEM_LARGE_MEMORY
	#define MAX_AMEM_PFN 0x0FFFFFFFFFFFFFFF
	typedef uint64_t spte_value_t;
#else
	#define MAX_AMEM_PFN 0x0FFFFFFF
	typedef uint32_t spte_value_t;
#endif

// Custom amem "shadow page table entry".
// The main linux PTEs are zapped before we can read them when the vma is
// closed, so we maintain a shadow copy so we don't leak our memory.
struct spte_t {
	// Use the spte_* functions instead of accessing this directly
	spte_value_t val;
};

#define SPTE_ENCODING_SHIFT 4
__always_inline spte_value_t spte_decode_pfn(struct spte_t spte)
{
	return spte.val >> SPTE_ENCODING_SHIFT;
}

__always_inline struct spte_t spte_none(void)
{
	struct spte_t spte = {
		.val = AMEM_DISTANCE_UNINITIALIZED
	};
	return spte;
}

__always_inline struct spte_t spte_from_ram_phys(phys_addr_t ram_phys)
{
	struct spte_t spte = {
		.val = (((size_t)ram_phys) >> PAGE_SHIFT) <<
			SPTE_ENCODING_SHIFT | AMEM_DISTANCE_RAM
	};
	return spte;
}

__always_inline struct spte_t spte_from_tcm_pfn(tcm_pfn_t tcm_pfn)
{
	struct spte_t spte = {
		.val = ((size_t)tcm_pfn) << SPTE_ENCODING_SHIFT |
			AMEM_DISTANCE_TCM
	};
	return spte;
}

__always_inline phys_addr_t spte_to_ram_phys(struct spte_t spte)
{
	return ((phys_addr_t)(spte.val >> SPTE_ENCODING_SHIFT)) << PAGE_SHIFT;
}

__always_inline tcm_pfn_t spte_to_tcm_pfn(struct spte_t spte)
{
	return (tcm_pfn_t)(spte.val >> SPTE_ENCODING_SHIFT);
}

__always_inline enum AMEM_DISTANCE spte_get_distance(struct spte_t spte)
{
	return (enum AMEM_DISTANCE)(spte.val & AMEM_DISTANCE_MASK);
}

#define AMEM_BITLOCK_RAM	16

// Number of bit locks assigned to pages
// Must be a power of two and less than 32
#define PAGE_BIT_LOCK_COUNT	16

struct amem {
	struct file *manager_file;	// the amem_manager

	// Entry for the list this amem buffer is in
	struct list_head amem_list;

	// list of ram pages allocated for the memory
	// protected by bit_locks(AMEM_BITLOCK_RAM)
	struct list_head free_pages;

	// id of the amem within the amem_manager.
	// Used to reference the amem_data for this amem
	u16 amem_id;

	// Size of the allocated memory in pages
	int size_pages;

	int flags;
	size_t max_latency_ns;	// maximum latency of memory in nanoseconds

	rwlock_t state_lock;
	ulong state;

	// Can there be multiple of these? Maybe it should be a linked list?
	// Currently multiple vmas are blocked in amem_mmap
	struct vm_area_struct *vma;

	// A set of 64 bit-sized locks.
	// The lower PAGE_BIT_LOCK_COUNT are used for locking each page while
	// performing per-page evict and load operations.
	// bitnum = (virt_addr >> PAGE_SHIFT) % PAGE_BIT_LOCK_COUNT;
	// This should have very low contention.
	ulong bit_locks;

	// If state contains AMEM_FAR_ZRAM or AMEM_FAR_NVM, this points to
	// a file that contains the evicted memory
	struct file *far_file;

	// The "shadow page table" for this amem object.
	// "amem->sptes" is an array with "amem->size_pages" elements.
	// protected by bit_locks(page_num % PAGE_BIT_LOCK_COUNT)
	// This uses the common "Struct Hack" for a dynamically sized struct.
	struct spte_t spt[0];
};


struct pte_fn_data {
	struct amem *amem;

	struct vm_area_struct *vma;
	struct spte_t *spte;
	pgprot_t vm_page_prot;		/* Access permissions of this VMA. */

	int already_mapped_count;
};

// Initialize new amem buffer with pages from TCM
int amem_tcm_init_pte_fn(pte_t *ptep, unsigned long addr, void *fn_data)
{
	struct pte_fn_data *data = (struct pte_fn_data *)fn_data;

	tcm_pfn_t tcm_pfn;
	phys_addr_t tcm_phys;
	void __iomem *tcm_virt;
	pte_t new_pte;

	// Grab the TCM page
	tcm_pfn = tcm_heap_allocate(tcm_pools[0].heap);

	// We pre-allocated the required number of TCM pages under a different
	// lock. If we hit TCM_HEAP_OOM here it's because something was
	// corrupted (or a very bad bug that causes TCM memory leaks).
	if (unlikely(tcm_pfn == TCM_HEAP_OOM))
		BUG();

	tcm_virt = tcm_virt_from_pfn(tcm_pfn);

	fast_clear_page(tcm_virt);

	// Map to the TCM page (no faulting bits).
	tcm_phys = tcm_phys_from_pfn(tcm_pfn);
	new_pte = __phys_to_pte(tcm_phys, data->vm_page_prot);

	// Write the main PTE
	WRITE_ONCE(*ptep, new_pte);

	// Write the shadow PTE
	*data->spte = spte_from_tcm_pfn(tcm_pfn);
	data->spte++;

	return 0;
}

// Initialize new amem buffer with pages from RAM
int amem_ram_init_pte_fn(pte_t *ptep, unsigned long addr, void *fn_data)
{
	struct pte_fn_data *data = (struct pte_fn_data *)fn_data;

	phys_addr_t ram_phys;
	pte_t new_pte;
	struct amem *amem = data->amem;
	struct page *page;

	// Pull a free page of RAM
	bit_spin_lock(AMEM_BITLOCK_RAM, &amem->bit_locks);
	if (unlikely(list_empty(&amem->free_pages)))
		BUG();

	page = list_first_entry(&amem->free_pages, struct page, slab_list);
	list_del(&page->slab_list);
	bit_spin_unlock(AMEM_BITLOCK_RAM, &amem->bit_locks);

	// Map to the RAM page with our default pgprot flags
	ram_phys = page_to_phys(page);
	new_pte = __phys_to_pte(ram_phys, data->vm_page_prot);

	WRITE_ONCE(*ptep, new_pte);

	// Write the shadow PTE
	*data->spte = spte_from_ram_phys(ram_phys);
	data->spte++;

	return 0;
}

// The memory is probably in RAM, but needs to be loaded to TCM:
int amem_tcm_pte_fn(pte_t *ptep, unsigned long addr, void *fn_data)
{
	int bitnum = (addr >> PAGE_SHIFT) % PAGE_BIT_LOCK_COUNT;
	struct pte_fn_data *data = (struct pte_fn_data *)fn_data;
	struct amem *amem = data->amem;

	// Lock the page, so fault() doesn't collide
	bit_spin_lock(bitnum, &amem->bit_locks);
	{
		struct spte_t spte = *(data->spte);
		uint8_t distance = spte_get_distance(spte);

		switch (distance) {
		case AMEM_DISTANCE_RAM: {
			phys_addr_t ram_phys = spte_to_ram_phys(spte);

			// Calculate kernel virtual address of page of RAM
			void *cur_virt_addr = phys_to_virt(ram_phys);
			tcm_pfn_t tcm_pfn;
			void __iomem *tcm_virt;
			phys_addr_t tcm_phys;
			pte_t new_pte;
			struct page *page = NULL;

			// Grab the TCM page
			tcm_pfn = tcm_heap_allocate(tcm_pools[0].heap);
			if (unlikely(tcm_pfn == TCM_HEAP_OOM))
				BUG();

			tcm_virt = tcm_virt_from_pfn(tcm_pfn);

			// Security: The TCM page has unknown data in it!
			// Entirely copy the RAM page into the TCM page
			fast_copy_page(tcm_virt, cur_virt_addr);

			// Remap to the copy in TCM with our default pgprot flags
			tcm_phys = tcm_phys_from_pfn(tcm_pfn);
			new_pte = __phys_to_pte(tcm_phys, data->vm_page_prot);

			WRITE_ONCE(*ptep, new_pte);

			// Write the shadow PTE
			*data->spte = spte_from_tcm_pfn(tcm_pfn);

			// Calculate the struct page*
			page = phys_to_page(ram_phys);

			// Add the RAM page back to the free list
			bit_spin_lock(AMEM_BITLOCK_RAM, &amem->bit_locks);
			list_add(&page->slab_list, &amem->free_pages);
			bit_spin_unlock(AMEM_BITLOCK_RAM, &amem->bit_locks);
		} break;
		case AMEM_DISTANCE_TCM: {
			// Nothing to do, already mapped to TCM
			data->already_mapped_count++;
		} break;
		default:
			// ERROR! Unknown address!
			BUG();
		}
	}
	// Unlock the page
	bit_spin_unlock(bitnum, &amem->bit_locks);

	data->spte++;

	return 0;
}

// The memory is probably in TCM, but needs to be evicted to RAM:
int amem_ram_pte_fn(pte_t *ptep, unsigned long addr, void *fn_data)
{
	int bitnum = (addr >> PAGE_SHIFT) % PAGE_BIT_LOCK_COUNT;
	struct pte_fn_data *data = (struct pte_fn_data *)fn_data;
	struct amem *amem = data->amem;

	// Lock the page, so fault() doesn't collide
	bit_spin_lock(bitnum, &amem->bit_locks);
	{
		struct spte_t spte = *(data->spte);
		uint8_t distance = spte_get_distance(spte);

		switch (distance) {
		case AMEM_DISTANCE_TCM: {
			tcm_pfn_t tcm_pfn = spte_to_tcm_pfn(spte);
			void *tcm_virt = tcm_virt_from_pfn(tcm_pfn);
			void *ram_virt;
			phys_addr_t ram_phys;
			pte_t new_pte;
			struct page *page;

			// Pull a free page of RAM
			bit_spin_lock(AMEM_BITLOCK_RAM, &amem->bit_locks);
			if (unlikely(list_empty(&amem->free_pages)))
				BUG();
			page = list_first_entry(&amem->free_pages, struct page, slab_list);
			list_del(&page->slab_list);
			bit_spin_unlock(AMEM_BITLOCK_RAM, &amem->bit_locks);

			ram_virt = page_to_virt(page);

			// Copy the old TCM page to the allocated page of RAM.
			fast_copy_page(ram_virt, tcm_virt);

			// Remap to the copy in RAM with our default pgprot flags
			ram_phys = virt_to_phys(ram_virt);
			new_pte = __phys_to_pte(ram_phys, data->vm_page_prot);

			WRITE_ONCE(*ptep, new_pte);

			// Write the shadow PTE
			*data->spte = spte_from_ram_phys(ram_phys);

			tcm_heap_free(tcm_pools[0].heap, tcm_pfn);

		} break;
		case AMEM_DISTANCE_RAM: {
			// Nothing to do, already mapped to RAM
			data->already_mapped_count++;
		} break;
		default:
			// ERROR! Unknown address!
			BUG();
		}
	}
	// Unlock the page
	bit_spin_unlock(bitnum, &amem->bit_locks);

	data->spte++;

	return 0;
}

// The spte needs to be cleared
inline void amem_free_spte(struct amem *amem, struct spte_t spte)
{
	uint8_t distance = spte_get_distance(spte);

	switch (distance) {
	case AMEM_DISTANCE_TCM: {
		tcm_pfn_t tcm_pfn = spte_to_tcm_pfn(spte);

		tcm_heap_free(tcm_pools[0].heap, tcm_pfn);
	} break;
	case AMEM_DISTANCE_RAM: {
		phys_addr_t ram_phys = spte_to_ram_phys(spte);
		struct page *page;

		page = phys_to_page(ram_phys);

		// Add the RAM page back to the free list
		bit_spin_lock(AMEM_BITLOCK_RAM, &amem->bit_locks);
		list_add(&page->slab_list, &amem->free_pages);
		bit_spin_unlock(AMEM_BITLOCK_RAM, &amem->bit_locks);
	} break;
	default:
		// ERROR! Unknown address!
		BUG();
	}
}


// Is this needed? TODO: T187806292
void amem_vm_open(struct vm_area_struct *vma)
{
	pr_notice("%s was called\n", __func__);
}

// called when VMA is destroyed. The existence of close() prevents the VMA
// from being "merged" (which is an unsupported operation)
void amem_vm_close(struct vm_area_struct *vma)
{
	struct file *filp = (struct file *)(vma->vm_private_data);
	struct amem *amem = (struct amem *)(filp->private_data);
	struct amem_manager *manager = (struct amem_manager *)
					(amem->manager_file->private_data);
	struct amem_data *amem_data = &manager->amem_clients[amem->amem_id];
	struct amem_data cleared_data = {};
	tcm_pfn_t size_pages = amem->size_pages;
	struct spte_t *spte = &amem->spt[0];
	unsigned long flags;
	int i;

	BUG_ON(vma != amem->vma);

	pr_notice("%s was called\n", __func__);

	write_lock(&amem->state_lock);

	amem->vma = NULL;

	if (amem->state != AMEM_DISTANCE_UNINITIALIZED) {
		for (i = 0; i < size_pages; i++) {
			amem_free_spte(amem, *spte);
			*spte = spte_none();
			spte++;
		}
	}

	if (amem->state == AMEM_DISTANCE_TCM) {
		spin_lock_irqsave(&tcm_pools[0].lock, flags);

		// return the evicted pages to free_count
		tcm_pools[0].free_count += size_pages;

		list_del(&amem->amem_list);

		spin_unlock_irqrestore(&tcm_pools[0].lock, flags);
	}

	amem->state = AMEM_DISTANCE_UNINITIALIZED;

	*amem_data = cleared_data;

	write_unlock(&amem->state_lock);

	fput(filp);
}

// called when a page within the VMA has a page-fault.
vm_fault_t amem_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct file *filp = (struct file *)(vma->vm_private_data);
	struct amem *amem = (struct amem *)(filp->private_data);

	struct pte_fn_data fn_data = {
		.amem = amem,
		.spte = &amem->spt[0],
		.vm_page_prot = vma->vm_page_prot,
	};

	pr_notice("%s was called\n", __func__);

	// Check if the fault was caused by a write access:
	if (vmf->flags & FAULT_FLAG_WRITE) {
		int err;
		// A RDONLY page fault occured while an evict or load operation
		// is in-progress.
		// Do operation for just this page including locks
		read_lock(&amem->state_lock);
		if (amem->state == AMEM_DISTANCE_TCM) {
			err = amem_tcm_pte_fn(vmf->pte, vmf->address, &fn_data);
		} else if (amem->state == AMEM_DISTANCE_RAM) {
			err = amem_ram_pte_fn(vmf->pte, vmf->address, &fn_data);
		} else if (amem->state == AMEM_DISTANCE_UNINITIALIZED) {
			// Buffer was accessed before initial load or after amem_vm_close
			err = -EINVAL;
		} else {
			// Corrupted state
			BUG();
		}
		read_unlock(&amem->state_lock);

		if (err < 0)
			return VM_FAULT_SIGSEGV;

	} else if (pte_none(vmf->orig_pte)) {

		// Check if the user tried to access the buffer before
		// amem_ioctl_initial_load completed. This is forbidden.
		if (!amem->far_file)
			return VM_FAULT_SIGSEGV;

		// A page fault occurred because the page is still in far_file
		// Do load operation for just this page including locks
		// TODO: T187807747 Implement evict/load to/from flash
		return VM_FAULT_SIGSEGV;
	} else {
		return VM_FAULT_SIGSEGV;
	}

	if (fn_data.already_mapped_count) {
		// If it was it already mapped, why did we fault?
		return VM_FAULT_SIGSEGV;
	}

	return VM_FAULT_NOPAGE;
}

static const struct vm_operations_struct amem_vm_ops = {
	.open = amem_vm_open,
	.close = amem_vm_close,
	.fault = amem_vm_fault,
	/* ... other operations ... */
};

// Undo the actions of amem_create_amem.
static int amem_release(struct inode *inode, struct file *filp)
{
	struct amem_context *context =
		container_of(inode->i_cdev, struct amem_context, cdev);
	struct amem *amem = filp->private_data;
	struct amem_manager *manager = (struct amem_manager *)
					(amem->manager_file->private_data);
	struct page *page;
	struct page *tmp;

	BUG_ON(amem->vma != NULL);

	list_for_each_entry_safe(page, tmp, &amem->free_pages, slab_list) {
		list_del(&page->slab_list);
		__free_page(page);
	}

	// Update total number of pages handled within /dev/amem
	atomic_sub(amem->size_pages, &context->page_count);

	write_lock(&manager->lock);

	BUG_ON(amem->amem_id == 0);

	if (manager->amem_id_highest == amem->amem_id)
		manager->amem_id_highest--;

	manager->amem_id_count--;

	write_unlock(&manager->lock);

	fput(amem->manager_file);
	kfree(amem);

	return 0;
}

// Called to initialize the virtual memory area for the amem buffer
// so it can be mapped into the client's memory space
static int amem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct amem *amem = filp->private_data;
	size_t mem_size = vma->vm_end - vma->vm_start;

	if (mem_size != (((size_t)(amem->size_pages)) << PAGE_SHIFT))
		return -EINVAL;

	write_lock(&amem->state_lock);

	// Currently supports only one mmap.
	// Fail if it's already been called.
	if (amem->vma) {
		write_unlock(&amem->state_lock);
		return -EFAULT;
	}

	amem->vma = vma;

	// Future operations on this VMA will use amem functions
	vma->vm_ops = &amem_vm_ops;
	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTCOPY;
	vma->vm_private_data = filp;

	// mmap shouldn't use the file for anything, the mappings are unrelated
	vma->vm_file = NULL;

	write_unlock(&amem->state_lock);

	// NOTE: This function doesn't actually map (pin) the buffer in memory
	//       The client must call amem_pin() first which will result in a
	//       amem_ioctl_load() call, which will perform the initial map.
	//       Accessing the memory before mapping will cause a SIGSEGV.

	return 0;
}

// Called when the first AMEM_IOCTL_LOAD occurs.
static long amem_ioctl_initial_load(struct amem *amem, unsigned long arg)
{
	pte_fn_t pte_fn;
	tcm_pfn_t size_pages = amem->size_pages;
	size_t size_bytes = ((size_t)size_pages) << PAGE_SHIFT;
	unsigned long flags;

	struct vm_area_struct *vma = amem->vma;
	struct pte_fn_data fn_data = {
		.amem = amem,
		.vma = vma,
		.spte = &amem->spt[0],
	};

	// TODO: T187806292 - Should this also include PTE_DEVMAP?
	// Add PTE_SPECIAL to the default page protection flags otherwise the
	// kernel will try to manage this memory (potentially trying to swap it
	// to disk).
	pgprot_val(vma->vm_page_prot) |= PTE_SPECIAL;

	spin_lock_irqsave(&tcm_pools[0].lock, flags);

	// Ensure that we have enough TCM available, otherwise target RAM
	if (tcm_pools[0].free_count >= size_pages) {
		tcm_pools[0].free_count -= size_pages;
		amem->state = AMEM_DISTANCE_TCM;

		// Move the amem to the tcm_pool list
		list_add(&amem->amem_list, &tcm_pools[0].amem_head[0]);
		fn_data.vm_page_prot = vma->vm_page_prot;

		pte_fn = amem_tcm_init_pte_fn;
	} else {
		amem->state = AMEM_DISTANCE_RAM;
		fn_data.vm_page_prot = vma->vm_page_prot;

		pte_fn = amem_ram_init_pte_fn;
	}

	spin_unlock_irqrestore(&tcm_pools[0].lock, flags);

	pr_notice("%s vma->vm_start: 0x%lx size_pages: %d dist: %lx", __func__,
		  (size_t)vma->vm_start, size_pages, amem->state);

	return apply_to_page_range(vma->vm_mm, vma->vm_start, size_bytes,
				   pte_fn, &fn_data);
}

// Called when a subsequent AMEM_IOCTL_LOAD occurs.
static long amem_ioctl_load(struct amem *amem, unsigned long arg)
{
	tcm_pfn_t size_pages = amem->size_pages;
	size_t size_bytes = ((size_t)size_pages) << PAGE_SHIFT;
	unsigned long flags;

	struct vm_area_struct *vma = amem->vma;

	// Security and perf/power: TCM needs non-cached memory.
	struct pte_fn_data fn_data = {
		.amem = amem,
		.vma = vma,
		.spte = &amem->spt[0],
		.vm_page_prot = vma->vm_page_prot,
	};

	// Check if the buffer is already loaded in TCM
	if (amem->state == AMEM_DISTANCE_TCM)
		return 0;

	// Ensure that we have enough TCM available, otherwise fail

	spin_lock_irqsave(&tcm_pools[0].lock, flags);

	if (tcm_pools[0].free_count >= size_pages) {
		tcm_pools[0].free_count -= size_pages;
		amem->state = AMEM_DISTANCE_TCM;

		// Move the amem to the tcm_pool list
		list_add(&amem->amem_list, &tcm_pools[0].amem_head[0]);
	}

	spin_unlock_irqrestore(&tcm_pools[0].lock, flags);

	if (amem->state != AMEM_DISTANCE_TCM)
		return -ENOMEM;

	pr_notice("%s: vm_start: %lx, size_pages: %d", __func__,
		  (size_t)vma->vm_start, size_pages);

	return apply_to_page_range(vma->vm_mm, vma->vm_start, size_bytes,
				   amem_tcm_pte_fn, &fn_data);
}

// Called when AMEM_IOCTL_EVICT occurs.
static long amem_ioctl_evict(struct amem *amem, unsigned long arg)
{
	tcm_pfn_t size_pages = amem->size_pages;
	size_t size_bytes = ((size_t)size_pages) << PAGE_SHIFT;
	unsigned long flags;

	struct vm_area_struct *vma = amem->vma;
	struct pte_fn_data fn_data = {
		.amem = amem,
		.vma = vma,
		.spte = &amem->spt[0],
		.vm_page_prot = vma->vm_page_prot,
	};

	// Check if the buffer is already evicted to RAM
	if (amem->state == AMEM_DISTANCE_RAM)
		return 0;

	// Ensure amem_ioctl_initial_load has been called first
	if (amem->state == AMEM_DISTANCE_UNINITIALIZED)
		return -EINVAL;

	spin_lock_irqsave(&tcm_pools[0].lock, flags);

	list_del(&amem->amem_list);

	amem->state = AMEM_DISTANCE_RAM;

	spin_unlock_irqrestore(&tcm_pools[0].lock, flags);

	clean_invalidate_cache_range(vma->vm_start, size_bytes);

	apply_to_page_range(vma->vm_mm, vma->vm_start, size_bytes,
			    amem_ram_pte_fn, &fn_data);

	spin_lock_irqsave(&tcm_pools[0].lock, flags);

	// return the evicted pages to free_count
	tcm_pools[0].free_count += size_pages;

	spin_unlock_irqrestore(&tcm_pools[0].lock, flags);

	return 0;
}

// Supports the efficient running of one of three possible operations:
// LOAD: To bring the buffer into a closer memory
// EVICT: To push the buffer into a farther memory
// GET_ID: Return the amem_id (used to index into the amem_clients buffer)
static long amem_ioctl(struct file *filp, unsigned int cmd,
		       unsigned long arg)
{
	struct amem *amem = filp->private_data;
	size_t size_bytes;
	long ret;

	write_lock(&amem->state_lock);

	// Client must first call mmap on the amem (amem_mmap)
	if (!amem->vma) {
		write_unlock(&amem->state_lock);
		return -EINVAL;
	}

	switch (cmd) {
	case AMEM_IOCTL_LOAD:
		if (amem->state == AMEM_DISTANCE_UNINITIALIZED)
			ret = amem_ioctl_initial_load(amem, arg);
		else
			ret = amem_ioctl_load(amem, arg);
		break;

	case AMEM_IOCTL_EVICT:
		ret = amem_ioctl_evict(amem, arg);
		break;

	case AMEM_IOCTL_GET_ID:
		if (copy_to_user((void __user *)arg, &amem->amem_id,
				  sizeof(amem->amem_id)))
			ret = -EFAULT;
		else
			ret = 0;
		break;

	case AMEM_IOCTL_GET_SIZE:
		size_bytes = ((size_t)(amem->size_pages)) << PAGE_SHIFT;
		if (copy_to_user((void __user *)arg, &size_bytes,
				  sizeof(size_bytes)))
			ret = -EFAULT;
		else
			ret = 0;
		break;

	default:
		ret = -EINVAL;
	}

	write_unlock(&amem->state_lock);

	return ret;
}

static const struct file_operations amem_fops = {
	.owner = THIS_MODULE,
	.release = amem_release,
	.mmap = amem_mmap,
	.unlocked_ioctl = amem_ioctl,
};

// Undo the allocations done in amem_create_manager
static int amem_manager_release(struct inode *inode, struct file *filp)
{
	struct amem_manager *manager = filp->private_data;

	free_page((unsigned long)manager->mgr_client);
	kfree(manager);

	return 0;
}

// Is this needed? TODO: T187806292
void amem_manager_vm_open(struct vm_area_struct *vma)
{
	pr_notice("%s was called\n", __func__);
}

// called when VMA is destroyed, also its existence prevents the VMA
// from being "merged"
void amem_manager_vm_close(struct vm_area_struct *vma)
{
	struct file *filp = (struct file *)(vma->vm_private_data);

	pr_notice("%s was called\n", __func__);

	fput(filp);
}

// called when a page within the VMA has a page-fault.
vm_fault_t amem_manager_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct file *filp = (struct file *)(vma->vm_private_data);
	struct amem_manager *manager = filp->private_data;

	remap_pfn_range(vma, vma->vm_start,
			virt_to_pfn(manager->mgr_client),
			PAGE_SIZE,
			vma->vm_page_prot);

	return VM_FAULT_NOPAGE;
}

static const struct vm_operations_struct amem_manager_vm_ops = {
	.open = amem_manager_vm_open,
	.close = amem_manager_vm_close,
	.fault = amem_manager_vm_fault,
};

// Map the 'mgr_client' shared-memory configuration space.
// This area is used to share data between user and kernel space.
// Each amem buffer is given AMEM_SHARED_DATA_SIZE bytes as a communication
// channel between the user space API implementation of amem and this amem_kmd
// kernel driver.
static int amem_manager_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct amem_manager *manager = filp->private_data;
	size_t mem_size = vma->vm_end - vma->vm_start;

	if (manager->mgr_client == NULL)
		return -ESPIPE;

	if (mem_size != PAGE_SIZE)
		return -EINVAL;

	vma->vm_ops = &amem_manager_vm_ops;
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTCOPY;
	vma->vm_private_data = filp;

	// mmap shouldn't use the file for anything, the mappings are unrelated
	vma->vm_file = NULL;

	return 0;
}

// Currently unused
static long amem_manager_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	return -ENOSYS;
}

static const struct file_operations amem_manager_fops = {
	.owner = THIS_MODULE,
	.release = amem_manager_release,
	.mmap = amem_manager_mmap,
	.unlocked_ioctl = amem_manager_ioctl,
};

static int amem_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static int amem_init_open(struct inode *inode, struct file *filp)
{
	struct amem_context *context =
		container_of(inode->i_cdev, struct amem_context, cdev);

	// Save a copy of amem_context for use by amem_init_ioctl
	filp->private_data = context;

	return 0;
}

// Maps the entire TCM for test purposes
// Only allowed in AMEM_TCM_TEST_MODE!
static int amem_init_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct amem_context *context = filp->private_data;
	size_t mem_size = vma->vm_end - vma->vm_start;
	unsigned long flags;

	spin_lock_irqsave(&tcm_lock, flags);

	if (tcm_state != AMEM_TCM_TEST_MODE) {
		spin_unlock_irqrestore(&tcm_lock, flags);
		return -ENODEV;
	}

	if (mem_size > context->test_tcm_data->mem_size) {
		spin_unlock_irqrestore(&tcm_lock, flags);
		return -EINVAL;
	}

	spin_unlock_irqrestore(&tcm_lock, flags);

	return remap_pfn_range(vma, vma->vm_start,
		((unsigned long)context->test_tcm_data->phys_addr) >> PAGE_SHIFT,
		mem_size,
		vma->vm_page_prot);
}

// IOCTL command that creates a new 'amem_manager' object
static long amem_create_manager(struct file *filp,
				struct amem_create_manager_args *args);

// IOCTL command that creates a new 'amem' object
static long amem_create_amem(struct file *filp,
			     struct amem_create_args *args);

static long amem_init_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	union {
		struct amem_create_args		amem;
		struct amem_create_manager_args	amem_manager;
	} local_args;

	switch (cmd) {
	case AMEM_IOCTL_CREATE_AMEM_MANAGER:
		if (copy_from_user(&local_args, (void __user *)arg,
				   sizeof(local_args.amem_manager)))
			return -EFAULT;

		return amem_create_manager(filp, &local_args.amem_manager);

	case AMEM_IOCTL_CREATE_AMEM:
		if (copy_from_user(&local_args, (void __user *)arg,
				   sizeof(local_args.amem)))
			return -EFAULT;

		return amem_create_amem(filp, &local_args.amem);
	}
	return -EINVAL;
}

// The only non-test operations should be open() and unlocked_ioctl().
// Test mode allows the mmap() operation.
// The IOCTL is used to initialize the type of file, which will unlock other
// operations.
static const struct file_operations amem_init_fops = {
	.owner = THIS_MODULE,
	.open = amem_init_open,
	.mmap = amem_init_mmap,
	.unlocked_ioctl = amem_init_ioctl,
};


// IOCTL command that creates a new 'amem_manager' object
static long amem_create_manager(struct file *filp,
				struct amem_create_manager_args *args)
{
	struct amem_context *context;
	struct amem_manager *manager;
	unsigned long flags;
	u64 tmp;

	spin_lock_irqsave(&tcm_lock, flags);

	if (tcm_state != AMEM_TCM_ACTIVATED) {
		size_t prev_tcm_size;

		if (tcm_state == AMEM_TCM_TEST_MODE) {
			// amem_manager is incompatible with TEST_MODE
			spin_unlock_irqrestore(&tcm_lock, flags);
			return -ENOEXEC;
		}

		prev_tcm_size = tcm_phys_addr_end - tcm_phys_addr_base;

		llcc_tcm_data = llcc_tcm_activate();

		// Set the TCM addresses again in case they changed.
		tcm_virt_base = llcc_tcm_data->virt_addr;
		tcm_phys_addr_base = llcc_tcm_data->phys_addr;
		tcm_phys_addr_end = llcc_tcm_data->phys_addr +
				    llcc_tcm_data->mem_size;

		// Double check we get the same size as before
		if (prev_tcm_size != llcc_tcm_data->mem_size)
			BUG();

		pr_notice("AMEM_TCM_ACTIVATED");
		tcm_state = AMEM_TCM_ACTIVATED;
	}

	spin_unlock_irqrestore(&tcm_lock, flags);

	manager = kzalloc(sizeof(struct amem_manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	// Copy the client_tag as a u64 into the union.
	// This is union'ed with client_tag as a char[8].
	manager->client_tag_as_u64 = args->client_tag;

	// The final character must be the null terminator.
	if (manager->client_tag[7] != 0) {
		kfree(manager);
		return -EINVAL;
	}

	// The first 7 characters of client_tag must have ASCII codes
	// between 32-127.

	// Check that at least one of the two (5th or 6th) bits
	// are set in each character. Ensures chars are >= 32.
	// First, tmp shifts the 6th bit into the 5th bit position and or's
	// it with the unshifted 5th bit.
	// Next it checks that all chars in tmp have the 5th bit set.
	tmp = args->client_tag | (args->client_tag >> 1);
	if ((tmp & 0x20202020202020) != 0x20202020202020) {
		kfree(manager);
		return -EINVAL;
	}

	// Check that none of the 7th bits are set (ensures not > 127)
	if (args->client_tag & 0x8080808080808080) {
		kfree(manager);
		return -EINVAL;
	}

	// The first amem_id is 1 (the zero index is reserved for the manager).
	manager->amem_id_count = 1;

	// Allocate the shared memory page for mgr_client and amem_clients
	manager->mgr_client = (struct amem_manager_data *)get_zeroed_page(GFP_KERNEL);
	if (!manager->mgr_client) {
		kfree(manager);
		return -ENOMEM;
	}

	spin_lock_irqsave(&filp->f_lock, flags);

	// The f_op is the sentinel for the amem_create_manager operation.
	// Only one IOCTL can pass through amem_create_manager.
	if (filp->f_op != &amem_init_fops) {
		spin_unlock_irqrestore(&filp->f_lock, flags);
		free_page((unsigned long)manager->mgr_client);
		kfree(manager);
		return -EINVAL;
	}

	// This is the instance that passes.
	// filp->private_data is now guaranteed to be struct amem_context
	context = filp->private_data;

	// Now make the private_data be the amem_manager
	filp->private_data = manager;

	// Sets a new set of file ops that are specific to the manager.
	// WARNING: THIS MUST BE THE FINAL STEP BEFORE COMPLETION
	filp->f_op = &amem_manager_fops;

	spin_unlock_irqrestore(&filp->f_lock, flags);

	return 0;
}

// IOCTL command that creates a new 'amem' object
static long amem_create_amem(struct file *filp,
			     struct amem_create_args *args)
{
	struct amem_context *context;
	struct amem_manager *manager;
	struct amem *amem;
	struct amem_data *amem_data;
	struct amem_data initial_amem_data = {};
	unsigned long flags;
	struct file *other_file;
	size_t size_pages = args->size >> PAGE_SHIFT;
	size_t dynamic_extra = sizeof(struct spte_t) * size_pages;
	struct page *page;
	struct page *tmp;
	long err = -ENOMEM;
	int i;

	// Validate the size is page-aligned
	if (args->size != (size_pages << PAGE_SHIFT))
		return -EINVAL;

	// Check only valid flags
	if ((args->flags & AMEM_VALID_FLAGS) != args->flags)
		return -EINVAL;

	if (args->amem_id == 0)
		return -EINVAL;

	if (args->amem_id >= AMEM_PER_MANAGER)
		return -EINVAL;

	if (size_pages > __UINT16_MAX__)
		return -EINVAL;

	other_file = fget(args->amem_manager_fd);
	if (!other_file)
		return -EBADF;

	if (other_file->f_op != &amem_manager_fops) {
		// Not an amem_manager file
		fput(other_file);
		return -EBADF;
	}

	// Now we have a 'file*' (fd->file) for the associated amem_manager.
	// We can access its 'private_data'
	manager = other_file->private_data;

	amem_data = &manager->amem_clients[args->amem_id];

	// validate the amem_data shared memory is all zeros (except virt_addr
	// which should be all ones). This isn't a kernel safety issue.
	// This helps the client notice if it has corrupted itself.
	initial_amem_data.virt_addr = AMEM_VIRT_ADDR_INIT;
	if (memcmp(amem_data, &initial_amem_data,
	    sizeof(struct amem_data)) != 0) {
		return -EINVAL;
	}

	amem = kzalloc(sizeof(struct amem) + dynamic_extra, GFP_KERNEL);
	if (!amem) {
		fput(other_file);
		return -ENOMEM;
	}

	amem->manager_file = other_file;

	INIT_LIST_HEAD(&amem->amem_list);

	amem->flags = args->flags;
	amem->size_pages = size_pages;
	amem->max_latency_ns = args->max_latency_ns;
	amem->amem_id = args->amem_id;
	INIT_LIST_HEAD(&amem->free_pages);

	write_lock(&manager->lock);
	if (manager->amem_id_highest < amem->amem_id)
		manager->amem_id_highest = amem->amem_id;

	if (manager->amem_id_count == AMEM_PER_MANAGER) {
		write_unlock(&manager->lock);
		err = -EOVERFLOW;
		goto fail;
	}
	manager->amem_id_count++;
	write_unlock(&manager->lock);

	// Allocate all the (RAM) pages needed for this buffer
	for (i = 0; i < size_pages; i++) {

		// Security: Grab a zeroed page to ensure random kernel data
		//           doesn't leak!
		page = virt_to_page(get_zeroed_page(GFP_KERNEL));

		if (!page)
			goto fail;

		list_add(&page->slab_list, &amem->free_pages);
	}

	spin_lock_irqsave(&filp->f_lock, flags);

	// The f_op is the sentinel for the amem_create operation.
	// Only one IOCTL can pass through amem_create.
	if (filp->f_op != &amem_init_fops) {
		spin_unlock_irqrestore(&filp->f_lock, flags);
		err = -EINVAL;
		goto fail;
	}

	// This is the instance that passes.
	// filp->private_data is now guaranteed to be struct amem_context
	context = filp->private_data;

	// Now make the private_data be the amem
	filp->private_data = amem;

	// Sets a new set of file ops that are specific to the amem.
	// WARNING: THIS MUST BE THE FINAL STEP BEFORE COMPLETION
	filp->f_op = &amem_fops;

	spin_unlock_irqrestore(&filp->f_lock, flags);

	// Keep track of the total number of pages handled within /dev/amem
	atomic_add(size_pages, &context->page_count);

	return 0;

fail:
	fput(other_file);
	list_for_each_entry_safe(page, tmp, &amem->free_pages, slab_list) {
		list_del(&page->slab_list);
		__free_page(page);
	}
	kfree(amem);

	return err;
}

static ssize_t page_count_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct amem_context *context =
		(struct amem_context *)dev_get_drvdata(dev);

	if (atomic_read(&context->page_count) < 0)
		return scnprintf(buf, PAGE_SIZE, "unknown");

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 atomic_read(&context->page_count));
}

static ssize_t active_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct amem_context *context =
		(struct amem_context *)dev_get_drvdata(dev);
	size_t mem_size = 0;
	unsigned long flags;

	spin_lock_irqsave(&tcm_lock, flags);

	if (tcm_state != AMEM_TCM_DEACTIVATED && context->test_tcm_data)
		mem_size = context->test_tcm_data->mem_size / SZ_1K;

	spin_unlock_irqrestore(&tcm_lock, flags);

	return scnprintf(buf, PAGE_SIZE, "%ld\n", mem_size);
}

static ssize_t active_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct amem_context *context =
		(struct amem_context *)dev_get_drvdata(dev);
	int error = 0;
	long value = 0;
	unsigned long flags;

	error = kstrtol(buf, 10, &value);
	if (error)
		return error;

	spin_lock_irqsave(&tcm_lock, flags);

	if (context->test_tcm_data) {

		// DEACTIVATE is the only valid value here
		if (value != AMEM_TCM_DEACTIVATED) {
			spin_unlock_irqrestore(&tcm_lock, flags);
			return -EINVAL;
		}

		pr_notice("AMEM - llcc_tcm_deactivate()...");
		llcc_tcm_deactivate(context->test_tcm_data);
		context->test_tcm_data = NULL;

		// WARNING: Do not set tcm_state to AMEM_TCM_DEACTIVATED here!
		// For security reasons, once TEST_MODE is set, it must remain
		// until the system is rebooted.

		spin_unlock_irqrestore(&tcm_lock, flags);
		return count;
	}

	// Turning on TEST_MODE is the only valid value here
	if (value != AMEM_TCM_TEST_MODE) {
		spin_unlock_irqrestore(&tcm_lock, flags);
		return -EINVAL;
	}

	// Can't enter test mode if TCM is already activated
	if (tcm_state == AMEM_TCM_ACTIVATED) {
		spin_unlock_irqrestore(&tcm_lock, flags);
		return -EINVAL;
	}

	pr_notice("AMEM - llcc_tcm_activate()...");
	context->test_tcm_data = llcc_tcm_activate();
	tcm_state = AMEM_TCM_TEST_MODE;
	spin_unlock_irqrestore(&tcm_lock, flags);

	return count;
}

// Check the entire TCM region to see if it has a good test pattern
static ssize_t check_data_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct amem_context *context =
		(struct amem_context *)dev_get_drvdata(dev);
	int i = 0;
	char *ptr;
	u32 invalid_data_found = 0;

	if (context->test_tcm_data == NULL)
		return scnprintf(buf, PAGE_SIZE, "inactive\n");

	ptr = (char *)context->test_tcm_data->virt_addr;

	for (i = 0; i < context->test_tcm_data->mem_size; i++) {
		if (ptr[i] != i % 0x100)
			invalid_data_found++;
	}

	if (invalid_data_found == 0)
		return scnprintf(buf, PAGE_SIZE, "good pattern\n");
	else
		return scnprintf(buf, PAGE_SIZE, "BAD pattern!!!! ERRORS found: %d\n",
			invalid_data_found);
}

// Fill the data with the test pattern
// The |value| indicates the 1MB chunk of TCM to operate on.
// If the value is positive, a test pattern is written to the chunk.
// If the value is negative, the chunk is checked to see if it
// contains the test pattern. The return code indicates failure or success.
static ssize_t check_data_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct amem_context *context =
		(struct amem_context *)dev_get_drvdata(dev);
	int i = 0;
	char *ptr;
	int error = 0;
	long value = 0;
	long tcm_size_mb;

	error = kstrtol(buf, 10, &value);
	if (error)
		return error;

	if (context->test_tcm_data == NULL)
		return -EINVAL;

	tcm_size_mb = context->test_tcm_data->mem_size / SZ_1M;

	if (value == 0)
		return -EINVAL;

	if (value > tcm_size_mb)
		return -EINVAL;

	if (-value > tcm_size_mb)
		return -EINVAL;

	ptr = (char *)(llcc_tcm_get_virt_addr(context->test_tcm_data));
	pr_notice("AMEM - %s() %ldMB 0x%lx", __func__,
		  tcm_size_mb, (size_t)ptr);

	if (value > 0) {
		// select the chunk
		ptr += (value - 1) * SZ_1M;

		// Write the test pattern to the chunk
		for (i = 0; i < SZ_1M; i++) {
			if (i % SZ_128K == 0)
				pr_notice("AMEM - %s() 0x%x\n", __func__, i);

			ptr[i] = i % 0x100;
		}
	} else {
		u32 invalid_data_found = 0;

		// select the chunk
		value = -value;
		ptr += (value - 1) * SZ_1M;

		// Validate the test pattern within the chunk
		for (i = 0; i < SZ_1M; i++) {
			if (i % SZ_128K == 0)
				pr_notice("AMEM - %s() 0x%x\n", __func__, i);

			if (ptr[i] != i % 0x100)
				invalid_data_found++;
		}

		if (invalid_data_found != 0) {
			pr_notice("AMEM - ERRORS FOUND!!!! %s(): %d",
				  __func__, invalid_data_found);
			return -EINVAL;
		}

		pr_notice("AMEM - %s() SUCCESS", __func__);
	}

	return count;
}

static DEVICE_ATTR_RO(page_count);
static DEVICE_ATTR_RW(active);
static DEVICE_ATTR_RW(check_data);

static void amem_destroy_tcm_pools(void)
{
	int i;

	for (i = 0; i < NUM_TCM_POOLS; i++) {
		if (tcm_pools[i].heap) {
			int e;

			// check for leaks.
			BUG_ON(tcm_pools[i].free_count !=
			       tcm_heap_get_size(tcm_pools[i].heap));

			// check for leaks. These lists should be empty.
			for (e = 0; e < TCM_ACCESS_EXPONENT_SLOTS; e++)
				BUG_ON(!list_empty(&tcm_pools[i].amem_head[e]));

			tcm_heap_destroy(tcm_pools[i].heap);
			tcm_pools[i].heap = NULL;
		}
	}
}

static int amem_init_tcm_pools(void)
{
	tcm_pfn_t tcm_pfn_count;
	struct llcc_tcm_data *tmp_tcm_data;
	int i;
	int err = 0;

	// do a temporary activation to grab the address extents
	tmp_tcm_data = llcc_tcm_activate();
	tcm_virt_base = tmp_tcm_data->virt_addr;
	tcm_phys_addr_base = tmp_tcm_data->phys_addr;
	tcm_phys_addr_end = tmp_tcm_data->phys_addr + tmp_tcm_data->mem_size;

	// round down (don't want partial pages)
	tcm_pfn_count = tmp_tcm_data->mem_size >> PAGE_SHIFT;

	llcc_tcm_deactivate(tmp_tcm_data);

	// TODO: T190198152 Support additional pools
	for (i = 0; i < 1; i++) {
		int e;

		tcm_pools[i].heap = tcm_heap_create(tcm_pfn_count);
		if (!tcm_pools[i].heap) {
			err = -ENOMEM;
			break;
		}
		tcm_pools[i].free_count = tcm_pfn_count;

		for (e = 0; e < TCM_ACCESS_EXPONENT_SLOTS; e++)
			INIT_LIST_HEAD(&tcm_pools[i].amem_head[e]);
	}

	if (err < 0) {
		amem_destroy_tcm_pools();
		return err;
	}

	return 0;
}

static int amem_probe(struct platform_device *pdev)
{
	int err = 0;
	struct amem_context *context = NULL;

	context = kzalloc(sizeof(struct amem_context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;

	err = amem_init_tcm_pools();
	if (err < 0)
		goto error_init_tcm_pools;

	mutex_init(&context->lock);

	min_ram_phys_addr = PHYS_OFFSET;
	max_ram_phys_addr = memblock_end_of_DRAM();

	if ((max_ram_phys_addr >> PAGE_SHIFT) > MAX_AMEM_PFN) {
		err = -ENOMEM;
		pr_err("Too much memory on system. Please use CONFIG_AMEM_LARGE_MEMORY");
		goto error_too_much_memory;
	}

	dev_set_drvdata(&pdev->dev, context);

	err = alloc_chrdev_region(&context->devno, 0, 1, "amem");
	if (err < 0)
		goto error_chrdev_region;

	context->class = class_create(THIS_MODULE, "amem");
	if (IS_ERR_OR_NULL(context->class)) {
		context->class = NULL;
		goto error_class_create;
	}

	context->class->dev_uevent = amem_uevent;

	cdev_init(&context->cdev, &amem_init_fops);
	context->cdev.owner = THIS_MODULE;
	err = cdev_add(&context->cdev, context->devno, 1);
	if (err < 0)
		goto error_cdev_add;

	context->sysfs_device = device_create(context->class, NULL,
					      context->devno, NULL, "amem");
	if (IS_ERR_OR_NULL(context->sysfs_device)) {
		context->sysfs_device = NULL;
		goto error;
	}

	dev_set_drvdata(context->sysfs_device, context);

	err = device_create_file(context->sysfs_device, &dev_attr_page_count);
	if (err) {
		pr_err("Failed to create 'page_count' attr");
		goto error;
	}
	err = device_create_file(context->sysfs_device, &dev_attr_active);
	if (err) {
		pr_err("Failed to create 'active' attr");
		goto error;
	}
	err = device_create_file(context->sysfs_device, &dev_attr_check_data);
	if (err) {
		pr_err("Failed to create 'check_data' attr");
		goto error;
	}

	pr_notice("AMEM Adaptive Memory support initialized");
	return 0;

error:
	if (context->sysfs_device != NULL) {
		device_remove_file(context->sysfs_device, &dev_attr_page_count);
		device_remove_file(context->sysfs_device, &dev_attr_active);
		device_remove_file(context->sysfs_device, &dev_attr_check_data);

		dev_set_drvdata(context->sysfs_device, NULL);

		device_destroy(context->class, context->devno);
	}

	cdev_del(&context->cdev);

error_cdev_add:
	class_destroy(context->class);
error_class_create:
	unregister_chrdev_region(context->devno, 1);
error_chrdev_region:
	dev_set_drvdata(&pdev->dev, NULL);
	amem_destroy_tcm_pools();
error_too_much_memory:
	amem_destroy_tcm_pools();
error_init_tcm_pools:
	kfree(context);
	return err;
}

static void amem_shutdown(struct platform_device *pdev)
{
	struct amem_context *context =
		(struct amem_context *)dev_get_drvdata(&pdev->dev);

	device_remove_file(context->sysfs_device, &dev_attr_page_count);
	device_remove_file(context->sysfs_device, &dev_attr_active);
	device_remove_file(context->sysfs_device, &dev_attr_check_data);

	dev_set_drvdata(context->sysfs_device, NULL);

	device_destroy(context->class, context->devno);

	cdev_del(&context->cdev);

	class_destroy(context->class);

	unregister_chrdev_region(context->devno, 1);

	dev_set_drvdata(&pdev->dev, NULL);
	amem_destroy_tcm_pools();
	kfree(context);
}

static const struct of_device_id amem_match[] = {
	{ .compatible = "meta,amem" },
	{},
};
MODULE_DEVICE_TABLE(of, amem_match);

static struct platform_driver amem_driver = {
	.shutdown = amem_shutdown,
	.driver = {
		.name = "amem_kmd",
		.owner = THIS_MODULE,
		.of_match_table = amem_match,
	},
};
builtin_platform_driver_probe(amem_driver, amem_probe);

MODULE_AUTHOR("Travis Martin<travm@meta.com>");
MODULE_DESCRIPTION("Adaptive Memory driver");
MODULE_LICENSE("GPL v2");
