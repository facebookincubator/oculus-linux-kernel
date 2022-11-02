/* SPDX-License-Identifier: GPL-2.0
 *
 * page_pool.c
 *	Author:	Jesper Dangaard Brouer <netoptimizer@brouer.com>
 *	Copyright (C) 2016 Red Hat, Inc.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <net/page_pool.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/page-flags.h>
#include <linux/mm.h> /* for __put_page() */

static int page_pool_init(struct page_pool *pool,
			  const struct page_pool_params *params)
{
	unsigned int ring_qsize = 1024; /* Default */

	memcpy(&pool->p, params, sizeof(pool->p));

	/* Validate only known flags were used */
	if (pool->p.flags & ~(PP_FLAG_ALL))
		return -EINVAL;

	if (pool->p.pool_size)
		ring_qsize = pool->p.pool_size;

	/* Sanity limit mem that can be pinned down */
	if (ring_qsize > 32768)
		return -E2BIG;

	/* DMA direction is either DMA_FROM_DEVICE or DMA_BIDIRECTIONAL.
	 * DMA_BIDIRECTIONAL is for allowing page used for DMA sending,
	 * which is the XDP_TX use-case.
	 */
	if ((pool->p.dma_dir != DMA_FROM_DEVICE) &&
	    (pool->p.dma_dir != DMA_BIDIRECTIONAL))
		return -EINVAL;

	if (ptr_ring_init(&pool->ring, ring_qsize, GFP_KERNEL) < 0)
		return -ENOMEM;

	return 0;
}

struct page_pool *page_pool_create(const struct page_pool_params *params)
{
	struct page_pool *pool;
	int err = 0;

	pool = kzalloc_node(sizeof(*pool), GFP_KERNEL, params->nid);
	if (!pool)
		return ERR_PTR(-ENOMEM);

	err = page_pool_init(pool, params);
	if (err < 0) {
		pr_warn("%s() gave up with errno %d\n", __func__, err);
		kfree(pool);
		return ERR_PTR(err);
	}
	return pool;
}
EXPORT_SYMBOL(page_pool_create);

/* fast path */
static struct page *__page_pool_get_cached(struct page_pool *pool)
{
	struct ptr_ring *r = &pool->ring;
	struct page *page;

	/* Quicker fallback, avoid locks when ring is empty */
	if (__ptr_ring_empty(r))
		return NULL;

	/* Test for safe-context, caller should provide this guarantee */
	if (likely(in_serving_softirq())) {
		if (likely(pool->alloc.count)) {
			/* Fast-path */
			page = pool->alloc.cache[--pool->alloc.count];
			return page;
		}
		/* Slower-path: Alloc array empty, time to refill
		 *
		 * Open-coded bulk ptr_ring consumer.
		 *
		 * Discussion: the ring consumer lock is not really
		 * needed due to the softirq/NAPI protection, but
		 * later need the ability to reclaim pages on the
		 * ring. Thus, keeping the locks.
		 */
		spin_lock(&r->consumer_lock);
		while ((page = __ptr_ring_consume(r))) {
			if (pool->alloc.count == PP_ALLOC_CACHE_REFILL)
				break;
			pool->alloc.cache[pool->alloc.count++] = page;
		}
		spin_unlock(&r->consumer_lock);
		return page;
	}

	/* Slow-path: Get page from locked ring queue */
	page = ptr_ring_consume(&pool->ring);
	return page;
}

/* slow path */
noinline
static struct page *__page_pool_alloc_pages_slow(struct page_pool *pool,
						 gfp_t _gfp)
{
	struct page *page;
	gfp_t gfp = _gfp;
	dma_addr_t dma;

	/* We could always set __GFP_COMP, and avoid this branch, as
	 * prep_new_page() can handle order-0 with __GFP_COMP.
	 */
	if (pool->p.order)
		gfp |= __GFP_COMP;

	/* FUTURE development:
	 *
	 * Current slow-path essentially falls back to single page
	 * allocations, which doesn't improve performance.  This code
	 * need bulk allocation support from the page allocator code.
	 */

	/* Cache was empty, do real allocation */
	page = alloc_pages_node(pool->p.nid, gfp, pool->p.order);
	if (!page)
		return NULL;

	if (!(pool->p.flags & PP_FLAG_DMA_MAP))
		goto skip_dma_map;

	/* Setup DMA mapping: use page->private for DMA-addr
	 * This mapping is kept for lifetime of page, until leaving pool.
	 */
	dma = dma_map_page(pool->p.dev, page, 0,
			   (PAGE_SIZE << pool->p.order),
			   pool->p.dma_dir);
	if (dma_mapping_error(pool->p.dev, dma)) {
		put_page(page);
		return NULL;
	}
	set_page_private(page, dma); /* page->private = dma; */

skip_dma_map:
	/* When page just alloc'ed is should/must have refcnt 1. */
	return page;
}

/* For using page_pool replace: alloc_pages() API calls, but provide
 * synchronization guarantee for allocation side.
 */
struct page *page_pool_alloc_pages(struct page_pool *pool, gfp_t gfp)
{
	struct page *page;

