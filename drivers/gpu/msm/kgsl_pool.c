// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/scatterlist.h>
#include <linux/swap.h>

#include "kgsl_device.h"
#include "kgsl_pool.h"
#include "kgsl_sharedmem.h"

#define KGSL_MAX_POOLS 4
#define KGSL_MAX_POOL_ORDER 8
#define KGSL_MAX_RESERVED_PAGES 4096

/**
 * struct kgsl_page_pool - Structure to hold information for the pool
 * @pool_order: Page order describing the size of the page
 * @page_count: Number of pages currently present in the pool
 * @reserved_pages: Number of pages reserved at init for the pool
 * @allocation_allowed: Tells if reserved pool gets exhausted, can we allocate
 * from system memory
 * @max_pages: Limit on number of pages this pool can hold
 * @list_lock: Spinlock for page list in the pool
 * @page_list: List of pages held/reserved in this pool
 */
struct kgsl_page_pool {
	unsigned int pool_order;
	int page_count;
	unsigned int reserved_pages;
	bool allocation_allowed;
	unsigned int max_pages;
	spinlock_t list_lock;
	struct list_head page_list;
};

static struct kgsl_page_pool kgsl_pools[KGSL_MAX_POOLS];
static int kgsl_num_pools;
static int kgsl_pool_max_pages;


/* Returns KGSL pool corresponding to input page order*/
static struct kgsl_page_pool *
_kgsl_get_pool_from_order(unsigned int order)
{
	int i;

	for (i = 0; i < kgsl_num_pools; i++) {
		if (kgsl_pools[i].pool_order == order)
			return &kgsl_pools[i];
	}

	return NULL;
}

/* Add a page to specified pool */
static void
_kgsl_pool_add_page(struct kgsl_page_pool *pool, struct page *p)
{
	/*
	 * Sanity check to make sure we don't re-pool a page that
	 * somebody else has a reference to.
	 */
	if (WARN_ON_ONCE(unlikely(page_count(p) > 1))) {
		__free_pages(p, pool->pool_order);
		return;
	}

	kgsl_zero_page(p, pool->pool_order);

	spin_lock(&pool->list_lock);
	list_add_tail(&p->lru, &pool->page_list);
	pool->page_count++;
	spin_unlock(&pool->list_lock);
	mod_node_page_state(page_pgdat(p), NR_KERNEL_MISC_RECLAIMABLE,
			    (1 << pool->pool_order));
}

/* Returns a page from specified pool */
static struct page *
_kgsl_pool_get_page(struct kgsl_page_pool *pool)
{
	struct page *p = NULL;

	spin_lock(&pool->list_lock);
	if (pool->page_count) {
		p = list_first_entry(&pool->page_list, struct page, lru);
		pool->page_count--;
		list_del(&p->lru);
	}
	spin_unlock(&pool->list_lock);

	if (p != NULL)
		mod_node_page_state(page_pgdat(p),
				    NR_KERNEL_MISC_RECLAIMABLE,
				    -(1 << pool->pool_order));

	return p;
}

/* Returns the number of pages in specified pool */
static int
kgsl_pool_size(struct kgsl_page_pool *kgsl_pool)
{
	int size;

	spin_lock(&kgsl_pool->list_lock);
	size = kgsl_pool->page_count * (1 << kgsl_pool->pool_order);
	spin_unlock(&kgsl_pool->list_lock);

	return size;
}

/* Returns the number of pages in all kgsl page pools */
static int kgsl_pool_size_total(void)
{
	int i;
	int total = 0;

	for (i = 0; i < kgsl_num_pools; i++)
		total += kgsl_pool_size(&kgsl_pools[i]);
	return total;
}

/*
 * This will shrink the specified pool by num_pages or its pool_size,
 * whichever is smaller.
 */
static unsigned int
_kgsl_pool_shrink(struct kgsl_page_pool *pool, int num_pages)
{
	int j;
	unsigned int pcount = 0;

	if (pool == NULL || num_pages <= 0)
		return pcount;

	for (j = 0; j < num_pages >> pool->pool_order; j++) {
		struct page *page = _kgsl_pool_get_page(pool);

		if (page != NULL) {
			__free_pages(page, pool->pool_order);
			pcount += (1 << pool->pool_order);
		} else {
			/* Break as this pool is empty */
			break;
		}
	}

	return pcount;
}

