/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021, 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wbuff.c
 * wbuff buffer management APIs
 */

#include <wbuff.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <qdf_debugfs.h>
#include "i_wbuff.h"

/*
 * Allocation holder array for all wbuff registered modules
 */
struct wbuff_holder wbuff;

/**
 * wbuff_get_pool_slot_from_len() - get pool_id from length
 * @mod: wbuff module reference
 * @len: length of the buffer
 *
 * Return: pool_id
 */
static uint8_t
wbuff_get_pool_slot_from_len(struct wbuff_module *mod, uint16_t len)
{
	struct wbuff_pool *pool;
	uint16_t prev_buf_size = 0;
	int i;

	for (i = 0; i < WBUFF_MAX_POOLS; i++) {
		pool = &mod->wbuff_pool[i];

		if (!pool->initialized)
			continue;

		if ((len > prev_buf_size) && (len <= pool->buffer_size))
			break;

		prev_buf_size = mod->wbuff_pool[i].buffer_size;
	}

	return i;
}

/**
 * wbuff_is_valid_alloc_req() - validate alloc  request
 * @req: allocation request from registered module
 * @num: number of pools required
 *
 * Return: true if valid wbuff_alloc_request
 *         false if invalid wbuff_alloc_request
 */
static bool
wbuff_is_valid_alloc_req(struct wbuff_alloc_request *req, uint8_t num)
{
	int i;

	for (i = 0; i < num; i++) {
		if (req[i].pool_id >= WBUFF_MAX_POOLS)
			return false;
	}

	return true;
}

/**
 * wbuff_prepare_nbuf() - allocate nbuf
 * @module_id: module ID
 * @pool_id: pool ID
 * @len: length of the buffer
 * @reserve: nbuf headroom to start with
 * @align: alignment for the nbuf
 *
 * Return: nbuf if success
 *         NULL if failure
 */
static qdf_nbuf_t wbuff_prepare_nbuf(uint8_t module_id, uint8_t pool_id,
				     uint32_t len, int reserve, int align)
{
	qdf_nbuf_t buf;
	unsigned long dev_scratch = 0;
	struct wbuff_module *mod = &wbuff.mod[module_id];
	struct wbuff_pool *wbuff_pool = &mod->wbuff_pool[pool_id];

	buf = qdf_nbuf_page_frag_alloc(NULL, len, reserve, align,
				       &wbuff.pf_cache);
	if (!buf)
		return NULL;
	dev_scratch = module_id;
	dev_scratch <<= WBUFF_MODULE_ID_SHIFT;
	dev_scratch |= ((pool_id << WBUFF_POOL_ID_SHIFT) | 1);
	qdf_nbuf_set_dev_scratch(buf, dev_scratch);

	wbuff_pool->mem_alloc += qdf_nbuf_get_allocsize(buf);

	return buf;
}

/**
 * wbuff_is_valid_handle() - validate wbuff handle
 * @handle: wbuff handle passed by module
 *
 * Return: true - valid wbuff_handle
 *         false - invalid wbuff_handle
 */
static bool wbuff_is_valid_handle(struct wbuff_handle *handle)
{
	if ((handle) && (handle->id < WBUFF_MAX_MODULES) &&
	    (wbuff.mod[handle->id].registered))
		return true;

	return false;
}

static char *wbuff_get_mod_name(enum wbuff_module_id module_id)
{
	char *str;

	switch (module_id) {
	case WBUFF_MODULE_WMI_TX:
		str = "WBUFF_MODULE_WMI_TX";
		break;
	case WBUFF_MODULE_CE_RX:
		str = "WBUFF_MODULE_CE_RX";
		break;
	default:
		str = "Invalid Module ID";
		break;
	}

	return str;
}

static void wbuff_debugfs_print(qdf_debugfs_file_t file, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	seq_vprintf(file, fmt, args);
	va_end(args);
}

static int wbuff_stats_debugfs_show(qdf_debugfs_file_t file, void *data)
{
	struct wbuff_module *mod;
	struct wbuff_pool *wbuff_pool;
	int i, j;

	wbuff_debugfs_print(file, "WBUFF POOL STATS:\n");
	wbuff_debugfs_print(file, "=================\n");

	for (i = 0; i < WBUFF_MAX_MODULES; i++) {
		mod = &wbuff.mod[i];

		if (!mod->registered)
			continue;

		wbuff_debugfs_print(file, "Module (%d) : %s\n", i,
				    wbuff_get_mod_name(i));

		wbuff_debugfs_print(file, "%s %25s %20s %20s\n", "Pool ID",
				    "Mem Allocated (In Bytes)",
				    "Wbuff Success Count",
				    "Wbuff Fail Count");

		for (j = 0; j < WBUFF_MAX_POOLS; j++) {
			wbuff_pool = &mod->wbuff_pool[j];

			if (!wbuff_pool->initialized)
				continue;

			wbuff_debugfs_print(file, "%d %30llu %20llu %20llu\n",
					    j, wbuff_pool->mem_alloc,
					    wbuff_pool->alloc_success,
					    wbuff_pool->alloc_fail);
		}
		wbuff_debugfs_print(file, "\n");
	}

	return 0;
}

