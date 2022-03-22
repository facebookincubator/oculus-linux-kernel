// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 */

#include <linux/delay.h>
#include <linux/iommu.h>
#include <linux/moduleparam.h>

#include "kgsl_device.h"
#include "kgsl_iommu.h"
#include "kgsl_lazy.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl."

/*
 * Kernel parameter option to force lazy allocation on/off.
 * 0: No forced behavior
 * 1: Force lazy allocation for all supported entries
 * 2: Force lazy allocation off globally
 */
static unsigned int force_lazy_gpumem;
#define FORCE_LAZY_ALLOCATION	1
#define FORCE_UNLAZY_ALLOCATION	2

static int force_lazy_gpumem_set(const char *val, const struct kernel_param *kp)
{
	int ret;
	int old_val = force_lazy_gpumem;

	ret = param_set_uint(val, kp);
	if (ret)
		return ret;

	/*
	 * If lazy allocation is not supported or force_lazy_gpumem is not set
	 * to a valid value as described above, ignore this update.
	 */
	if (!IS_ENABLED(CONFIG_QCOM_KGSL_LAZY_ALLOCATION) ||
			force_lazy_gpumem > FORCE_UNLAZY_ALLOCATION) {
		force_lazy_gpumem = old_val;
		return -EINVAL;
	}

	return 0;
}
module_param_call(force_lazy_gpumem, force_lazy_gpumem_set, param_get_uint,
			&force_lazy_gpumem, 0644);

void kgsl_memdesc_set_lazy_configuration(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, uint64_t *flags, uint32_t *priv)
{
	struct kgsl_mmu *mmu = &device->mmu;
	const unsigned int memtype = MEMFLAGS(*flags, KGSL_MEMTYPE_MASK,
			KGSL_MEMTYPE_SHIFT);

	/*
	 * If we've forced lazy allocation on via the kernel parameter, enable
	 * it globally. If it is not supported for this entry for some reason
	 * it will be masked back off in the other checks here.
	 */
	if (force_lazy_gpumem == FORCE_LAZY_ALLOCATION)
		*priv |= KGSL_MEMDESC_LAZY_ALLOCATION;

	/*
	 * If lazy allocation is enabled for global memory entries then enable
	 * that here so conditions that might later disable this setting are
	 * handled appropriately.
	 */
	if (IS_ENABLED(CONFIG_QCOM_KGSL_LAZY_GLOBALS) &&
			(*priv & KGSL_MEMDESC_GLOBAL))
		*priv |= KGSL_MEMDESC_LAZY_ALLOCATION;

	/*
	 * TODO (T107891327): Some apps are creating large (multi-MiB)
	 * single-use Vulkan command buffers that they free shortly after,
	 * leading to stutter from GPU page fault churn. Disable lazy allocation
	 * for these entries for now until we can resolve this issue.
	 */
	if (memtype == KGSL_MEMTYPE_VK_CMDBUFFER)
		*priv &= ~KGSL_MEMDESC_LAZY_ALLOCATION;

	/*
	 * Enable lazy allocation by default for GPU read-only entries when
	 * the kernel is configured to do so. Pages for these entries are
	 * typically only faulted in via the CPU's VM fault routine, and GPU
	 * read page faults on them are rare.
	 *
	 * NOTE: This explicitly overrides the block on lazy allocation of
	 * command buffers above since that stutter stems from GPU page faults.
	 */
	if (IS_ENABLED(CONFIG_QCOM_KGSL_LAZY_GPUREADONLY) &&
			(*flags & KGSL_MEMFLAGS_GPUREADONLY))
		*priv |= KGSL_MEMDESC_LAZY_ALLOCATION;

	/*
	 * Lazily allocating globals without having split tables enabled is
	 * a nightmare because we then have to go and remap the associated
	 * PTEs in every page table. Too messy to be worth it.
	 * TODO (T106345538): Handle mapping into both global and LPAC PTs.
	 */
	if ((*priv & KGSL_MEMDESC_GLOBAL) != 0 &&
			(!kgsl_iommu_split_tables_enabled(mmu) ||
			 !IS_ERR_OR_NULL(mmu->lpac_pagetable)))
		*priv &= ~KGSL_MEMDESC_LAZY_ALLOCATION;

	/* Fixed memory isn't allocated, lazily or otherwise. */
	if (*priv & KGSL_MEMDESC_FIXED)
		*priv &= ~KGSL_MEMDESC_LAZY_ALLOCATION;

	/*
	 * If lazy allocation for secure entries is disabled globally then
	 * mask it off here.
	 */
	if (!IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION) &&
			(*flags & KGSL_MEMFLAGS_SECURE))
		*priv &= ~KGSL_MEMDESC_LAZY_ALLOCATION;

	/*
	 * If we've forced lazy allocation off via the kernel parameter, disable
	 * it globally.
	 */
	if (force_lazy_gpumem == FORCE_UNLAZY_ALLOCATION)
		*priv &= ~KGSL_MEMDESC_LAZY_ALLOCATION;
}