/*
 * This function reduces the total pool size
 * to number of pages specified by target_pages.
 *
 * If target_pages are greater than current pool size
 * nothing needs to be done otherwise remove
 * (current_pool_size - target_pages) pages from pool
 * starting from higher order pool.
 */
static unsigned long
kgsl_pool_reduce(unsigned int target_pages, bool exit)
{
	int total_pages = 0;
	int i;
	int nr_removed;
	struct kgsl_page_pool *pool;
	unsigned long pcount = 0;

	total_pages = kgsl_pool_size_total();

	for (i = (kgsl_num_pools - 1); i >= 0; i--) {
		pool = &kgsl_pools[i];

		/*
		 * Only reduce the pool sizes for pools which are allowed to
		 * allocate memory unless we are at close, in which case the
		 * reserved memory for all pools needs to be freed
		 */
		if (!pool->allocation_allowed && !exit)
			continue;

		nr_removed = total_pages - target_pages - pcount;
		if (nr_removed <= 0)
			return pcount;

		/* Round up to integral number of pages in this pool */
		nr_removed = ALIGN(nr_removed, 1 << pool->pool_order);

		/* Remove nr_removed pages from this pool*/
		pcount += _kgsl_pool_shrink(pool, nr_removed);
	}

	return pcount;
}

/**
 * kgsl_pool_free_pages() - Free pages in the pages array
 * @pages: pointer of the pages array
 *
 * Free the pages by collapsing any physical adjacent pages.
 * Pages are added back to the pool, if pool has sufficient space
 * otherwise they are given back to system.
 */
void kgsl_pool_free_pages(struct page **pages, unsigned int pcount)
{
	int i;

	if (pages == NULL || pcount == 0)
		return;

	if (WARN(!kern_addr_valid((unsigned long)pages),
		"Address of pages=%pK is not valid\n", pages))
		return;

	for (i = 0; i < pcount;) {
		/*
		 * Free each page or compound page group individually.
		 */
		struct page *p = pages[i];

		if (WARN(!kern_addr_valid((unsigned long)p),
			"Address of page=%pK is not valid\n", p))
			return;

		i += 1 << compound_order(p);
		kgsl_pool_free_page(p);
	}
}
static int kgsl_pool_idx_lookup(unsigned int order)
{
	int i;

	for (i = 0; i < kgsl_num_pools; i++)
		if (order == kgsl_pools[i].pool_order)
			return i;

	return -ENOMEM;
}

static int kgsl_pool_get_retry_order(unsigned int order)
{
	int i;

	for (i = kgsl_num_pools-1; i > 0; i--)
		if (order >= kgsl_pools[i].pool_order)
			return kgsl_pools[i].pool_order;

	return 0;
}

/**
 * kgsl_pool_alloc_page() - Allocate a page of requested size
 * @page_size: Size of the page to be allocated
 * @pages: pointer to hold list of pages, should be big enough to hold
 * requested page
 * @len: Length of array pages.
 *
 * Return total page count on success and negative value on failure
 */