static int wbuff_stats_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, wbuff_stats_debugfs_show,
			   inode->i_private);
}

static const struct file_operations wbuff_stats_fops = {
	.owner          = THIS_MODULE,
	.open           = wbuff_stats_debugfs_open,
	.release        = single_release,
	.read           = seq_read,
	.llseek         = seq_lseek,
};

static QDF_STATUS wbuff_debugfs_init(void)
{
	wbuff.wbuff_debugfs_dir =
		qdf_debugfs_create_dir("wbuff", NULL);

	if (!wbuff.wbuff_debugfs_dir)
		return QDF_STATUS_E_FAILURE;

	wbuff.wbuff_stats_dentry =
		qdf_debugfs_create_entry("wbuff_stats", QDF_FILE_USR_READ,
					 wbuff.wbuff_debugfs_dir, NULL,
					 &wbuff_stats_fops);
	if (!wbuff.wbuff_stats_dentry)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

static void wbuff_debugfs_exit(void)
{
	if (!wbuff.wbuff_debugfs_dir)
		return;

	debugfs_remove_recursive(wbuff.wbuff_debugfs_dir);
	wbuff.wbuff_debugfs_dir = NULL;
}

QDF_STATUS wbuff_module_init(void)
{
	struct wbuff_module *mod = NULL;
	uint8_t module_id = 0, pool_id = 0;

	if (!qdf_nbuf_is_dev_scratch_supported()) {
		wbuff.initialized = false;
		return QDF_STATUS_E_NOSUPPORT;
	}

	for (module_id = 0; module_id < WBUFF_MAX_MODULES; module_id++) {
		mod = &wbuff.mod[module_id];
		qdf_spinlock_create(&mod->lock);
		for (pool_id = 0; pool_id < WBUFF_MAX_POOLS; pool_id++)
			mod->wbuff_pool[pool_id].pool = NULL;
		mod->registered = false;
	}

	wbuff_debugfs_init();

	wbuff.initialized = true;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wbuff_module_deinit(void)
{
	struct wbuff_module *mod = NULL;
	uint8_t module_id = 0;

	if (!wbuff.initialized)
		return QDF_STATUS_E_INVAL;

	wbuff.initialized = false;
	wbuff_debugfs_exit();

	for (module_id = 0; module_id < WBUFF_MAX_MODULES; module_id++) {
		mod = &wbuff.mod[module_id];
		if (mod->registered)
			wbuff_module_deregister((struct wbuff_mod_handle *)
						&mod->handle);
		qdf_spinlock_destroy(&mod->lock);
	}

	return QDF_STATUS_SUCCESS;
}

struct wbuff_mod_handle *
wbuff_module_register(struct wbuff_alloc_request *req, uint8_t num_pools,
		      int reserve, int align, enum wbuff_module_id module_id)
{
	struct wbuff_module *mod = NULL;
	struct wbuff_pool *wbuff_pool;
	qdf_nbuf_t buf = NULL;
	uint32_t len;
	uint16_t pool_size;
	uint8_t pool_id;
	int i;
	int j;

	if (!wbuff.initialized)
		return NULL;

	if ((num_pools == 0) || (num_pools > WBUFF_MAX_POOLS))
		return NULL;

	if (module_id >= WBUFF_MAX_MODULES)
		return NULL;

	if (!wbuff_is_valid_alloc_req(req, num_pools))
		return NULL;

	mod = &wbuff.mod[module_id];
	if (mod->registered)
		return NULL;

	mod->handle.id = module_id;

	for (i = 0; i < num_pools; i++) {
		pool_id = req[i].pool_id;
		pool_size = req[i].pool_size;
		len = req[i].buffer_size;
		wbuff_pool = &mod->wbuff_pool[pool_id];

		if (!pool_size)
			continue;

		/**
		 * Allocate pool_size number of buffers for
		 * the pool given by pool_id
		 */
		for (j = 0; j < pool_size; j++) {
			buf = wbuff_prepare_nbuf(module_id, pool_id, len,
						 reserve, align);
			if (!buf)
				continue;

			if (!wbuff_pool->pool)
				qdf_nbuf_set_next(buf, NULL);
			else
				qdf_nbuf_set_next(buf, wbuff_pool->pool);

			wbuff_pool->pool = buf;
		}

		wbuff_pool->pool_id = pool_id;
		wbuff_pool->buffer_size = len;
		wbuff_pool->initialized = true;
	}

	mod->reserve = reserve;
	mod->align = align;
	mod->registered = true;


	return (struct wbuff_mod_handle *)&mod->handle;
}

QDF_STATUS wbuff_module_deregister(struct wbuff_mod_handle *hdl)
{
	struct wbuff_handle *handle;
	struct wbuff_module *mod = NULL;
	uint8_t module_id = 0, pool_id = 0;
	qdf_nbuf_t first = NULL, buf = NULL;
	struct wbuff_pool *wbuff_pool;

	handle = (struct wbuff_handle *)hdl;

	if ((!wbuff.initialized) || (!wbuff_is_valid_handle(handle)))
		return QDF_STATUS_E_INVAL;

	module_id = handle->id;

	if (module_id >= WBUFF_MAX_MODULES)
		return QDF_STATUS_E_INVAL;

	mod = &wbuff.mod[module_id];

	qdf_spin_lock_bh(&mod->lock);
	for (pool_id = 0; pool_id < WBUFF_MAX_POOLS; pool_id++) {
		wbuff_pool = &mod->wbuff_pool[pool_id];

		if (!wbuff_pool->initialized)
			continue;

		first = wbuff_pool->pool;
		while (first) {
			buf = first;
			first = qdf_nbuf_next(buf);
			qdf_nbuf_free(buf);
		}

		wbuff_pool->mem_alloc = 0;
		wbuff_pool->alloc_success = 0;
		wbuff_pool->alloc_fail = 0;

	}
	mod->registered = false;
	qdf_spin_unlock_bh(&mod->lock);

	return QDF_STATUS_SUCCESS;
}

qdf_nbuf_t
wbuff_buff_get(struct wbuff_mod_handle *hdl, uint8_t pool_id, uint32_t len,
	       const char *func_name, uint32_t line_num)
{
	struct wbuff_handle *handle;
	struct wbuff_module *mod = NULL;
	struct wbuff_pool *wbuff_pool;
	uint8_t module_id = 0;
	qdf_nbuf_t buf = NULL;

	handle = (struct wbuff_handle *)hdl;

	if ((!wbuff.initialized) || (!wbuff_is_valid_handle(handle)) ||
	    ((pool_id >= WBUFF_MAX_POOL_ID && !len)))
		return NULL;

	module_id = handle->id;

	if (module_id >= WBUFF_MAX_MODULES)
		return NULL;

	mod = &wbuff.mod[module_id];

	if (pool_id == WBUFF_MAX_POOL_ID && len)
		pool_id = wbuff_get_pool_slot_from_len(mod, len);

	if (pool_id >= WBUFF_MAX_POOLS)
		return NULL;

	wbuff_pool = &mod->wbuff_pool[pool_id];
	if (!wbuff_pool->initialized)
		return NULL;

	qdf_spin_lock_bh(&mod->lock);
	if (wbuff_pool->pool) {
		buf = wbuff_pool->pool;
		wbuff_pool->pool = qdf_nbuf_next(buf);
		mod->pending_returns++;
	}
	qdf_spin_unlock_bh(&mod->lock);

	if (buf) {
		qdf_nbuf_set_next(buf, NULL);
		qdf_net_buf_debug_update_node(buf, func_name, line_num);
		wbuff_pool->alloc_success++;
	} else {
		wbuff_pool->alloc_fail++;
	}

	return buf;
}

qdf_nbuf_t wbuff_buff_put(qdf_nbuf_t buf)
{
	qdf_nbuf_t buffer = buf;
	unsigned long pool_info = 0;
	uint8_t module_id = 0, pool_id = 0;
	struct wbuff_pool *wbuff_pool;

	if (qdf_nbuf_get_users(buffer) > 1)
		return buffer;

	if (!wbuff.initialized)
		return buffer;

	pool_info = qdf_nbuf_get_dev_scratch(buf);
	if (!pool_info)
		return buffer;

	module_id = (pool_info & WBUFF_MODULE_ID_BITMASK) >>
			WBUFF_MODULE_ID_SHIFT;
	pool_id = (pool_info & WBUFF_POOL_ID_BITMASK) >> WBUFF_POOL_ID_SHIFT;

	if (module_id >= WBUFF_MAX_MODULES || pool_id >= WBUFF_MAX_POOLS)
		return buffer;

	wbuff_pool = &wbuff.mod[module_id].wbuff_pool[pool_id];
	if (!wbuff_pool->initialized)
		return buffer;

	qdf_nbuf_reset(buffer, wbuff.mod[module_id].reserve,
		       wbuff.mod[module_id].align);

	qdf_spin_lock_bh(&wbuff.mod[module_id].lock);
	if (wbuff.mod[module_id].registered) {
		qdf_nbuf_set_next(buffer, wbuff_pool->pool);
		wbuff_pool->pool = buffer;
		wbuff.mod[module_id].pending_returns--;
		buffer = NULL;
	}
	qdf_spin_unlock_bh(&wbuff.mod[module_id].lock);

	return buffer;
}
