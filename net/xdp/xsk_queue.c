// SPDX-License-Identifier: GPL-2.0
/* XDP user-space ring structure
 * Copyright(c) 2018 Intel Corporation.
 */

#include <linux/slab.h>

#include "xsk_queue.h"

void xskq_set_umem(struct xsk_queue *q, struct xdp_umem_props *umem_props)
{
	if (!q)
		return;

	q->umem_props = *umem_props;
}

static u32 xskq_umem_get_ring_size(struct xsk_queue *q)
{
	return sizeof(struct xdp_umem_ring) + q->nentries * sizeof(u64);
}

static u32 xskq_rxtx_get_ring_size(struct xsk_queue *q)
{
	return sizeof(struct xdp_ring) + q->nentries * sizeof(struct xdp_desc);
}

struct xsk_queue *xskq_create(u32 nentries, bool umem_queue)
{
	struct xsk_queue *q;
	gfp_t gfp_flags;
	size_t size;

	q = kzalloc(sizeof(*q), GFP_KERNEL);
	if (!q)
		return NULL;

	q->nentries = nentries;
	q->ring_mask = nentries - 1;

	gfp_flags = GFP_KERNEL | __GFP_ZERO | __GFP_NOWARN |
		    __GFP_COMP  | __GFP_NORETRY;
	size = umem_queue ? xskq_umem_get_ring_size(q) :
	       xskq_rxtx_get_ring_size(q);

	q->ring = (struct xdp_ring *)__get_free_pages(gfp_flags,
						      get_order(size));
	if (!q->ring) {
		kfree(q);
		return NULL;
	}

	return q;
}

void xskq_destroy(struct xsk_queue *q)
{
	if (!q)
		return;

	page_frag_free(q->ring);
	kfree(q);
}
