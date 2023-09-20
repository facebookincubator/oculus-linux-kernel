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

#ifdef CONFIG_QCOM_KGSL_PAGE_POOLING

/**
 * struct kgsl_page_pool - Structure to hold information for the pool
 * @pool_order: Page order describing the size of the page
 * @page_count: Number of pages currently present in the pool
 * @allocation_allowed: Tells if reserved pool gets exhausted, can we allocate
 * from system memory
 * @list_lock: Spinlock for page list in the pool
 * @page_list: List of pages held/reserved in this pool
 */
struct kgsl_page_pool {
	unsigned int pool_order;
	int page_count;
	bool allocation_allowed;
	spinlock_t list_lock;
	struct list_head page_list;
};

static struct kgsl_page_pool kgsl_pools[4];
static int kgsl_num_pools;
static int kgsl_pool_max_pages;

/* Return the index of the pool for the specified order */
static int kgsl_get_pool_index(int order)
{
	int i;

	for (i = 0; i < kgsl_num_pools; i++) {
		if (kgsl_pools[i].pool_order == order)
			return i;
	}

	return -EINVAL;
}

/* Returns KGSL pool corresponding to input page order*/
static struct kgsl_page_pool *
_kgsl_get_pool_from_order(int order)
{
	int index = kgsl_get_pool_index(order);

	return index >= 0 ? &kgsl_pools[index] : NULL;
}

