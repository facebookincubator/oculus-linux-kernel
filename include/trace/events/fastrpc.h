/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM fastrpc

#if !defined(_TRACE_FASTRPC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_FASTRPC_H
#include <linux/tracepoint.h>

TRACE_EVENT(fastrpc_glink_send,

	TP_PROTO(int cid, uint64_t smq_ctx,
		uint64_t ctx, uint32_t handle,
		uint32_t sc, uint64_t addr, uint64_t size),

	TP_ARGS(cid, smq_ctx, ctx, handle, sc, addr, size),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(uint64_t, smq_ctx)
		__field(uint64_t, ctx)
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(uint64_t, addr)
		__field(uint64_t, size)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
		__entry->addr = addr;
		__entry->size = size;
	),

	TP_printk("to cid %d: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x, addr 0x%llx, size %llu",
		__entry->cid, __entry->smq_ctx, __entry->ctx, __entry->handle,
		__entry->sc, __entry->addr, __entry->size)
);

TRACE_EVENT(fastrpc_glink_response,

	TP_PROTO(int cid, uint64_t smq_ctx,
		uint64_t ctx, uint32_t handle,
		uint32_t sc, uint64_t addr, uint64_t size),

	TP_ARGS(cid, smq_ctx, ctx, handle, sc, addr, size),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(uint64_t, smq_ctx)
		__field(uint64_t, ctx)
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(uint64_t, addr)
		__field(uint64_t, size)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
		__entry->addr = addr;
		__entry->size = size;
	),

	TP_printk("to cid %d: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x, addr 0x%llx, size %llu",
		__entry->cid, __entry->smq_ctx, __entry->ctx, __entry->handle,
		__entry->sc, __entry->addr, __entry->size)
);

TRACE_EVENT(fastrpc_context_interrupt,

	TP_PROTO(int cid, uint64_t smq_ctx, uint64_t ctx,
		uint32_t handle, uint32_t sc),

	TP_ARGS(cid, smq_ctx, ctx, handle, sc),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(uint64_t, smq_ctx)
		__field(uint64_t, ctx)
		__field(uint32_t, handle)
		__field(uint32_t, sc)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
	),

	TP_printk("to cid %d: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x",
		__entry->cid, __entry->smq_ctx,
		__entry->ctx, __entry->handle, __entry->sc)
);


TRACE_EVENT(fastrpc_dma_alloc_start,

	TP_PROTO(int cid, uint64_t phys, size_t size),

	TP_ARGS(cid, phys, size),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(uint64_t, phys)
		__field(size_t, size)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->phys = phys;
		__entry->size = size
	),

	TP_printk("cid %d, phys 0x%llx, size %zu",
		__entry->cid, __entry->phys, __entry->size)
);

TRACE_EVENT(fastrpc_dma_alloc_end,

	TP_PROTO(int cid, uint64_t phys, size_t size),

	TP_ARGS(cid, phys, size),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(uint64_t, phys)
		__field(size_t, size)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->phys = phys;
		__entry->size = size
	),

	TP_printk("cid %d, phys 0x%llx, size %zu",
		__entry->cid, __entry->phys, __entry->size)
);

TRACE_EVENT(fastrpc_invoke_start,

	TP_PROTO(int cid, uint64_t ctx),

	TP_ARGS(cid, ctx),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(uint64_t, ctx)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->ctx = ctx;
	),

	TP_printk("cid %d,  ctx 0x%llx",
		__entry->cid, __entry->ctx)
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