/* Keep a small pool of pages ready to fulfill lazy allocation requests. */
static struct workqueue_struct *lazymem_workqueue;
static atomic_t lazy_pool_status = ATOMIC_INIT(0);

/* Lazy pool status bits */
#define INITIALIZED BIT(0)
#define AUTO_REFILL BIT(1)

static DEFINE_SPINLOCK(lazy_pool_lock);
static LIST_HEAD(lazy_pool);

#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
static DEFINE_SPINLOCK(lazy_secure_pool_lock);
static LIST_HEAD(lazy_secure_pool);
#endif

struct lazy_pool_refill_ws {
	struct delayed_work dwork;
	struct kgsl_device *device;
	bool secure;
	int pool_size;

	spinlock_t *lock;
	atomic_t waiter_count;
	wait_queue_head_t waitq;
	atomic_t pool_count;
	struct list_head *pool;
};
static struct lazy_pool_refill_ws lazy_pool_refill_work;
static struct lazy_pool_refill_ws lazy_secure_pool_refill_work;

static bool _check_page_available(atomic_t *pool_count)
{
	return (atomic_read(&lazy_pool_status) & INITIALIZED) &&
			atomic_read(pool_count) > 0;
}

static int _wait_for_available_page(bool secure)
{
	struct lazy_pool_refill_ws *refill_work = secure ?
			&lazy_secure_pool_refill_work : &lazy_pool_refill_work;
	int ret;

	atomic_inc(&refill_work->waiter_count);
	ret = wait_event_interruptible_timeout(refill_work->waitq,
			_check_page_available(&refill_work->pool_count),
			msecs_to_jiffies(1));
	atomic_dec(&refill_work->waiter_count);

	return ret;
}

static struct page *_alloc_lazy_page(struct kgsl_device *device, gfp_t gfp_mask,
		bool secure)
{
	struct scatterlist sgl;
	struct page *page;

	/* We always need zeroed pages regardless of the other GFP flags. */
	page = alloc_page(gfp_mask | __GFP_ZERO);
	if (!page)
		return ERR_PTR(-ENOMEM);

	/* Make sure the cache is clean */
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);
	sg_dma_address(&sgl) = page_to_phys(page);

	dma_sync_sg_for_device(device->dev->parent, &sgl, 1, DMA_BIDIRECTIONAL);

#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	/* Lock this page if this is a secure memory entry. */
	if (secure) {
		int lock_status = kgsl_lock_page(page);

		if (lock_status) {
			/*
			 * If we failed to lock, see if the page is wedged in
			 * an indeterminate state. If so we just have to bail
			 * completely (`lock_sgt` will log an error). Otherwise
			 * we at least can free the page before returning.
			 */
			if (lock_status != -EADDRNOTAVAIL)
				__free_page(page);

			return ERR_PTR(lock_status);
		}
	}
#endif

	if (secure)
		KGSL_STATS_ADD(PAGE_SIZE, &kgsl_driver.stats.secure,
				&kgsl_driver.stats.secure_max);
	else
		KGSL_STATS_ADD(PAGE_SIZE, &kgsl_driver.stats.page_alloc,
				&kgsl_driver.stats.page_alloc_max);

	return page;
}

static int _free_lazy_page(struct page *page, const bool secure)
{
	struct lazy_pool_refill_ws *refill_work = secure ?
			&lazy_secure_pool_refill_work : &lazy_pool_refill_work;
	struct scatterlist sgl;
	unsigned long flags;

	if (IS_ERR_OR_NULL(page))
		return 0;

#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	if (secure) {
		int lock_status = kgsl_unlock_page(page);

		if (lock_status) {
			/*
			 * Uh oh. We were unable to unlock the page? Nothing we
			 * can do except print an error. This page will be stuck
			 * in limbo until reboot. :(
			 */
			pr_err("%s:%d secure page unlock failed: %d\n",
					__func__, __LINE__, lock_status);

			return lock_status;
		}
	}
