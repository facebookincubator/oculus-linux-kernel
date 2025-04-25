/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: i_wbuff.h
 * wbuff private
 */

#ifndef _I_WBUFF_H
#define _I_WBUFF_H

#include <qdf_nbuf.h>
#include <wbuff.h>

#define WBUFF_MODULE_ID_SHIFT 4
#define WBUFF_MODULE_ID_BITMASK 0xF0

#define WBUFF_POOL_ID_SHIFT 1
#define WBUFF_POOL_ID_BITMASK 0xE

/**
 * struct wbuff_handle - wbuff handle to the registered module
 * @id: the identifier for the registered module.
 */
struct wbuff_handle {
	uint8_t id;
};

/**
 * struct wbuff_pool - structure representing wbuff pool
 * @initialized: To identify whether pool is initialized
 * @pool: nbuf pool
 * @buffer_size: size of the buffer in this @pool
 * @pool_id: pool identifier
 * @alloc_success: Successful allocations for this pool
 * @alloc_fail: Failed allocations for this pool
 * @mem_alloc: Memory allocated for this pool
 */
struct wbuff_pool {
	bool initialized;
	qdf_nbuf_t pool;
	uint16_t buffer_size;
	uint8_t pool_id;
	uint64_t alloc_success;
	uint64_t alloc_fail;
	uint64_t mem_alloc;
};

/**
 * struct wbuff_module - allocation holder for wbuff registered module
 * @registered: To identify whether module is registered
 * @pending_returns: Number of buffers pending to be returned to
 * wbuff by the module
 * @lock: Lock for accessing per module buffer pools
 * @handle: wbuff handle for the registered module
 * @reserve: nbuf headroom to start with
 * @align: alignment for the nbuf
 * @wbuff_pool: pools for all available buffers for the module
 */
struct wbuff_module {
	bool registered;
	uint16_t pending_returns;
	qdf_spinlock_t lock;
	struct wbuff_handle handle;
	int reserve;
	int align;
	struct wbuff_pool wbuff_pool[WBUFF_MAX_POOLS];
};

/**
 * struct wbuff_holder - allocation holder for wbuff
 * @initialized: to identified whether module is initialized
 * @mod: list of modules
 * @pf_cache: Reference to page frag cache, used for nbuf allocations
 * @wbuff_debugfs_dir: wbuff debugfs root directory
 * @wbuff_stats_dentry: wbuff debugfs stats file
 */
struct wbuff_holder {
	bool initialized;
	struct wbuff_module mod[WBUFF_MAX_MODULES];
	qdf_frag_cache_t pf_cache;
	struct dentry *wbuff_debugfs_dir;
	struct dentry *wbuff_stats_dentry;
};
#endif /* _WBUFF_H */
