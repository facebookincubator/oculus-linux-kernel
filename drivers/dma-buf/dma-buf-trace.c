/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/dma-buf/dma-buf-trace.c
 *
 * Copyright (C) 2022 Meta Inc.
 */

#include <linux/dma-buf.h>

#define CREATE_TRACE_POINTS
#include "dma-buf-trace.h"

static atomic_long_t total_dma_buf_bytes;

void track_dma_buf_created(struct dma_buf *buf)
{
	long total;

	if (IS_ERR(buf)) {
		WARN_ONCE(1, "Track on invalid dma buffer during create");
		return;
	}

	total = atomic_long_add_return(buf->size, &total_dma_buf_bytes);
	trace_dma_heap_stat(file_inode(buf->file)->i_ino, buf->size, total);
}

void track_dma_buf_destroyed(struct dma_buf *buf)
{
	long total = atomic_long_sub_return(buf->size, &total_dma_buf_bytes);
	trace_dma_heap_stat(file_inode(buf->file)->i_ino, -buf->size, total);
}