#endif

	/*
	 * For non-secure pages we just have to clear the page and make sure
	 * it's flushed from the cache, then we can return it to the pool. Let's
	 * let it refill up to twice the pool size to take some pressure off the
	 * allocator thread.
	 */
	if ((atomic_read(&lazy_pool_status) & INITIALIZED) && !secure &&
			atomic_read(&refill_work->pool_count) <
			refill_work->pool_size * 2) {
		clear_page(page_address(page));

		/* Make sure the cache is clean */
		sg_init_table(&sgl, 1);
		sg_set_page(&sgl, page, PAGE_SIZE, 0);
		sg_dma_address(&sgl) = page_to_phys(page);

		dma_sync_sg_for_device(refill_work->device->dev->parent, &sgl,
				1, DMA_BIDIRECTIONAL);

		spin_lock_irqsave(refill_work->lock, flags);
		INIT_LIST_HEAD(&page->lru);
		list_add_tail(&page->lru, refill_work->pool);
		atomic_inc(&refill_work->pool_count);
		spin_unlock_irqrestore(refill_work->lock, flags);

		return 0;
	}

	__free_page(page);
	if (secure)
		atomic_long_sub(PAGE_SIZE, &kgsl_driver.stats.secure);
	else
		atomic_long_sub(PAGE_SIZE, &kgsl_driver.stats.page_alloc);

	return 0;
}

void kgsl_free_lazy_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
	struct page *page, *tmp;
	size_t freed_size = 0;
	bool secure = false;

#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	secure = kgsl_memdesc_is_secured(memdesc);
#endif

	list_for_each_entry_safe(page, tmp, page_list, lru) {
		if (_free_lazy_page(page, secure))
			continue;

		freed_size += PAGE_SIZE;
	}

	atomic_long_sub(freed_size, &memdesc->physsize);
}

static void _refill_lazy_pool(struct work_struct *work)
{
	struct lazy_pool_refill_ws *refill_work = container_of(work,
			struct lazy_pool_refill_ws, dwork.work);
	struct page *page = NULL;
	unsigned long flags;
	const gfp_t gfp_mask = GFP_KERNEL | __GFP_HIGH;
	int new_pages, wake_count;

restart_refill:
	/* Track how many pages are allocated in each refill pass. */
	new_pages = 0;
	wake_count = 0;

	while ((atomic_read(&lazy_pool_status) & INITIALIZED) &&
			atomic_read(&refill_work->pool_count)
			< refill_work->pool_size) {
		/*
		 * Allocate a page. If we fail, kick off a job to free any
		 * memory entries that are pending destruction and break here,
		 * since this generally indicates an OoM condition.
		 */
		page = _alloc_lazy_page(refill_work->device, gfp_mask,
				refill_work->secure);
		if (IS_ERR_OR_NULL(page)) {
			queue_work(kgsl_driver.mem_workqueue,
					&kgsl_driver.destroy_work);
			flush_workqueue(kgsl_driver.mem_workqueue);
			break;
		}

		spin_lock_irqsave(refill_work->lock, flags);
		list_add_tail(&page->lru, refill_work->pool);
		atomic_inc(&refill_work->pool_count);
		new_pages++;
		spin_unlock_irqrestore(refill_work->lock, flags);

		/* Wake up at most one thread waiting on fresh pages. */
		if (atomic_read(&refill_work->waiter_count) > 0) {
			wake_up_interruptible(&refill_work->waitq);
			wake_count++;
		}
	}

	if (!(atomic_read(&lazy_pool_status) & INITIALIZED))
		return;

	wake_count += atomic_read(&refill_work->waiter_count);
	if (!IS_ERR_OR_NULL(page) && (new_pages > 0 || wake_count > 0)) {
		/*
		 * Restart refill if we created any new pages this pass or we
		 * had/have active waiters (and no errors were encountered)
		 * since that means that we have something actively consuming
		 * them. Sleep a little while to let a bit of pressure build.
		 */
		usleep_range(100, 200);
		goto restart_refill;
	} else if (atomic_read(&lazy_pool_status) & AUTO_REFILL) {
		/* Requeue refill 10 ms from now to keep the pool ready. */
		queue_delayed_work(lazymem_workqueue, &refill_work->dwork,
				msecs_to_jiffies(10));
	}
}