/* Add a page to specified pool */
static void
_kgsl_pool_add_page(struct kgsl_page_pool *pool, struct page *p)
{
	if (!p)
		return;

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

/* Returns the number of pages in all kgsl page pools */
static int kgsl_pool_size_total(void)
{
	int i;
	int total = 0;

	for (i = 0; i < kgsl_num_pools; i++) {
		struct kgsl_page_pool *kgsl_pool = &kgsl_pools[i];

		spin_lock(&kgsl_pool->list_lock);
		total += kgsl_pool->page_count * (1 << kgsl_pool->pool_order);
		spin_unlock(&kgsl_pool->list_lock);
	}

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

		if (!page)
			break;

		__free_pages(page, pool->pool_order);
		pcount += (1 << pool->pool_order);
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

static int kgsl_pool_get_retry_order(unsigned int order)
{
	int i;

	for (i = kgsl_num_pools-1; i > 0; i--)
		if (order >= kgsl_pools[i].pool_order)
			return kgsl_pools[i].pool_order;

	return 0;
}

/*
 * Return true if the pool of specified page size is supported
 * or no pools are supported otherwise return false.
 */
static bool kgsl_pool_available(unsigned int page_size)
{
	int order = ilog2(page_size >> PAGE_SHIFT);

	if (!kgsl_num_pools)
		return true;

	return (kgsl_get_pool_index(order) >= 0);
}

/**
 * kgsl_get_page_size() - Get supported pagesize
 * @size: Size of the page
 * @align: Desired alignment of the size
 *
 * Return supported pagesize
 */
#if !defined(CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS)
static inline int kgsl_get_page_size(size_t size, unsigned int align,
		struct kgsl_memdesc *memdesc)
{
	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM)
		return PAGE_SIZE;

	if (align >= ilog2(SZ_1M) && size >= SZ_1M &&
		kgsl_pool_available(SZ_1M))
		return SZ_1M;
	else if (align >= ilog2(SZ_64K) && size >= SZ_64K &&
		kgsl_pool_available(SZ_64K))
		return SZ_64K;
	else if (align >= ilog2(SZ_8K) && size >= SZ_8K &&
		kgsl_pool_available(SZ_8K))
		return SZ_8K;
	else
		return PAGE_SIZE;
}
#else
static inline int kgsl_get_page_size(size_t size, unsigned int align,
		struct kgsl_memdesc *memdesc)
{
	return PAGE_SIZE;
}
#endif

/*
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

	pool_idx = kgsl_get_pool_index(order);
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

	if (!kgsl_pool_max_pages ||
			(kgsl_pool_size_total() < kgsl_pool_max_pages)) {
		pool = _kgsl_get_pool_from_order(page_order);
		if (pool != NULL) {
			_kgsl_pool_add_page(pool, page);
			return;
		}
	}

	/* Give back to system as not added to pool */
	__free_pages(page, page_order);
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

static void kgsl_pool_reserve_pages(struct kgsl_page_pool *pool,
		struct device_node *node)
{

	u32 reserved = 0;
	int i;

	of_property_read_u32(node, "qcom,mempool-reserved", &reserved);

	/* Limit the total number of reserved pages to 4096 */
	reserved = min_t(u32, reserved, 4096);

	for (i = 0; i < reserved; i++) {
		gfp_t gfp_mask = kgsl_gfp_mask(pool->pool_order);
		struct page *page;

		page = alloc_pages(gfp_mask, pool->pool_order);
		_kgsl_pool_add_page(pool, page);
	}
}

static int kgsl_of_parse_mempool(struct kgsl_page_pool *pool,
		struct device_node *node)
{
	u32 size;
	int order;

	if (of_property_read_u32(node, "qcom,mempool-page-size", &size))
		return -EINVAL;

	order = ilog2(size >> PAGE_SHIFT);

	if (order > 8) {
		pr_err("kgsl: %pOF: pool order %d is too big\n", node, order);
		return -EINVAL;
	}

	pool->pool_order = order;
	pool->allocation_allowed = of_property_read_bool(node,
		"qcom,mempool-allocate");

	spin_lock_init(&pool->list_lock);
	INIT_LIST_HEAD(&pool->page_list);

	kgsl_pool_reserve_pages(pool, node);

	return 0;
}

static void kgsl_of_get_mempools(struct device_node *parent)
{
	struct device_node *node, *child;
	int index = 0;

	node = of_find_compatible_node(parent, NULL, "qcom,gpu-mempools");
	if (!node)
		return;

	/* Get Max pages limit for mempool */
	of_property_read_u32(node, "qcom,mempool-max-pages",
			&kgsl_pool_max_pages);

	for_each_child_of_node(node, child) {
		if (!kgsl_of_parse_mempool(&kgsl_pools[index], child))
			index++;

		if (index == ARRAY_SIZE(kgsl_pools)) {
			of_node_put(child);
			break;
		}
	}

	kgsl_num_pools = index;
	of_node_put(node);
}

void kgsl_init_page_pools(struct kgsl_device *device)
{
	if (device->flags & KGSL_FLAG_USE_SHMEM)
		return;

	/* Get GPU mempools data and configure pools */
	kgsl_of_get_mempools(device->pdev->dev.of_node);

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
#else /* CONFIG_QCOM_KGSL_PAGE_POOLING */
static int kgsl_get_page_size(size_t size, unsigned int align,
		struct kgsl_memdesc *memdesc)
{
	return PAGE_SIZE;
}

int kgsl_pool_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			struct kgsl_memdesc *memdesc)
{
	struct page *page = NULL;
	gfp_t gfp_mask = kgsl_gfp_mask(0);

	if (pages == NULL || pages_len == 0)
		return -EINVAL;

	page = alloc_pages(gfp_mask, 0);
	if (page == NULL)
		return -ENOMEM;

	kgsl_zero_page(page, 0);

	*pages = page;

	*page_size = PAGE_SIZE;
	*align = PAGE_SHIFT;

	return 1;
}

void kgsl_pool_free_page(struct page *page)
{
	int page_order;

	if (page == NULL)
		return;

	page_order = compound_order(page);

	__free_pages(page, page_order);
}

void kgsl_init_page_pools(struct platform_device *pdev) {}

void kgsl_exit_page_pools(void) {}
#endif /* !CONFIG_QCOM_KGSL_PAGE_POOLING */

int kgsl_pool_alloc_pages(struct kgsl_memdesc *memdesc, u64 size,
		struct page ***pages)
{
	int count = 0;
	int npages = size >> PAGE_SHIFT;
	struct page **local = kvcalloc(npages, sizeof(*local), GFP_KERNEL);
	u32 page_size, align;
	u64 len = size;
	bool memwq_flush_done = false;

	if (!local)
		return -ENOMEM;

	/* Start with 1MB alignment to get the biggest page we can */
	align = ilog2(SZ_1M);

	page_size = kgsl_get_page_size(len, align, memdesc);

	while (len) {
		int ret = kgsl_alloc_page(&page_size, &local[count],
				npages, &align, memdesc, count);

		if (ret == -EAGAIN)
			continue;
		else if (ret == -ENOMEM && !memwq_flush_done &&
				!kgsl_sharedmem_get_noretry()) {
			/* if OoM, retry once after flushing mem_wq */
			flush_workqueue(kgsl_driver.mem_workqueue);
			memwq_flush_done = true;
			continue;
		} else if (ret <= 0) {
			int i;

			for (i = 0; i < count; ) {
				int n = 1 << compound_order(local[i]);

				kgsl_free_page(memdesc, local[i]);
				i += n;
			}
			kvfree(local);

			if (!kgsl_sharedmem_get_noretry())
				pr_err_ratelimited("kgsl: out of memory: only allocated %lldKb of %lldKb requested\n",
					(size - len) >> 10, size >> 10);

			return -ENOMEM;
		}

		count += ret;
		npages -= ret;
		len -= page_size;

		page_size = kgsl_get_page_size(len, align, memdesc);
	}

	*pages = local;

	return count;
}