	/* Fast-path: Get a page from cache */
	page = __page_pool_get_cached(pool);
	if (page)
		return page;

	/* Slow-path: cache empty, do real allocation */
	page = __page_pool_alloc_pages_slow(pool, gfp);
	return page;
}
EXPORT_SYMBOL(page_pool_alloc_pages);

/* Cleanup page_pool state from page */
static void __page_pool_clean_page(struct page_pool *pool,
				   struct page *page)
{
	if (!(pool->p.flags & PP_FLAG_DMA_MAP))
		return;

	/* DMA unmap */
	dma_unmap_page(pool->p.dev, page_private(page),
		       PAGE_SIZE << pool->p.order, pool->p.dma_dir);
	set_page_private(page, 0);
}

/* Return a page to the page allocator, cleaning up our state */
static void __page_pool_return_page(struct page_pool *pool, struct page *page)
{
	__page_pool_clean_page(pool, page);
	put_page(page);
	/* An optimization would be to call __free_pages(page, pool->p.order)
	 * knowing page is not part of page-cache (thus avoiding a
	 * __page_cache_release() call).
	 */
}

static bool __page_pool_recycle_into_ring(struct page_pool *pool,
				   struct page *page)
{
	int ret;
	/* BH protection not needed if current is serving softirq */
	if (in_serving_softirq())
		ret = ptr_ring_produce(&pool->ring, page);
	else
		ret = ptr_ring_produce_bh(&pool->ring, page);

	return (ret == 0) ? true : false;
}

/* Only allow direct recycling in special circumstances, into the
 * alloc side cache.  E.g. during RX-NAPI processing for XDP_DROP use-case.
 *
 * Caller must provide appropriate safe context.
 */
static bool __page_pool_recycle_direct(struct page *page,
				       struct page_pool *pool)
{
	if (unlikely(pool->alloc.count == PP_ALLOC_CACHE_SIZE))
		return false;

	/* Caller MUST have verified/know (page_ref_count(page) == 1) */
	pool->alloc.cache[pool->alloc.count++] = page;
	return true;
}

void __page_pool_put_page(struct page_pool *pool,
			  struct page *page, bool allow_direct)
{
	/* This allocator is optimized for the XDP mode that uses
	 * one-frame-per-page, but have fallbacks that act like the
	 * regular page allocator APIs.
	 *
	 * refcnt == 1 means page_pool owns page, and can recycle it.
	 */
	if (likely(page_ref_count(page) == 1)) {
		/* Read barrier done in page_ref_count / READ_ONCE */

		if (allow_direct && in_serving_softirq())
			if (__page_pool_recycle_direct(page, pool))
				return;

		if (!__page_pool_recycle_into_ring(pool, page)) {
			/* Cache full, fallback to free pages */
			__page_pool_return_page(pool, page);
		}
		return;
	}
	/* Fallback/non-XDP mode: API user have elevated refcnt.
	 *
	 * Many drivers split up the page into fragments, and some
	 * want to keep doing this to save memory and do refcnt based
	 * recycling. Support this use case too, to ease drivers
	 * switching between XDP/non-XDP.
	 *
	 * In-case page_pool maintains the DMA mapping, API user must
	 * call page_pool_put_page once.  In this elevated refcnt
	 * case, the DMA is unmapped/released, as driver is likely
	 * doing refcnt based recycle tricks, meaning another process
	 * will be invoking put_page.
	 */
	__page_pool_clean_page(pool, page);
	put_page(page);
}
EXPORT_SYMBOL(__page_pool_put_page);

static void __page_pool_empty_ring(struct page_pool *pool)
{
	struct page *page;

	/* Empty recycle ring */
	while ((page = ptr_ring_consume_bh(&pool->ring))) {
		/* Verify the refcnt invariant of cached pages */
		if (!(page_ref_count(page) == 1))
			pr_crit("%s() page_pool refcnt %d violation\n",
				__func__, page_ref_count(page));

		__page_pool_return_page(pool, page);
	}
}

static void __page_pool_destroy_rcu(struct rcu_head *rcu)
{
	struct page_pool *pool;

	pool = container_of(rcu, struct page_pool, rcu);

	WARN(pool->alloc.count, "API usage violation");

	__page_pool_empty_ring(pool);
	ptr_ring_cleanup(&pool->ring, NULL);
	kfree(pool);
}

/* Cleanup and release resources */
void page_pool_destroy(struct page_pool *pool)
{
	struct page *page;

	/* Empty alloc cache, assume caller made sure this is
	 * no-longer in use, and page_pool_alloc_pages() cannot be
	 * call concurrently.
	 */
	while (pool->alloc.count) {
		page = pool->alloc.cache[--pool->alloc.count];
		__page_pool_return_page(pool, page);
	}

	/* No more consumers should exist, but producers could still
	 * be in-flight.
	 */
	__page_pool_empty_ring(pool);

	/* An xdp_mem_allocator can still ref page_pool pointer */
	call_rcu(&pool->rcu, __page_pool_destroy_rcu);
}
EXPORT_SYMBOL(page_pool_destroy);