int kgsl_lazy_page_pool_init(struct kgsl_device *device)
{
	lazymem_workqueue = alloc_workqueue("kgsl-lazymem", WQ_UNBOUND |
			WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (!lazymem_workqueue)
		return -EINVAL;

	INIT_DELAYED_WORK(&lazy_pool_refill_work.dwork, _refill_lazy_pool);
	init_waitqueue_head(&lazy_pool_refill_work.waitq);
	lazy_pool_refill_work.device = device;
	lazy_pool_refill_work.secure = false;
	lazy_pool_refill_work.pool_size = CONFIG_QCOM_KGSL_LAZY_POOL_SIZE;
	lazy_pool_refill_work.lock = &lazy_pool_lock;
	lazy_pool_refill_work.pool = &lazy_pool;
	atomic_set(&lazy_pool_refill_work.waiter_count, 0);
	atomic_set(&lazy_pool_refill_work.pool_count, 0);

#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	INIT_DELAYED_WORK(&lazy_secure_pool_refill_work.dwork,
			_refill_lazy_pool);
	init_waitqueue_head(&lazy_secure_pool_refill_work.waitq);
	lazy_secure_pool_refill_work.device = device;
	lazy_secure_pool_refill_work.secure = true;
	lazy_secure_pool_refill_work.pool_size =
			CONFIG_QCOM_KGSL_SECURE_LAZY_POOL_SIZE;
	lazy_secure_pool_refill_work.lock = &lazy_secure_pool_lock;
	lazy_secure_pool_refill_work.pool = &lazy_secure_pool;
	atomic_set(&lazy_secure_pool_refill_work.waiter_count, 0);
	atomic_set(&lazy_secure_pool_refill_work.pool_count, 0);
#endif

	atomic_set(&lazy_pool_status, INITIALIZED | AUTO_REFILL);

	queue_delayed_work(lazymem_workqueue, &lazy_pool_refill_work.dwork, 0);
#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	queue_delayed_work(lazymem_workqueue,
			&lazy_secure_pool_refill_work.dwork, 0);
#endif
	flush_workqueue(lazymem_workqueue);

	return 0;
}

void kgsl_lazy_page_pool_exit(void)
{
	struct page *page, *tmp;

	/* If the lazy pools never initialized then there's nothing to do. */
	if (!(atomic_xchg(&lazy_pool_status, 0) & INITIALIZED))
		return;

	wake_up_interruptible_all(&lazy_pool_refill_work.waitq);
	cancel_delayed_work(&lazy_pool_refill_work.dwork);
#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	wake_up_interruptible_all(&lazy_secure_pool_refill_work.waitq);
	cancel_delayed_work(&lazy_secure_pool_refill_work.dwork);
#endif
	flush_workqueue(lazymem_workqueue);
	destroy_workqueue(lazymem_workqueue);

	list_for_each_entry_safe(page, tmp, &lazy_pool, lru) {
		list_del(&page->lru);
		_free_lazy_page(page, false);
	}

#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	list_for_each_entry_safe(page, tmp, &lazy_secure_pool, lru) {
		list_del(&page->lru);
		_free_lazy_page(page, true);
	}
#endif
}

void kgsl_lazy_page_pool_resume(void)
{
	if (!(atomic_read(&lazy_pool_status) & INITIALIZED))
		return;

	atomic_or(AUTO_REFILL, &lazy_pool_status);

	queue_delayed_work(lazymem_workqueue, &lazy_pool_refill_work.dwork, 0);
#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	queue_delayed_work(lazymem_workqueue, &lazy_secure_pool_refill_work.dwork, 0);
#endif
	flush_workqueue(lazymem_workqueue);
}

void kgsl_lazy_page_pool_suspend(void)
{
	if (!(atomic_read(&lazy_pool_status) & INITIALIZED))
		return;

	atomic_andnot(AUTO_REFILL, &lazy_pool_status);

	wake_up_interruptible_all(&lazy_pool_refill_work.waitq);
	cancel_delayed_work(&lazy_pool_refill_work.dwork);
#if IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	wake_up_interruptible_all(&lazy_secure_pool_refill_work.waitq);
	cancel_delayed_work(&lazy_secure_pool_refill_work.dwork);
#endif
	flush_workqueue(lazymem_workqueue);
}

static struct page *_get_lazy_pool_page(struct lazy_pool_refill_ws *refill_work)
{
	struct page *page = NULL;
	unsigned long flags;
	int pool_count = 0;

	spin_lock_irqsave(refill_work->lock, flags);
	if (!list_empty(refill_work->pool)) {
		page = list_first_entry(refill_work->pool, struct page, lru);

		list_del_init(&page->lru);
		pool_count = atomic_dec_return(&refill_work->pool_count);
	}
	spin_unlock_irqrestore(refill_work->lock, flags);

	/*
	 * If the pool is at or below 1/8 capacity queue refilling if it's not
	 * refilling already.
	 */
	if (pool_count <= (refill_work->pool_size >> 3)) {
		cancel_delayed_work(&refill_work->dwork);
		queue_delayed_work(lazymem_workqueue, &refill_work->dwork, 0);
	}

	return page;
}

