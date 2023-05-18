// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016, 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/msm_ipa.h>
#include <linux/mutex.h>
#include <linux/ipa.h>
#include "ipa_i.h"

#define IPA_DMA_POLLING_MIN_SLEEP_RX 1010
#define IPA_DMA_POLLING_MAX_SLEEP_RX 1050
#define IPA_DMA_SYS_DESC_MAX_FIFO_SZ 0x7FF8
#define IPA_DMA_MAX_PKT_SZ 0xFFFF
#define IPA_DMA_MAX_PENDING_SYNC (IPA_SYS_DESC_FIFO_SZ / \
	sizeof(struct sps_iovec) - 1)
#define IPA_DMA_MAX_PENDING_ASYNC (IPA_DMA_SYS_DESC_MAX_FIFO_SZ / \
	sizeof(struct sps_iovec) - 1)

#define IPADMA_DRV_NAME "ipa_dma"

#define IPADMA_DBG(fmt, args...) \
	do { \
		pr_debug(IPADMA_DRV_NAME " %s:%d " fmt, \
		__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPADMA_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPADMA_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPADMA_ERR(fmt, args...) \
	do { \
		pr_err(IPADMA_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPADMA_FUNC_ENTRY() \
	IPADMA_DBG_LOW("ENTRY\n")

#define IPADMA_FUNC_EXIT() \
	IPADMA_DBG_LOW("EXIT\n")


#ifdef CONFIG_DEBUG_FS
#define IPADMA_MAX_MSG_LEN 1024
static char dbg_buff[IPADMA_MAX_MSG_LEN];
static void ipa_dma_debugfs_init(void);
static void ipa_dma_debugfs_destroy(void);
#else
static void ipa_dma_debugfs_init(void) {}
static void ipa_dma_debugfs_destroy(void) {}
#endif

/**
 * struct ipa_dma_xfer_wrapper - IPADMA transfer descr wrapper
 * @phys_addr_src: physical address of the source data to copy
 * @phys_addr_dest: physical address to store the copied data
 * @len: len in bytes to copy
 * @link: linked to the wrappers list on the proper(sync/async) cons pipe
 * @xfer_done: completion object for sync_memcpy completion
 * @callback: IPADMA client provided completion callback
 * @user1: cookie1 for above callback
 *
 * This struct can wrap both sync and async memcpy transfers descriptors.
 */
struct ipa_dma_xfer_wrapper {
	u64 phys_addr_src;
	u64 phys_addr_dest;
	u16 len;
	struct list_head link;
	struct completion xfer_done;
	void (*callback)(void *user1);
	void *user1;
};

/**
 * struct ipa_dma_ctx -IPADMA driver context information
 * @is_enabled:is ipa_dma enabled?
 * @destroy_pending: destroy ipa_dma after handling all pending memcpy
 * @ipa_dma_xfer_wrapper_cache: cache of ipa_dma_xfer_wrapper structs
 * @sync_lock: lock for synchronisation in sync_memcpy
 * @async_lock: lock for synchronisation in async_memcpy
 * @enable_lock: lock for is_enabled
 * @pending_lock: lock for synchronize is_enable and pending_cnt
 * @done: no pending works-ipadma can be destroyed
 * @ipa_dma_sync_prod_hdl: handle of sync memcpy producer
 * @ipa_dma_async_prod_hdl:handle of async memcpy producer
 * @ipa_dma_sync_cons_hdl: handle of sync memcpy consumer
 * @sync_memcpy_pending_cnt: number of pending sync memcopy operations
 * @async_memcpy_pending_cnt: number of pending async memcopy operations
 * @uc_memcpy_pending_cnt: number of pending uc memcopy operations
 * @total_sync_memcpy: total number of sync memcpy (statistics)
 * @total_async_memcpy: total number of async memcpy (statistics)
 * @total_uc_memcpy: total number of uc memcpy (statistics)
 */
struct ipa_dma_ctx {
	bool is_enabled;
	bool destroy_pending;
	struct kmem_cache *ipa_dma_xfer_wrapper_cache;
	struct mutex sync_lock;
	spinlock_t async_lock;
	struct mutex enable_lock;
	spinlock_t pending_lock;
	struct completion done;
	u32 ipa_dma_sync_prod_hdl;
	u32 ipa_dma_async_prod_hdl;
	u32 ipa_dma_sync_cons_hdl;
	u32 ipa_dma_async_cons_hdl;
	atomic_t sync_memcpy_pending_cnt;
	atomic_t async_memcpy_pending_cnt;
	atomic_t uc_memcpy_pending_cnt;
	atomic_t total_sync_memcpy;
	atomic_t total_async_memcpy;
	atomic_t total_uc_memcpy;
};
static struct ipa_dma_ctx *ipa_dma_ctx;

/**
 * ipa2_dma_init() -Initialize IPADMA.
 *
 * This function initialize all IPADMA internal data and connect in dma:
 *	MEMCPY_DMA_SYNC_PROD ->MEMCPY_DMA_SYNC_CONS
 *	MEMCPY_DMA_ASYNC_PROD->MEMCPY_DMA_SYNC_CONS
 *
 * Return codes: 0: success
 *		-EFAULT: IPADMA is already initialized
 *		-ENOMEM: allocating memory error
 *		-EPERM: pipe connection failed
 */
int ipa2_dma_init(void)
{
	struct ipa_dma_ctx *ipa_dma_ctx_t;
	struct ipa_sys_connect_params sys_in;
	int res = 0;

	IPADMA_FUNC_ENTRY();

	if (ipa_dma_ctx) {
		IPADMA_ERR("Already initialized.\n");
		return -EFAULT;
	}
	ipa_dma_ctx_t = kzalloc(sizeof(*(ipa_dma_ctx)), GFP_KERNEL);

	if (!ipa_dma_ctx_t) {
		IPADMA_ERR("kzalloc error.\n");
		return -ENOMEM;
	}

	ipa_dma_ctx_t->ipa_dma_xfer_wrapper_cache =
		kmem_cache_create("IPA_DMA_XFER_WRAPPER",
			sizeof(struct ipa_dma_xfer_wrapper), 0, 0, NULL);
	if (!ipa_dma_ctx_t->ipa_dma_xfer_wrapper_cache) {
		IPAERR(":failed to create ipa dma xfer wrapper cache.\n");
		res = -ENOMEM;
		goto fail_mem_ctrl;
	}

	mutex_init(&ipa_dma_ctx_t->enable_lock);
	spin_lock_init(&ipa_dma_ctx_t->async_lock);
	mutex_init(&ipa_dma_ctx_t->sync_lock);
	spin_lock_init(&ipa_dma_ctx_t->pending_lock);
	init_completion(&ipa_dma_ctx_t->done);
	ipa_dma_ctx_t->is_enabled = false;
	ipa_dma_ctx_t->destroy_pending = false;
	atomic_set(&ipa_dma_ctx_t->async_memcpy_pending_cnt, 0);
	atomic_set(&ipa_dma_ctx_t->sync_memcpy_pending_cnt, 0);
	atomic_set(&ipa_dma_ctx_t->uc_memcpy_pending_cnt, 0);
	atomic_set(&ipa_dma_ctx_t->total_async_memcpy, 0);
	atomic_set(&ipa_dma_ctx_t->total_sync_memcpy, 0);
	atomic_set(&ipa_dma_ctx_t->total_uc_memcpy, 0);

	/* IPADMA SYNC PROD-source for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_SYNC_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_MEMCPY_DMA_SYNC_CONS;
	sys_in.skip_ep_cfg = false;
	if (ipa2_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_sync_prod_hdl)) {
		IPADMA_ERR(":setup sync prod pipe failed\n");
		res = -EPERM;
		goto fail_sync_prod;
	}

	/* IPADMA SYNC CONS-destination for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_SYNC_CONS;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.skip_ep_cfg = false;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	sys_in.notify = NULL;
	sys_in.priv = NULL;
	if (ipa2_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_sync_cons_hdl)) {
		IPADMA_ERR(":setup sync cons pipe failed.\n");
		res = -EPERM;
		goto fail_sync_cons;
	}

	IPADMA_DBG("SYNC MEMCPY pipes are connected\n");

	/* IPADMA ASYNC PROD-source for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD;
	sys_in.desc_fifo_sz = IPA_DMA_SYS_DESC_MAX_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS;
	sys_in.skip_ep_cfg = false;
	sys_in.notify = NULL;
	if (ipa2_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_async_prod_hdl)) {
		IPADMA_ERR(":setup async prod pipe failed.\n");
		res = -EPERM;
		goto fail_async_prod;
	}

	/* IPADMA ASYNC CONS-destination for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS;
	sys_in.desc_fifo_sz = IPA_DMA_SYS_DESC_MAX_FIFO_SZ;
	sys_in.skip_ep_cfg = false;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	sys_in.notify = ipa_dma_async_memcpy_notify_cb;
	sys_in.priv = NULL;
	if (ipa2_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_async_cons_hdl)) {
		IPADMA_ERR(":setup async cons pipe failed.\n");
		res = -EPERM;
		goto fail_async_cons;
	}
	ipa_dma_debugfs_init();
	ipa_dma_ctx = ipa_dma_ctx_t;
	IPADMA_DBG("ASYNC MEMCPY pipes are connected\n");

	IPADMA_FUNC_EXIT();
	return res;
fail_async_cons:
	ipa2_teardown_sys_pipe(ipa_dma_ctx_t->ipa_dma_async_prod_hdl);
fail_async_prod:
	ipa2_teardown_sys_pipe(ipa_dma_ctx_t->ipa_dma_sync_cons_hdl);
fail_sync_cons:
	ipa2_teardown_sys_pipe(ipa_dma_ctx_t->ipa_dma_sync_prod_hdl);
fail_sync_prod:
	kmem_cache_destroy(ipa_dma_ctx_t->ipa_dma_xfer_wrapper_cache);
fail_mem_ctrl:
	kfree(ipa_dma_ctx_t);
	ipa_dma_ctx = NULL;
	return res;

}


/**
 * ipa2_dma_enable() -Vote for IPA clocks.
 *
 *Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 *		-EPERM: Operation not permitted as ipa_dma is already
 *		 enabled
 */
int ipa2_dma_enable(void)
{
	IPADMA_FUNC_ENTRY();
	if (ipa_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't enable\n");
		return -EPERM;
	}
	mutex_lock(&ipa_dma_ctx->enable_lock);
	if (ipa_dma_ctx->is_enabled) {
		IPADMA_ERR("Already enabled.\n");
		mutex_unlock(&ipa_dma_ctx->enable_lock);
		return -EPERM;
	}
	IPA_ACTIVE_CLIENTS_INC_SPECIAL("DMA");
	ipa_dma_ctx->is_enabled = true;
	mutex_unlock(&ipa_dma_ctx->enable_lock);

	IPADMA_FUNC_EXIT();
	return 0;
}

static bool ipa_dma_work_pending(void)
{
	if (atomic_read(&ipa_dma_ctx->sync_memcpy_pending_cnt)) {
		IPADMA_DBG("pending sync\n");
		return true;
	}
	if (atomic_read(&ipa_dma_ctx->async_memcpy_pending_cnt)) {
		IPADMA_DBG("pending async\n");
		return true;
	}
	if (atomic_read(&ipa_dma_ctx->uc_memcpy_pending_cnt)) {
		IPADMA_DBG("pending uc\n");
		return true;
	}
	IPADMA_DBG_LOW("no pending work\n");
	return false;
}

/**
 * ipa2_dma_disable()- Unvote for IPA clocks.
 *
 * enter to power save mode.
 *
 * Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 *		-EPERM: Operation not permitted as ipa_dma is already
 *			diabled
 *		-EFAULT: can not disable ipa_dma as there are pending
 *			memcopy works
 */
int ipa2_dma_disable(void)
{
	unsigned long flags;

	IPADMA_FUNC_ENTRY();
	if (ipa_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't disable\n");
		return -EPERM;
	}
	mutex_lock(&ipa_dma_ctx->enable_lock);
	spin_lock_irqsave(&ipa_dma_ctx->pending_lock, flags);
	if (!ipa_dma_ctx->is_enabled) {
		IPADMA_ERR("Already disabled.\n");
		spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
		mutex_unlock(&ipa_dma_ctx->enable_lock);
		return -EPERM;
	}
	if (ipa_dma_work_pending()) {
		IPADMA_ERR("There is pending work, can't disable.\n");
		spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
		mutex_unlock(&ipa_dma_ctx->enable_lock);
		return -EFAULT;
	}
	ipa_dma_ctx->is_enabled = false;
	spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
	IPA_ACTIVE_CLIENTS_DEC_SPECIAL("DMA");
	mutex_unlock(&ipa_dma_ctx->enable_lock);
	IPADMA_FUNC_EXIT();
	return 0;
}

/**
 * ipa2_dma_sync_memcpy()- Perform synchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-SPS_ERROR: on sps faliures
 *		-EFAULT: other
 */
int ipa2_dma_sync_memcpy(u64 dest, u64 src, int len)
{
	int ep_idx;
	int res;
	int i = 0;
	struct ipa_sys_context *cons_sys;
	struct ipa_sys_context *prod_sys;
	struct sps_iovec iov;
	struct ipa_dma_xfer_wrapper *xfer_descr = NULL;
	struct ipa_dma_xfer_wrapper *head_descr = NULL;
	unsigned long flags;

	IPADMA_FUNC_ENTRY();

	IPADMA_DBG_LOW("dest =  0x%llx, src = 0x%llx, len = %d\n",
		dest, src, len);
	if (ipa_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't memcpy\n");
		return -EPERM;
	}
	if ((max(src, dest) - min(src, dest)) < len) {
		IPADMA_ERR("invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}
	if (len > IPA_DMA_MAX_PKT_SZ || len <= 0) {
		IPADMA_ERR("invalid len, %d\n", len);
		return	-EINVAL;
	}
	if (((u32)src != src) || ((u32)dest != dest)) {
		IPADMA_ERR("Bad addr - only 32b addr supported for BAM");
		return -EINVAL;
	}
	spin_lock_irqsave(&ipa_dma_ctx->pending_lock, flags);
	if (!ipa_dma_ctx->is_enabled) {
		IPADMA_ERR("can't memcpy, IPADMA isn't enabled\n");
		spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
		return -EPERM;
	}
	atomic_inc(&ipa_dma_ctx->sync_memcpy_pending_cnt);
	spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
	if (atomic_read(&ipa_dma_ctx->sync_memcpy_pending_cnt) >=
		IPA_DMA_MAX_PENDING_SYNC) {
		atomic_dec(&ipa_dma_ctx->sync_memcpy_pending_cnt);
		IPADMA_ERR("Reached pending requests limit\n");
		return -EFAULT;
	}

	ep_idx = ipa2_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_SYNC_CONS);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_SYNC_CONS);
		return -EFAULT;
	}
	cons_sys = ipa_ctx->ep[ep_idx].sys;

	ep_idx = ipa2_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_SYNC_PROD);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_SYNC_PROD);
		return -EFAULT;
	}
	prod_sys = ipa_ctx->ep[ep_idx].sys;

	xfer_descr = kmem_cache_zalloc(ipa_dma_ctx->ipa_dma_xfer_wrapper_cache,
					GFP_KERNEL);
	if (!xfer_descr) {
		IPADMA_ERR("failed to alloc xfer descr wrapper\n");
		res = -ENOMEM;
		goto fail_mem_alloc;
	}
	xfer_descr->phys_addr_dest = dest;
	xfer_descr->phys_addr_src = src;
	xfer_descr->len = len;
	init_completion(&xfer_descr->xfer_done);

	mutex_lock(&ipa_dma_ctx->sync_lock);
	list_add_tail(&xfer_descr->link, &cons_sys->head_desc_list);
	cons_sys->len++;
	res = sps_transfer_one(cons_sys->ep->ep_hdl, dest, len, NULL, 0);
	if (res) {
		IPADMA_ERR("Failed: sps_transfer_one on dest descr\n");
		goto fail_sps_send;
	}
	res = sps_transfer_one(prod_sys->ep->ep_hdl, src, len,
		NULL, SPS_IOVEC_FLAG_EOT);
	if (res) {
		IPADMA_ERR("Failed: sps_transfer_one on src descr\n");
		ipa_assert();
	}
	head_descr = list_first_entry(&cons_sys->head_desc_list,
				struct ipa_dma_xfer_wrapper, link);

	/* in case we are not the head of the list, wait for head to wake us */
	if (xfer_descr != head_descr) {
		mutex_unlock(&ipa_dma_ctx->sync_lock);
		wait_for_completion(&xfer_descr->xfer_done);
		mutex_lock(&ipa_dma_ctx->sync_lock);
		head_descr = list_first_entry(&cons_sys->head_desc_list,
					struct ipa_dma_xfer_wrapper, link);
		ipa_assert_on(xfer_descr != head_descr);
	}
	mutex_unlock(&ipa_dma_ctx->sync_lock);

	do {
		/* wait for transfer to complete */
		res = sps_get_iovec(cons_sys->ep->ep_hdl, &iov);
		if (res)
			IPADMA_ERR("Failed: get_iovec, returned %d loop#:%d\n"
			, res, i);

		usleep_range(IPA_DMA_POLLING_MIN_SLEEP_RX,
			IPA_DMA_POLLING_MAX_SLEEP_RX);
		i++;
	} while (iov.addr == 0);

	mutex_lock(&ipa_dma_ctx->sync_lock);
	list_del(&head_descr->link);
	cons_sys->len--;
	kmem_cache_free(ipa_dma_ctx->ipa_dma_xfer_wrapper_cache, xfer_descr);
	/* wake the head of the list */
	if (!list_empty(&cons_sys->head_desc_list)) {
		head_descr = list_first_entry(&cons_sys->head_desc_list,
				struct ipa_dma_xfer_wrapper, link);
		complete(&head_descr->xfer_done);
	}
	mutex_unlock(&ipa_dma_ctx->sync_lock);

	ipa_assert_on(dest != iov.addr);
	ipa_assert_on(len != iov.size);
	atomic_inc(&ipa_dma_ctx->total_sync_memcpy);
	atomic_dec(&ipa_dma_ctx->sync_memcpy_pending_cnt);
	if (ipa_dma_ctx->destroy_pending && !ipa_dma_work_pending())
		complete(&ipa_dma_ctx->done);

	IPADMA_FUNC_EXIT();
	return res;

fail_sps_send:
	list_del(&xfer_descr->link);
	cons_sys->len--;
	mutex_unlock(&ipa_dma_ctx->sync_lock);
	kmem_cache_free(ipa_dma_ctx->ipa_dma_xfer_wrapper_cache, xfer_descr);
fail_mem_alloc:
	atomic_dec(&ipa_dma_ctx->sync_memcpy_pending_cnt);
	if (ipa_dma_ctx->destroy_pending && !ipa_dma_work_pending())
		complete(&ipa_dma_ctx->done);
	return res;
}

/**
 * ipa2_dma_async_memcpy()- Perform asynchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 * @user_cb: callback function to notify the client when the copy was done.
 * @user_param: cookie for user_cb.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-SPS_ERROR: on sps faliures
 *		-EFAULT: descr fifo is full.
 */
int ipa2_dma_async_memcpy(u64 dest, u64 src, int len,
		void (*user_cb)(void *user1), void *user_param)
{
	int ep_idx;
	int res = 0;
	struct ipa_dma_xfer_wrapper *xfer_descr = NULL;
	struct ipa_sys_context *prod_sys;
	struct ipa_sys_context *cons_sys;
	unsigned long flags;

	IPADMA_FUNC_ENTRY();
	IPADMA_DBG_LOW("dest =  0x%llx, src = 0x%llx, len = %d\n",
		dest, src, len);
	if (ipa_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't memcpy\n");
		return -EPERM;
	}
	if ((max(src, dest) - min(src, dest)) < len) {
		IPADMA_ERR("invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}
	if (len > IPA_DMA_MAX_PKT_SZ || len <= 0) {
		IPADMA_ERR("invalid len, %d\n", len);
		return	-EINVAL;
	}
	if (((u32)src != src) || ((u32)dest != dest)) {
		IPADMA_ERR("Bad addr - only 32b addr supported for BAM");
		return -EINVAL;
	}
	if (!user_cb) {
		IPADMA_ERR("null pointer: user_cb\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&ipa_dma_ctx->pending_lock, flags);
	if (!ipa_dma_ctx->is_enabled) {
		IPADMA_ERR("can't memcpy, IPA_DMA isn't enabled\n");
		spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
		return -EPERM;
	}
	atomic_inc(&ipa_dma_ctx->async_memcpy_pending_cnt);
	spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
	if (atomic_read(&ipa_dma_ctx->async_memcpy_pending_cnt) >=
		IPA_DMA_MAX_PENDING_ASYNC) {
		atomic_dec(&ipa_dma_ctx->async_memcpy_pending_cnt);
		IPADMA_ERR("Reached pending requests limit\n");
		return -EFAULT;
	}

	ep_idx = ipa2_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS);
		return -EFAULT;
	}
	cons_sys = ipa_ctx->ep[ep_idx].sys;

	ep_idx = ipa2_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_SYNC_PROD);
		return -EFAULT;
	}
	prod_sys = ipa_ctx->ep[ep_idx].sys;

	xfer_descr = kmem_cache_zalloc(ipa_dma_ctx->ipa_dma_xfer_wrapper_cache,
					GFP_KERNEL);
	if (!xfer_descr) {
		IPADMA_ERR("failed to alloc xfrer descr wrapper\n");
		res = -ENOMEM;
		goto fail_mem_alloc;
	}
	xfer_descr->phys_addr_dest = dest;
	xfer_descr->phys_addr_src = src;
	xfer_descr->len = len;
	xfer_descr->callback = user_cb;
	xfer_descr->user1 = user_param;

	spin_lock_irqsave(&ipa_dma_ctx->async_lock, flags);
	list_add_tail(&xfer_descr->link, &cons_sys->head_desc_list);
	cons_sys->len++;
	res = sps_transfer_one(cons_sys->ep->ep_hdl, dest, len, xfer_descr, 0);
	if (res) {
		IPADMA_ERR("Failed: sps_transfer_one on dest descr\n");
		goto fail_sps_send;
	}
	res = sps_transfer_one(prod_sys->ep->ep_hdl, src, len,
		NULL, SPS_IOVEC_FLAG_EOT);
	if (res) {
		IPADMA_ERR("Failed: sps_transfer_one on src descr\n");
		ipa_assert();
		goto fail_sps_send;
	}
	spin_unlock_irqrestore(&ipa_dma_ctx->async_lock, flags);
	IPADMA_FUNC_EXIT();
	return res;

fail_sps_send:
	list_del(&xfer_descr->link);
	spin_unlock_irqrestore(&ipa_dma_ctx->async_lock, flags);
	kmem_cache_free(ipa_dma_ctx->ipa_dma_xfer_wrapper_cache, xfer_descr);
fail_mem_alloc:
	atomic_dec(&ipa_dma_ctx->async_memcpy_pending_cnt);
	if (ipa_dma_ctx->destroy_pending && !ipa_dma_work_pending())
		complete(&ipa_dma_ctx->done);
	return res;
}

/**
 * ipa2_dma_uc_memcpy() - Perform a memcpy action using IPA uC
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-EBADF: IPA uC is not loaded
 */
int ipa2_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len)
{
	int res;
	unsigned long flags;

	IPADMA_FUNC_ENTRY();
	if (ipa_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't memcpy\n");
		return -EPERM;
	}
	if ((max(src, dest) - min(src, dest)) < len) {
		IPADMA_ERR("invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}
	if (len > IPA_DMA_MAX_PKT_SZ || len <= 0) {
		IPADMA_ERR("invalid len, %d\n", len);
		return	-EINVAL;
	}

	spin_lock_irqsave(&ipa_dma_ctx->pending_lock, flags);
	if (!ipa_dma_ctx->is_enabled) {
		IPADMA_ERR("can't memcpy, IPADMA isn't enabled\n");
		spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);
		return -EPERM;
	}
	atomic_inc(&ipa_dma_ctx->uc_memcpy_pending_cnt);
	spin_unlock_irqrestore(&ipa_dma_ctx->pending_lock, flags);

	res = ipa_uc_memcpy(dest, src, len);
	if (res) {
		IPADMA_ERR("ipa_uc_memcpy failed %d\n", res);
		goto dec_and_exit;
	}

	atomic_inc(&ipa_dma_ctx->total_uc_memcpy);
	res = 0;
dec_and_exit:
	atomic_dec(&ipa_dma_ctx->uc_memcpy_pending_cnt);
	if (ipa_dma_ctx->destroy_pending && !ipa_dma_work_pending())
		complete(&ipa_dma_ctx->done);
	IPADMA_FUNC_EXIT();
	return res;
}

/**
 * ipa2_dma_destroy() -teardown IPADMA pipes and release ipadma.
 *
 * this is a blocking function, returns just after destroying IPADMA.
 */
void ipa2_dma_destroy(void)
{
	int res = 0;

	IPADMA_FUNC_ENTRY();
	if (!ipa_dma_ctx) {
		IPADMA_ERR("IPADMA isn't initialized\n");
		return;
	}

	if (ipa_dma_work_pending()) {
		ipa_dma_ctx->destroy_pending = true;
		IPADMA_DBG("There are pending memcpy, wait for completion\n");
		wait_for_completion(&ipa_dma_ctx->done);
	}

	res = ipa2_teardown_sys_pipe(ipa_dma_ctx->ipa_dma_async_cons_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA ASYNC CONS failed\n");
	ipa_dma_ctx->ipa_dma_async_cons_hdl = 0;
	res = ipa2_teardown_sys_pipe(ipa_dma_ctx->ipa_dma_sync_cons_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA SYNC CONS failed\n");
	ipa_dma_ctx->ipa_dma_sync_cons_hdl = 0;
	res = ipa2_teardown_sys_pipe(ipa_dma_ctx->ipa_dma_async_prod_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA ASYNC PROD failed\n");
	ipa_dma_ctx->ipa_dma_async_prod_hdl = 0;
	res = ipa2_teardown_sys_pipe(ipa_dma_ctx->ipa_dma_sync_prod_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA SYNC PROD failed\n");
	ipa_dma_ctx->ipa_dma_sync_prod_hdl = 0;

	ipa_dma_debugfs_destroy();
	kmem_cache_destroy(ipa_dma_ctx->ipa_dma_xfer_wrapper_cache);
	kfree(ipa_dma_ctx);
	ipa_dma_ctx = NULL;

	IPADMA_FUNC_EXIT();
}

/**
 * ipa_dma_async_memcpy_notify_cb() -Callback function which will be called by
 * IPA driver after getting notify from SPS driver or poll mode on Rx operation
 * is completed (data was written to dest descriptor on async_cons ep).
 *
 * @priv -not in use.
 * @evt - event name - IPA_RECIVE.
 * @data -the iovec.
 */
void ipa_dma_async_memcpy_notify_cb(void *priv
			, enum ipa_dp_evt_type evt, unsigned long data)
{
	int ep_idx = 0;
	struct sps_iovec *iov = (struct sps_iovec *) data;
	struct ipa_dma_xfer_wrapper *xfer_descr_expected;
	struct ipa_sys_context *sys;
	unsigned long flags;

	IPADMA_FUNC_ENTRY();

	ep_idx = ipa2_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS);
	sys = ipa_ctx->ep[ep_idx].sys;

	spin_lock_irqsave(&ipa_dma_ctx->async_lock, flags);
	xfer_descr_expected = list_first_entry(&sys->head_desc_list,
				 struct ipa_dma_xfer_wrapper, link);
	list_del(&xfer_descr_expected->link);
	sys->len--;
	spin_unlock_irqrestore(&ipa_dma_ctx->async_lock, flags);

	ipa_assert_on(xfer_descr_expected->phys_addr_dest != iov->addr);
	ipa_assert_on(xfer_descr_expected->len != iov->size);

	atomic_inc(&ipa_dma_ctx->total_async_memcpy);
	atomic_dec(&ipa_dma_ctx->async_memcpy_pending_cnt);
	xfer_descr_expected->callback(xfer_descr_expected->user1);

	kmem_cache_free(ipa_dma_ctx->ipa_dma_xfer_wrapper_cache,
		xfer_descr_expected);

	if (ipa_dma_ctx->destroy_pending && !ipa_dma_work_pending())
		complete(&ipa_dma_ctx->done);

	IPADMA_FUNC_EXIT();
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static struct dentry *dfile_info;

static ssize_t ipa_dma_debugfs_read(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	int nbytes = 0;

	if (!ipa_dma_ctx) {
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"Not initialized\n");
	} else {
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"Status:\n	IPADMA is %s\n",
			(ipa_dma_ctx->is_enabled) ? "Enabled" : "Disabled");
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"Statistics:\n	total sync memcpy: %d\n	",
			atomic_read(&ipa_dma_ctx->total_sync_memcpy));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"total async memcpy: %d\n	",
			atomic_read(&ipa_dma_ctx->total_async_memcpy));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"pending sync memcpy jobs: %d\n	",
			atomic_read(&ipa_dma_ctx->sync_memcpy_pending_cnt));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"pending async memcpy jobs: %d\n",
			atomic_read(&ipa_dma_ctx->async_memcpy_pending_cnt));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"pending uc memcpy jobs: %d\n",
			atomic_read(&ipa_dma_ctx->uc_memcpy_pending_cnt));
	}
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_dma_debugfs_reset_statistics(struct file *file,
					const char __user *ubuf,
					size_t count,
					loff_t *ppos)
{
	unsigned long missing;
	s8 in_num = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &in_num))
		return -EFAULT;
	switch (in_num) {
	case 0:
		if (ipa_dma_work_pending())
			IPADMA_ERR("Note, there are pending memcpy\n");

		atomic_set(&ipa_dma_ctx->total_async_memcpy, 0);
		atomic_set(&ipa_dma_ctx->total_sync_memcpy, 0);
		break;
	default:
		IPADMA_ERR("invalid argument: To reset statistics echo 0\n");
		break;
	}
	return count;
}

static const struct file_operations ipadma_stats_ops = {
	.read = ipa_dma_debugfs_read,
	.write = ipa_dma_debugfs_reset_statistics,
};

static void ipa_dma_debugfs_init(void)
{
	const mode_t read_write_mode = 0666;

	dent = debugfs_create_dir("ipa_dma", NULL);
	if (IS_ERR(dent)) {
		IPADMA_ERR("fail to create folder ipa_dma\n");
		return;
	}

	dfile_info =
		debugfs_create_file("info", read_write_mode, dent,
				 NULL, &ipadma_stats_ops);
	if (!dfile_info || IS_ERR(dfile_info)) {
		IPADMA_ERR("fail to create file stats\n");
		goto fail;
	}
	return;
fail:
	debugfs_remove_recursive(dent);
}

static void ipa_dma_debugfs_destroy(void)
{
	debugfs_remove_recursive(dent);
}

#endif /* !CONFIG_DEBUG_FS */