int kgsl_pool_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			struct kgsl_memdesc *memdesc)
{
	int j;
	int pcount = 0;
	struct kgsl_page_pool *pool;
	struct page *page = NULL;
	struct page *p = NULL;
	int order = get_order(*page_size);
	int pool_idx;
	size_t size = 0;

	if ((pages == NULL) || pages_len < (*page_size >> PAGE_SHIFT))
		return -EINVAL;

	/* If the pool is not configured get pages from the system */
	if (!kgsl_num_pools) {
		gfp_t gfp_mask = kgsl_gfp_mask(order);

		page = alloc_pages(gfp_mask, order);
		if (page == NULL) {
			/* Retry with lower order pages */
			if (order > 0) {
				size = PAGE_SIZE << --order;
				goto eagain;

			} else
				return -ENOMEM;
		}
		kgsl_zero_page(page, order);
		goto done;
	}

	pool = _kgsl_get_pool_from_order(order);
	if (pool == NULL) {
		/* Retry with lower order pages */
		if (order > 0) {
			size = PAGE_SIZE << kgsl_pool_get_retry_order(order);
			goto eagain;
		} else {
			/*
			 * Fall back to direct allocation in case
			 * pool with zero order is not present
			 */
			gfp_t gfp_mask = kgsl_gfp_mask(order);

			page = alloc_pages(gfp_mask, order);
			if (page == NULL)
				return -ENOMEM;
			kgsl_zero_page(page, order);
			goto done;
		}
	}

	pool_idx = kgsl_pool_idx_lookup(order);
	page = _kgsl_pool_get_page(pool);

	/* Allocate a new page if not allocated from pool */
	if (page == NULL) {
		gfp_t gfp_mask = kgsl_gfp_mask(order);

		/* Only allocate non-reserved memory for certain pools */
		if (!pool->allocation_allowed && pool_idx > 0) {
			size = PAGE_SIZE <<
					kgsl_pools[pool_idx-1].pool_order;
			goto eagain;
		}

		page = alloc_pages(gfp_mask, order);

		if (!page) {
			if (pool_idx > 0) {
				/* Retry with lower order pages */
				size = PAGE_SIZE <<
					kgsl_pools[pool_idx-1].pool_order;
				goto eagain;
			} else
				return -ENOMEM;
		}

		kgsl_zero_page(page, order);
	}

done:
	for (j = 0; j < (*page_size >> PAGE_SHIFT); j++) {
		p = nth_page(page, j);
		pages[pcount] = p;
		pcount++;
	}

	mod_node_page_state(page_pgdat(page), NR_UNRECLAIMABLE_PAGES,
					(1 << order));
	return pcount;

eagain:
	*page_size = kgsl_get_page_size(size,
			ilog2(size), memdesc);
	*align = ilog2(*page_size);
	return -EAGAIN;
}

void kgsl_pool_free_page(struct page *page)
{
	struct kgsl_page_pool *pool;
	int page_order;

	if (page == NULL)
		return;

	page_order = compound_order(page);

	mod_node_page_state(page_pgdat(page), NR_UNRECLAIMABLE_PAGES,
					-(1 << page_order));

	if (!kgsl_pool_max_pages ||
			(kgsl_pool_size_total() < kgsl_pool_max_pages)) {
		pool = _kgsl_get_pool_from_order(page_order);
		if (pool && (pool->page_count < pool->max_pages)) {
			_kgsl_pool_add_page(pool, page);
			return;
		}
	}

	/* Give back to system as not added to pool */
	__free_pages(page, page_order);
}

/*
 * Return true if the pool of specified page size is supported
 * or no pools are supported otherwise return false.
 */
bool kgsl_pool_avaialable(int page_size)
{
	int i;

	if (!kgsl_num_pools)
		return true;

	for (i = 0; i < kgsl_num_pools; i++)
		if (ilog2(page_size >> PAGE_SHIFT) == kgsl_pools[i].pool_order)
			return true;

	return false;
}

static void kgsl_pool_reserve_pages(void)
{
	int i, j;
	unsigned int page_count;

	for (i = 0; i < kgsl_num_pools; i++) {
		struct page *page;

		page_count = min_t(unsigned int, kgsl_pools[i].max_pages,
				kgsl_pools[i].reserved_pages);
		for (j = 0; j < page_count; j++) {
			int order = kgsl_pools[i].pool_order;
			gfp_t gfp_mask = kgsl_gfp_mask(order);

			page = alloc_pages(gfp_mask, order);
			if (page != NULL)
				_kgsl_pool_add_page(&kgsl_pools[i], page);
		}
	}
}

/* Functions for the shrinker */

static unsigned long
kgsl_pool_shrink_scan_objects(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	/* nr represents number of pages to be removed*/
	int nr = sc->nr_to_scan;
	int total_pages = kgsl_pool_size_total();

	/* Target pages represents new  pool size */
	int target_pages = (nr > total_pages) ? 0 : (total_pages - nr);

	/* Reduce pool size to target_pages */
	return kgsl_pool_reduce(target_pages, false);
}