static inline void *_fetch_ptep(struct kgsl_memdesc *memdesc,
		uint64_t offset, void **pptepp)
{
	struct kgsl_iommu_pt *iommu_pt = memdesc->pagetable->priv;
	dma_addr_t iova;
	unsigned long start_time;
	void *ptep;
	const bool trace_enabled = trace_kgsl_iommu_fetch_ptep_enabled();

	if (unlikely(trace_enabled))
		start_time = ktime_get_ns();

	iova = memdesc->gpuaddr + offset;

	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (iova & (1ULL << 48))
		iova |= 0xffff000000000000;

	ptep = iommu_fetch_iova_ptep(iommu_pt->domain, iova, pptepp);

	if (unlikely(trace_enabled)) {
		unsigned long end_time = ktime_get_ns();

		trace_kgsl_iommu_fetch_ptep(memdesc->gpuaddr, offset, ptep,
				*pptepp, (IS_ERR(ptep) ? PTR_ERR(ptep) : 0),
				end_time - start_time);
	}

	return ptep;
}

static inline int _decode_ptep(struct kgsl_memdesc *memdesc, void *ptep,
		struct page **pagep, int *protp)
{
	struct kgsl_iommu_pt *iommu_pt = memdesc->pagetable->priv;
	unsigned long start_time;
	int ret;
	const bool trace_enabled = trace_kgsl_iommu_decode_ptep_enabled();

	if (unlikely(trace_enabled))
		start_time = ktime_get_ns();

	ret = iommu_decode_ptep(iommu_pt->domain, ptep, pagep, protp);

	if (unlikely(trace_enabled)) {
		unsigned long end_time = ktime_get_ns();
		long pfn = 0;

		if (kptr_restrict < 2)
			pfn = IS_ERR(*pagep) ? PTR_ERR(*pagep) :
					page_to_pfn(*pagep);

		trace_kgsl_iommu_decode_ptep(ptep, pfn, (!protp ? 0 : *protp),
				ret, end_time - start_time);
	}

	return ret;
}

static inline int _remap_ptep(struct kgsl_memdesc *memdesc, void *ptep,
		void *pptep, uint64_t offset, struct page *page, int prot)
{
	struct kgsl_iommu_pt *iommu_pt = memdesc->pagetable->priv;
	dma_addr_t iova;
	unsigned long start_time;
	int ret;
	const bool trace_enabled = trace_kgsl_iommu_remap_ptep_enabled();

	if (unlikely(trace_enabled))
		start_time = ktime_get_ns();

	iova = memdesc->gpuaddr + offset;

	/* Sign extend TTBR1 addresses all the way to avoid warning */
	if (iova & (1ULL << 48))
		iova |= 0xffff000000000000;

	ret = iommu_remap_ptep(iommu_pt->domain, ptep, pptep, iova, page, prot);

	if (unlikely(trace_enabled)) {
		unsigned long end_time = ktime_get_ns();
		long pfn = 0;

		if (kptr_restrict < 2)
			pfn = IS_ERR(page) ? PTR_ERR(page) : page_to_pfn(page);

		trace_kgsl_iommu_remap_ptep(pfn, prot, memdesc->gpuaddr, offset,
				ptep, pptep, ret, end_time - start_time);
	}

	return ret;
}

static int _get_lazy_page(struct kgsl_memdesc *memdesc, unsigned long offset,
		unsigned int prot, void *ptep, void *pptep, struct page **pagep,
		const bool lazy_pools_initialized)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(memdesc->pagetable->mmu);
	unsigned long start_time, alloc_time = 0;
	int ret = 0;
	const bool secure = kgsl_memdesc_is_secured(memdesc);
	const bool trace_enabled = trace_kgsl_alloc_lazy_page_enabled();

	/*
	 * We should never hit this since lazy allocation should be masked off
	 * for secure entries in 'set_lazy_configuration' if the kernel
	 * configuration allowing it is missing, but give it a quick check here
	 * anyway.
	 */
	BUG_ON(secure && !IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION));

	if (!pagep)
		return -EINVAL;

	if (offset >= memdesc->size)
		return -ERANGE;

	if (trace_enabled)
		start_time = ktime_get_ns();

	*pagep = !lazy_pools_initialized ? NULL : _get_lazy_pool_page(secure ?
			&lazy_secure_pool_refill_work :	&lazy_pool_refill_work);
	if (IS_ERR_OR_NULL(*pagep)) {
		/*
		 * Locking/unlocking secure pages takes a *long* time, so let
		 * the caller know to unlock the PT map mutex and wait for a
		 * secure page to become available.
		 */
		if (secure && lazy_pools_initialized)
			return -EAGAIN;

		*pagep = _alloc_lazy_page(device, GFP_KERNEL | __GFP_HIGH,
				secure);
	}

	if (IS_ERR_OR_NULL(*pagep)) {
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * If we successfully allocated a secure page above without using the
	 * page pool then we need to jump back to the caller to retry mapping
	 * with the new page. This is to double check that we haven't raced
	 * allocation from another page fault at this address.
	 */
	if (secure && !lazy_pools_initialized)
		return 0;

	if (trace_enabled)
		alloc_time = ktime_get_ns();

	ret = _remap_ptep(memdesc, ptep, pptep, offset, *pagep, prot);
	if (ret) {
		ret = -EINTR;
		goto err;
	}

	atomic_long_add(PAGE_SIZE, &memdesc->physsize);

err:
	if (trace_enabled) {
		unsigned long end_time = ktime_get_ns();
		long pfn = 0;

		if (!alloc_time)
			alloc_time = end_time;
		if (ret)
			pfn = ret;
		else if (kptr_restrict < 2)
			pfn = page_to_pfn(*pagep);

		trace_kgsl_alloc_lazy_page(pfn, prot, memdesc->gpuaddr, offset,
				ptep, pptep, alloc_time - start_time,
				end_time - alloc_time);
	}

	return ret;
}

