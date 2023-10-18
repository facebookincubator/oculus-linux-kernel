/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WLAN_DP_PREALLOC_H
#define _WLAN_DP_PREALLOC_H

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_dp_rx_thread.h>
#include <qdf_trace.h>
#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>

#ifdef DP_MEM_PRE_ALLOC
/**
 * dp_prealloc_init() - Pre-allocate DP memory
 * @ctrl_psoc: objmgr psoc
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc);

/**
 * dp_prealloc_deinit() - Free pre-alloced DP memory
 *
 * Return: None
 */
void dp_prealloc_deinit(void);

/**
 * dp_prealloc_get_context_memory() - gets pre-alloc DP context memory from
 *				      global pool
 * @ctxt_type: type of DP context
 * @ctxt_size: size of memory needed
 *
 * This is done only as part of init happening in a single context. Hence
 * no lock is used for protection
 *
 * Return: Address of context
 */
void *dp_prealloc_get_context_memory(uint32_t ctxt_type, qdf_size_t ctxt_size);

/**
 * dp_prealloc_put_context_memory() - puts back pre-alloc DP context memory to
 *				      global pool
 * @ctxt_type: type of DP context
 * @vaddr: address of DP context
 *
 * This is done only as part of de-init happening in a single context. Hence
 * no lock is used for protection
 *
 * Return: Failure if address not found
 */
QDF_STATUS dp_prealloc_put_context_memory(uint32_t ctxt_type, void *vaddr);

/**
 * dp_prealloc_get_coherent() - gets pre-alloc DP memory
 * @size: size of memory needed
 * @base_vaddr_unaligned: Unaligned virtual address.
 * @paddr_unaligned: Unaligned physical address.
 * @paddr_aligned: Aligned physical address.
 * @align: Base address alignment.
 * @align: alignment needed
 * @ring_type: HAL ring type
 *
 * The function does not handle concurrent access to pre-alloc memory.
 * All ring memory allocation from pre-alloc memory should happen from single
 * context to avoid race conditions.
 *
 * Return: unaligned virtual address if success or null if memory alloc fails.
 */
void *dp_prealloc_get_coherent(uint32_t *size, void **base_vaddr_unaligned,
			       qdf_dma_addr_t *paddr_unaligned,
			       qdf_dma_addr_t *paddr_aligned,
			       uint32_t align,
			       uint32_t ring_type);

/**
 * dp_prealloc_put_coherent() - puts back pre-alloc DP memory
 * @size: size of memory to be returned
 * @vaddr_unligned: Unaligned virtual address.
 * @paddr: Physical address
 *
 * Return: None
 */
void dp_prealloc_put_coherent(qdf_size_t size, void *vaddr_unligned,
			      qdf_dma_addr_t paddr);

/**
 * dp_prealloc_get_multi_pages() - gets pre-alloc DP multi-pages memory
 * @src_type: the source that do memory allocation
 * @element_size: single element size
 * @element_num: total number of elements should be allocated
 * @pages: multi page information storage
 * @cacheable: coherent memory or cacheable memory
 *
 * Return: None.
 */
void dp_prealloc_get_multi_pages(uint32_t src_type,
				 qdf_size_t element_size,
				 uint16_t element_num,
				 struct qdf_mem_multi_page_t *pages,
				 bool cacheable);

/**
 * dp_prealloc_put_multi_pages() - puts back pre-alloc DP multi-pages memory
 * @src_type: the source that do memory freement
 * @pages: multi page information storage
 *
 * Return: None
 */
void dp_prealloc_put_multi_pages(uint32_t src_type,
				 struct qdf_mem_multi_page_t *pages);

/**
 * dp_prealloc_get_consistent_mem_unaligned() - gets pre-alloc unaligned
 *						consistent memory
 * @size: total memory size
 * @base_addr: pointer to dma address
 * @ring_type: HAL ring type that requires memory
 *
 * Return: memory virtual address pointer on success, NULL on failure
 */
void *dp_prealloc_get_consistent_mem_unaligned(qdf_size_t size,
					       qdf_dma_addr_t *base_addr,
					       uint32_t ring_type);

/**
 * dp_prealloc_put_consistent_mem_unaligned() - puts back pre-alloc unaligned
 *						consistent memory
 * @va_unaligned: memory virtual address pointer
 *
 * Return: None
 */
void dp_prealloc_put_consistent_mem_unaligned(void *va_unaligned);

#else
static inline
QDF_STATUS dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_prealloc_deinit(void) { }

#endif

uint32_t dp_get_tx_inqueue(ol_txrx_soc_handle soc);

#endif /* _WLAN_DP_PREALLOC_H */
