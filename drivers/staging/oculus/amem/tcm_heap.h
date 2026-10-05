/* SPDX-License-Identifier: GPL+
 *
 * Copyright Meta Platforms, Inc. and its affiliates.
 */

#ifndef _TCM_HEAP_H
#define _TCM_HEAP_H

/**
 * @file tcm_heap.c
 *
 * @brief Implementation of a simple heap of free TCM page frame numbers.
 *
 * @details The heap is thread-safe if you use only these four functions:
 * tcm_heap_create
 * tcm_heap_get_size
 * tcm_heap_destroy
 * tcm_heap_allocate
 * tcm_heap_free
 *
 */

#include <linux/spinlock.h>

// u16 supports a maximum of 65535 pages of TCM.
// With 4KB pages, this is 256MB.
typedef u16 tcm_pfn_t;

#define TCM_HEAP_OOM ((tcm_pfn_t)-1)

struct tcm_heap {
	spinlock_t lock;
	int free_size;
	int heap_size;

	// Commonly used "Struct hack" to allow dynamic size of the heap
	tcm_pfn_t heap[];
};

static int tcm_heap_get_size(struct tcm_heap *h)
{
	if (!h)
		return 0;

	return h->heap_size;
}

static struct tcm_heap *tcm_heap_create(int heap_size)
{
	int i;
	struct tcm_heap *h;

	int tcm_heap_mem_size;

	// Add space for the heap that follows the struct tcm_heap
	tcm_heap_mem_size = sizeof(struct tcm_heap);
	tcm_heap_mem_size += sizeof(tcm_pfn_t) * heap_size;

	h = kzalloc(tcm_heap_mem_size, GFP_KERNEL);
	if (!h)
		return NULL;

	spin_lock_init(&h->lock);
	h->free_size = heap_size;
	h->heap_size = heap_size;

	// Initially fill the heap with all the free TCM PFNs
	for (i = 0; i < heap_size; i++)
		h->heap[i] = i;

	return h;
}

static void tcm_heap_destroy(struct tcm_heap *h)
{
	if (!h)
		return;

	// This checks for leaks
	if (h->free_size != h->heap_size)
		BUG();

	// Free the struct
	kfree(h);
}

__always_inline void tcm_swap(tcm_pfn_t *a, tcm_pfn_t *b)
{
	tcm_pfn_t temp = *a;
	*a = *b;
	*b = temp;
}

// heapify the 'free' side.
static void tcm_min_heapify(struct tcm_heap *h, tcm_pfn_t i)
{
	tcm_pfn_t smallest = i;
	tcm_pfn_t left = 2 * i + 1;
	tcm_pfn_t right = 2 * i + 2;

	if (left < h->free_size && h->heap[left] < h->heap[smallest])
		smallest = left;

	if (right < h->free_size && h->heap[right] < h->heap[smallest])
		smallest = right;

	if (smallest != i) {
		tcm_swap(&h->heap[i], &h->heap[smallest]);
		tcm_min_heapify(h, smallest);
	}
}

static tcm_pfn_t tcm_heap_allocate(struct tcm_heap *h)
{
	tcm_pfn_t root;
	unsigned long flags;

	spin_lock_irqsave(&h->lock, flags);

	if (h->free_size == 0) {
		spin_unlock_irqrestore(&h->lock, flags);
		return TCM_HEAP_OOM;
	}

	root = h->heap[0];

	h->heap[0] = h->heap[--h->free_size];
	tcm_min_heapify(h, 0);

	spin_unlock_irqrestore(&h->lock, flags);
	return root;
}

static void tcm_heap_free(struct tcm_heap *h, tcm_pfn_t key)
{
	tcm_pfn_t i;
	unsigned long flags;

	spin_lock_irqsave(&h->lock, flags);

	i = h->free_size++;
	h->heap[i] = key;

	while (i != 0 && h->heap[(i - 1) / 2] > h->heap[i]) {
		tcm_swap(&h->heap[i], &h->heap[(i - 1) / 2]);
		i = (i - 1) / 2;
	}
	spin_unlock_irqrestore(&h->lock, flags);
}

#endif // _TCM_HEAP_H