int kgsl_fetch_lazy_page(struct kgsl_memdesc *memdesc, uint64_t offset,
		struct page **pagep)
{
	void *ptep = NULL, *pptep = NULL;
	unsigned long start_time, search_time = 0;
	unsigned int prot;
	int status = 0;
	const bool lazy_pools_initialized = (atomic_read(&lazy_pool_status) &
			INITIALIZED) != 0;
	const bool trace_enabled = trace_kgsl_lazy_cpu_fault_enabled();

	if (trace_enabled)
		start_time = ktime_get_ns();

	/* It is never valid for the CPU to fault in secure pages. */
	if (kgsl_memdesc_is_secured(memdesc))
		return -EPERM;

	prot = kgsl_iommu_get_protection_flags(memdesc->pagetable, memdesc);

retry:
	rt_mutex_lock(&memdesc->pagetable->map_mutex);

	ptep = _fetch_ptep(memdesc, offset, &pptep);
	if (IS_ERR_OR_NULL(ptep)) {
		status = IS_ERR(ptep) ? PTR_ERR(ptep) : -ENOENT;
		goto err;
	} else if (!pptep) {
		status = -EINVAL;
		goto err;
	}

	if (_decode_ptep(memdesc, ptep, pagep, NULL)) {
		status = -EINVAL;
		goto err;
	}

	if (trace_enabled && !search_time)
		search_time = ktime_get_ns();

	if (IS_ERR_OR_NULL(*pagep))
		status = _get_lazy_page(memdesc, offset, prot, ptep, pptep,
				pagep, lazy_pools_initialized);

err:
	rt_mutex_unlock(&memdesc->pagetable->map_mutex);

	/* If mapping failed free any page we might have grabbed and retry. */
	if (status == -EINTR) {
		if (!IS_ERR_OR_NULL(*pagep)) {
			_free_lazy_page(*pagep, false);
			*pagep = NULL;
		}

		goto retry;
	} else if (status == -EAGAIN) {
		_wait_for_available_page(false);

		goto retry;
	}

	if (trace_enabled) {
		unsigned long end_time = ktime_get_ns();
		long pfn = 0;

		if (!search_time)
			search_time = end_time;
		if (!status && kptr_restrict < 2) {
			status = IS_ERR(*pagep) ? PTR_ERR(*pagep) : 0;
			pfn = IS_ERR_OR_NULL(*pagep) ? 0 : page_to_pfn(*pagep);
		}

		trace_kgsl_lazy_cpu_fault(pfn, prot, memdesc->gpuaddr + offset,
				ptep, pptep, status, search_time - start_time,
				end_time - search_time);
	}

	return status;
}

int kgsl_lazy_vmfault(struct kgsl_memdesc *memdesc, struct vm_area_struct *vma,
		struct vm_fault *vmf)
{
	struct page *page = NULL;
	const uint64_t offset = vmf->address - vma->vm_start;
	int status = 0;

	if (offset >= memdesc->size)
		return VM_FAULT_SIGBUS;

