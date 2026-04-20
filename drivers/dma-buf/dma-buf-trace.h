/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/dma-buf/dma-buf-trace.h
 *
 * Copyright (C) 2022 Meta Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM dmabuf_heap

#if !defined(_DMA_BUF_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DMA_BUF_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(dma_heap_stat,
	TP_PROTO(unsigned long inode, long len,
		unsigned long total_allocated),
	TP_ARGS(inode, len, total_allocated),
	TP_STRUCT__entry(__field(unsigned long, inode)
		__field(long, len)
		__field(unsigned long, total_allocated)
	),
	TP_fast_assign(__entry->inode = inode;
		__entry->len = len;
		__entry->total_allocated = total_allocated;
	),
	TP_printk("inode=%lu len=%ldB total_allocated=%luB",
		__entry->inode,
		__entry->len,
		__entry->total_allocated)
);

void track_dma_buf_created(struct dma_buf *buf);
void track_dma_buf_destroyed(struct dma_buf *buf);

#endif /* _DMA_BUF_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE dma-buf-trace
#include <trace/define_trace.h>