static unsigned long
kgsl_pool_shrink_count_objects(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	/* Trigger mem_workqueue flush to free memory */
	kgsl_schedule_work(&kgsl_driver.mem_work);

	/* Return total pool size as everything in pool can be freed */
	return kgsl_pool_size_total();
}

/* Shrinker callback data*/
static struct shrinker kgsl_pool_shrinker = {
	.count_objects = kgsl_pool_shrink_count_objects,
	.scan_objects = kgsl_pool_shrink_scan_objects,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};

static void kgsl_pool_config(unsigned int order, unsigned int reserved_pages,
		bool allocation_allowed, unsigned int max_pages)
{
#ifdef CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS
	if (order > 0) {
		pr_err("kgsl: pool order:%d not supprted\n", order);
		return;
	}
#endif
	if ((order > KGSL_MAX_POOL_ORDER) ||
			(reserved_pages > KGSL_MAX_RESERVED_PAGES))
		return;

	kgsl_pools[kgsl_num_pools].pool_order = order;
	kgsl_pools[kgsl_num_pools].reserved_pages = reserved_pages;
	kgsl_pools[kgsl_num_pools].allocation_allowed = allocation_allowed;
	kgsl_pools[kgsl_num_pools].max_pages = max_pages;
	spin_lock_init(&kgsl_pools[kgsl_num_pools].list_lock);
	INIT_LIST_HEAD(&kgsl_pools[kgsl_num_pools].page_list);
	kgsl_num_pools++;
}

static void kgsl_of_parse_mempools(struct device_node *node)
{
	struct device_node *child;
	unsigned int page_size, reserved_pages = 0, max_pages = UINT_MAX;
	bool allocation_allowed;

	for_each_child_of_node(node, child) {
		unsigned int index;

		if (of_property_read_u32(child, "reg", &index))
			return;

		if (index >= KGSL_MAX_POOLS)
			continue;

		if (of_property_read_u32(child, "qcom,mempool-page-size",
					&page_size))
			return;

		of_property_read_u32(child, "qcom,mempool-reserved",
				&reserved_pages);

		allocation_allowed = of_property_read_bool(child,
				"qcom,mempool-allocate");

		of_property_read_u32(child, "qcom,mempool-max-pages",
				&max_pages);

		kgsl_pool_config(ilog2(page_size >> PAGE_SHIFT), reserved_pages,
				allocation_allowed, max_pages);
	}
}

static void kgsl_of_get_mempools(struct device_node *parent)
{
	struct device_node *node = NULL;

	/*
	 * If available memory is less than 2GB first
	 * check for low memory pool configuration. If
	 * low memory configuration is not specified
	 * then fallback to default pool configuration.
	 */
	if (totalram_pages < (SZ_2G >> PAGE_SHIFT))
		node = of_find_compatible_node(parent, NULL,
				"qcom,gpu-mempools-lowmem");

	if (node == NULL)
		node = of_find_compatible_node(parent, NULL,
				"qcom,gpu-mempools");

	if (node != NULL) {
		/* Get Max pages limit for mempool */
		of_property_read_u32(node, "qcom,mempool-max-pages",
				&kgsl_pool_max_pages);
		kgsl_of_parse_mempools(node);
	}
}

void kgsl_init_page_pools(struct kgsl_device *device)
{
	if (device->flags & KGSL_FLAG_USE_SHMEM)
		return;

	/* Get GPU mempools data and configure pools */
	kgsl_of_get_mempools(device->pdev->dev.of_node);

	/* Reserve the appropriate number of pages for each pool */
	kgsl_pool_reserve_pages();

	/* Initialize shrinker */
	register_shrinker(&kgsl_pool_shrinker);
}

void kgsl_exit_page_pools(void)
{
	/* Release all pages in pools, if any.*/
	kgsl_pool_reduce(0, true);

	/* Unregister shrinker */
	unregister_shrinker(&kgsl_pool_shrinker);
}