	status = kgsl_fetch_lazy_page(memdesc, offset, &page);
	if (status == -ENOMEM)
		return VM_FAULT_OOM;
	else if (status || IS_ERR_OR_NULL(page))
		return VM_FAULT_SIGBUS;

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

static int _get_memdesc_ref(struct kgsl_memdesc *memdesc)
{
	/*
	 * If this memdesc pointer is invalid, the entry hasn't been mapped yet,
	 * or it doesn't use lazy alloation, then fail here and log the fault.
	 */
	if (IS_ERR_OR_NULL(memdesc) || !(memdesc->priv & KGSL_MEMDESC_MAPPED) ||
			!(memdesc->priv & KGSL_MEMDESC_LAZY_ALLOCATION))
		return -EINVAL;

	/*
	 * If this memdesc is backed by a kgsl_mem_entry then try to grab a ref
	 * to keep it from being freed from under us. If it has been marked for
	 * deletion or we fail to get a reference then return -ENOENT and we'll
	 * fall back on logging the fault.
	 */
	if (!kgsl_memdesc_is_global(memdesc)) {
		struct kgsl_mem_entry *entry = container_of(memdesc,
				struct kgsl_mem_entry, memdesc);

		if (atomic_read(&entry->pending_free) ||
				kgsl_mem_entry_get(entry) == 0)
			return -ENOENT;
	}

	return 0;
}

static void _put_memdesc_ref(struct kgsl_memdesc *memdesc)
{
	/*
	 * If this memdesc is backed by a kgsl_mem_entry then we need to put
	 * the reference to the entry we took with _get_memdesc_ref earlier.
	 * This function is a no-op for global entries which are not refcounted.
	 */
	if (!IS_ERR_OR_NULL(memdesc) && !kgsl_memdesc_is_global(memdesc)) {
		struct kgsl_mem_entry *entry = container_of(
				memdesc, struct kgsl_mem_entry, memdesc);

		kgsl_mem_entry_put_deferred(entry);
	}
}

static struct kgsl_memdesc *_find_lazy_memdesc_at_addr(
		struct kgsl_pagetable *pagetable, uint64_t gpuaddr)
{
	struct kgsl_memdesc *memdesc = ERR_PTR(-ENOENT);
	struct kgsl_iommu_pt *pt;
	struct rb_node *node;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&pagetable->lock, flags);
	pt = pagetable->priv;
	node = pt->rbtree.rb_node;
	while (node != NULL) {
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		if (gpuaddr < entry->memdesc->gpuaddr)
			node = node->rb_left;
		else if (gpuaddr >= entry->memdesc->gpuaddr + entry->footprint)
			node = node->rb_right;
		else {
			memdesc = entry->memdesc;

			/*
			 * Check that the memdesc at this address is a valid
			 * lazily-allocated entry. If it isn't, or if we fail
			 * to grab a reference to it for some reason, bail out
			 * and log the fault.
			 */
			ret = _get_memdesc_ref(memdesc);
			if (ret)
				memdesc = ERR_PTR(ret);

			break;
		}
	}
	spin_unlock_irqrestore(&pagetable->lock, flags);

	return memdesc;
}

int kgsl_lazy_gpu_fault_handler(struct kgsl_iommu_context *ctx,
		struct kgsl_pagetable *fault_pt, unsigned long addr,
		int fault_flags)
{
	struct kgsl_memdesc *memdesc = NULL;
	struct page *page = NULL, *new_secure_page = NULL;
	void *ptep = NULL, *pptep = NULL;
	uint64_t offset;
	unsigned long start_time, search_time = 0;
	unsigned int prot = 0;
	int status = 0;
	bool secure = false, locked = false;
	const bool lazy_pools_initialized = (atomic_read(&lazy_pool_status) &
			INITIALIZED) != 0;
	const bool trace_enabled = trace_kgsl_lazy_gpu_fault_enabled();

	if (IS_ERR_OR_NULL(fault_pt))
		return -EINVAL;

#if !IS_ENABLED(CONFIG_QCOM_KGSL_SECURE_LAZY_ALLOCATION)
	/* If secure lazy allocation is disabled then log any secure PFs. */
	if (fault_pt->name == KGSL_MMU_SECURE_PT ||
			(addr >= KGSL_IOMMU_SECURE_BASE(fault_pt->mmu) &&
			 addr < KGSL_IOMMU_SECURE_END(fault_pt->mmu)))
		return -ENOENT;
#endif

	if (trace_enabled)
		start_time = ktime_get_ns();

	/*
	 * If we didn't find an entry at this address or it doesn't use lazy
	 * allocation then skip straight to logging this fault.
	 */
	memdesc = _find_lazy_memdesc_at_addr(fault_pt, addr);
	if (IS_ERR_OR_NULL(memdesc))
		return -ENOENT;

	/* Get the fault offset relative to the start of the memory entry. */
	offset = addr - memdesc->gpuaddr;
	if (unlikely(offset >= memdesc->size)) {
		status = -EINVAL;
		goto bad_offset;
	}

	/* Secure memory entries sometimes require special treatment. */
	secure = kgsl_memdesc_is_secured(memdesc);

	prot = kgsl_iommu_get_protection_flags(memdesc->pagetable, memdesc);

retry:
	if (!locked) {
		rt_mutex_lock(&fault_pt->map_mutex);
		locked = true;
	}

	ptep = _fetch_ptep(memdesc, offset, &pptep);
	if (IS_ERR_OR_NULL(ptep) || IS_ERR_OR_NULL(pptep)) {
		status = PTR_ERR(ptep);
		goto err;
	}

	status = _decode_ptep(memdesc, ptep, &page, NULL);
	if (unlikely(status))
		goto err;

	if (trace_enabled && !search_time)
		search_time = ktime_get_ns();

	if (likely(!page || IS_ERR(page))) {
		if (unlikely(new_secure_page != NULL)) {
			/*
			 * We can't allocate secure pages in atomic context
			 * because locking/unlocking pages can sleep, so we need
			 * to have a special case for remapping of secure pages
			 * when the secure lazy page pool is not available to
			 * make sure we can stay locked throughout the process.
			 */
			status = _remap_ptep(memdesc, ptep, pptep,
					offset, new_secure_page, prot);
			if (unlikely(status))
				goto err;
			else {
				/*
				 * On successful remap we NULL out the new
				 * secure page pointer so that we don't free it
				 * below.
				 */
				new_secure_page = NULL;

				atomic_long_add(PAGE_SIZE, &memdesc->physsize);
			}
		} else {
			/*
			 * If lazy pools are not being used we need to unlock
			 * to allocate secure pages since locking/unlocking
			 * pages may sleep.
			 */
			if (locked && secure && !lazy_pools_initialized) {
				locked = false;
				rt_mutex_unlock(&fault_pt->map_mutex);
			}

			/* Allocate a new page for this PTE. */
			status = _get_lazy_page(memdesc, offset, prot, ptep,
					pptep, &page, lazy_pools_initialized);
			if (unlikely(status))
				goto err;
			else if (IS_ERR_OR_NULL(page)) {
				status = IS_ERR(page) ? PTR_ERR(page) : -EINVAL;
				goto err;
			} else if (unlikely(secure && !lazy_pools_initialized)) {
				/*
				 * If we had to unlock above to allocate a new
				 * secure page without using the page pools then
				 * we should go around for another pass to make
				 * sure we weren't raced by another page fault.
				 * Secure page allocation is **slow**!
				 */
				new_secure_page = page;
				goto retry;
			}
		}
	} else if (unlikely(fault_flags & IOMMU_FAULT_PERMISSION)) {
		/*
		 * There's a page mapped in at this IOVA but we're hitting a
		 * permission fault? Bail out here and log the fault. This
		 * *should* only happen if the GPU is trying to write to a GPU
		 * read-only entry.
		 */
		status = -EPERM;
		goto err;
	}

	/*
	 * We've successfully handled this request. Clear the fault status
	 * register.
	 */
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_FSR, 0xffffffff);

	/* Make sure the call to clear the FSR posts before resuming. */
	wmb();

	/*
	 * Write 0 to the RESUME register to tell the SMMU to *retry* the
	 * transaction. If we hit a permission fault on the second go-around
	 * we'll catch it above.
	 */
	KGSL_IOMMU_SET_CTX_REG(ctx, KGSL_IOMMU_CTX_RESUME, 0);

	status = 0;

err:
	if (locked) {
		locked = false;
		rt_mutex_unlock(&fault_pt->map_mutex);
	}

	/*
	 * We might have raced on allocation of this page or there was an error
	 * during remap, so we need to clean up after ourselves now that we're
	 * no longer inside the PT spinlock.
	 */
	if (!IS_ERR_OR_NULL(new_secure_page))
		_free_lazy_page(new_secure_page, true);

	/* If mapping failed free any page we might have grabbed and retry. */
	if (status == -EINTR) {
		if (!IS_ERR_OR_NULL(page)) {
			_free_lazy_page(page, secure);
			page = NULL;
		}

		goto retry;
	} else if (status == -EAGAIN) {
		_wait_for_available_page(secure);

		goto retry;
	}

bad_offset:
	/* Put the reference to the entry we took earlier (if applicable) */
	_put_memdesc_ref(memdesc);

	if (trace_enabled) {
		unsigned long end_time = ktime_get_ns();
		long pfn = 0;

		if (!search_time)
			search_time = end_time;
		if (kptr_restrict < 2)
			pfn = IS_ERR_OR_NULL(page) ? PTR_ERR(page) :
					page_to_pfn(page);

		trace_kgsl_lazy_gpu_fault(pfn, prot, addr, ptep, pptep, status,
				search_time - start_time,
				end_time - search_time);
	}

	return status;
}
