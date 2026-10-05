// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file arfw_int_map.c
 *
 * @brief Implementation of common arfw map and dma operations.
 *
 * @details
 *
 *******************************************************************************/

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "arfw_log.h"
#include "arfw_int_map.h"
#include "ar_utils.h"

static void arfw_unmap_direct_region(struct arfw_client_region_mapping *mapping)
{
	AR_ASSERT(mapping);
	AR_ASSERT(mapping->type == ARFW_CLIENT_REGION_DIRECT);

	if (mapping->direct.base_ptr) {
		vunmap(mapping->direct.base_ptr);
		mapping->direct.base_ptr = NULL;
	}

	if (mapping->direct.pages) {
		unpin_user_pages(mapping->direct.pages,
				 mapping->direct.num_pages);
		kfree(mapping->direct.pages);
		mapping->direct.pages = NULL;
	}

	mapping->direct.num_pages = 0;
}

static void arfw_unmap_dmabuf_region(struct arfw_client_region_mapping *mapping)
{
	struct arfw_region_dmabuf_mapping *buf;

	AR_ASSERT(mapping);
	AR_ASSERT(mapping->type == ARFW_CLIENT_REGION_DMABUF);
	buf = &mapping->dmabuf;

	if (buf->att && buf->sgt) {
		dma_buf_unmap_attachment(buf->att, buf->sgt,
					 mapping->dma_direction);
		buf->sgt = NULL;
	}
	if (buf->buf && buf->att) {
		dma_buf_detach(buf->buf, buf->att);
		buf->att = NULL;
	}

	if (buf->buf) {
		dma_buf_put(buf->buf);
		buf->buf = NULL;
	}
}

void arfw_unmap_region(struct arfw_client_region_mapping *mapping)
{
	AR_ASSERT(mapping);

	switch (mapping->type) {
	case ARFW_CLIENT_REGION_DIRECT:
		arfw_unmap_direct_region(mapping);
		break;
	case ARFW_CLIENT_REGION_DMABUF:
		arfw_unmap_dmabuf_region(mapping);
		break;
	default:
		AR_ASSERT(false);
		break;
	}

	mapping->region_size = 0;
}

static int arfw_map_direct_region(const struct ar_mem_region *region,
				  struct arfw_client_region_mapping *mapping,
				  bool map_kernel)
{
	int num_pages = 0, num_pinned = 0, err = 0;
	int temp, pages_to_pin, pages_pinned;
	const struct ar_mem_segment *segment;
	struct page **pages;

	AR_ASSERT(region);
	AR_ASSERT(mapping);
	AR_ASSERT(mapping->type == ARFW_CLIENT_REGION_DIRECT);

	if (region->segment_count > AR_REGION_MAX_SEGMENTS) {
		AR_LOG_ARFW_ERR(AR_LOG_VMAP_MEM,
				"Region has too many segments [count %u]",
				region->segment_count);
		return -EINVAL;
	}

	// validate memory region and calculate number of pages
	for (temp = 0; temp < region->segment_count; temp++) {
		segment = &region->segments[temp];
		if (((uintptr_t)segment->ptr & (PAGE_SIZE - 1)) ||
		    (segment->size & (PAGE_SIZE - 1))) {
			AR_LOG_ARFW_ERR(
				AR_LOG_VMAP_MEM,
				"Region not aligned [segment ptr %p segment size %u]",
				segment->ptr, segment->size);
			return -EINVAL;
		}

		num_pages += segment->size / PAGE_SIZE;
	}

	// allocate pages information
	pages = kcalloc(num_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	// pin and get the user pages
	for (temp = 0; temp < region->segment_count; temp++) {
		segment = &region->segments[temp];
		pages_to_pin = segment->size / PAGE_SIZE;

		pages_pinned = pin_user_pages_fast((uintptr_t)segment->ptr,
						   pages_to_pin, FOLL_WRITE,
						   &pages[num_pinned]);

		if (pages_pinned < pages_to_pin) {
			err = -EFAULT;
			AR_LOG_ARFW_ERR(
				AR_LOG_VMAP_MEM,
				"Failed to get user pages [segment ptr %p segment size %d err %u]",
				segment->ptr, segment->size, pages_pinned);
			goto put_pages;
		}

		num_pinned += pages_pinned;
	}

	// We only need to map pages if we are mapping queues.
	// There is no need for waste on buffers.
	if (map_kernel) {
		mapping->direct.base_ptr = vmap(
			pages, num_pages, VM_MAP | VM_USERMAP, PAGE_KERNEL);
		if (mapping->direct.base_ptr == NULL) {
			err = -ENOMEM;
			goto put_pages;
		}
	}

	mapping->direct.pages = pages;
	mapping->direct.num_pages = num_pages;
	mapping->region_size = num_pages * PAGE_SIZE;

	return 0;

put_pages:
	// clean up any pinned pages
	if (num_pages) {
		for (temp = 0; temp < num_pinned; temp++)
			put_page(pages[temp]);
	}
	kfree(pages);
	return err;
}

// Walk down the sg table and check all chunks if they are sequential.
// Currently we only support one chunk regions, but iommu might spit out
// a number of sequential chunks. For instance, this is the case for intel-iommu.
static bool arfw_check_dmabuf_chunks(struct sg_table *sgt, size_t *len)
{
	struct scatterlist *sg;
	int i;
	dma_addr_t addr;

	AR_ASSERT(sgt);
	AR_ASSERT(len);

	addr = sg_dma_address(sgt->sgl);
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		if (addr != sg_dma_address(sg)) {
			AR_LOG_ARFW_ERR(
				AR_LOG_VMAP_MEM,
				"Found a non-sequential buffer chunk prev=%llx, next=%llx",
				addr, sg_dma_address(sg));
			return false;
		}
		addr = sg_dma_address(sg) + sg_dma_len(sg);
		*len += sg_dma_len(sg);
	}

	return true;
}

static int arfw_map_dmabuf_region(const struct ar_mem_region *region,
				  struct arfw_client_region_mapping *mapping)
{
	int err = 0;
	size_t region_size = 0;
	struct arfw_region_dmabuf_mapping *buf;

	// For now we only support regions backed by a single dmabuf for simplicity.
	AR_ASSERT(region);
	AR_ASSERT(region->segment_count == 1);

	AR_ASSERT(mapping);
	AR_ASSERT(mapping->type == ARFW_CLIENT_REGION_DMABUF);
	buf = &mapping->dmabuf;

	if (!mapping->hw_dev) {
		err = -ENODEV;
		AR_LOG_ARFW_DBG(AR_LOG_VMAP_MEM,
				"Device does not support dma buf, err=%d", err);
		return err;
	}

	buf->buf = dma_buf_get(region->segments[0].mem_fd);
	if (IS_ERR_OR_NULL(buf->buf)) {
		err = PTR_ERR(buf->buf);
		AR_LOG_ARFW_DBG(AR_LOG_VMAP_MEM,
				"Failed to get dma buf, err=%d", err);
		return err;
	}

	buf->att = dma_buf_attach(buf->buf, mapping->hw_dev);
	if (IS_ERR_OR_NULL(buf->att)) {
		err = PTR_ERR(buf->att);
		AR_LOG_ARFW_ERR(AR_LOG_VMAP_MEM,
				"Failed to attach dma buf, err=%d", err);
		goto free_buf;
	}

	buf->sgt = dma_buf_map_attachment(buf->att, mapping->dma_direction);
	if (IS_ERR_OR_NULL(buf->sgt)) {
		err = PTR_ERR(buf->sgt);
		AR_LOG_ARFW_ERR(AR_LOG_VMAP_MEM,
				"Failed to map dma buf attachment, err=%d",
				err);
		goto free_att;
	} else if (!buf->sgt->sgl || buf->sgt->nents == 0) {
		err = -EINVAL;
		AR_LOG_ARFW_ERR(AR_LOG_VMAP_MEM,
				"No chunks in this buffer, err=%d", err);
		goto free_sgt;
	} else if (!arfw_check_dmabuf_chunks(buf->sgt, &region_size)) {
		err = -ENOSPC;
		AR_LOG_ARFW_ERR(AR_LOG_VMAP_MEM,
				"Buffer chunks need to be sequential, err=%d",
				err);
		goto free_sgt;
	} else if (region_size != region->segments[0].size) {
		err = -EINVAL;
		AR_LOG_ARFW_ERR(
			AR_LOG_VMAP_MEM,
			"Buffer requested region size %x does not match dmabuf reported size %zx, err=%d",
			region->segments[0].size, region_size, err);
		goto free_sgt;
	}

	// We cannot expose only a section of the region due to how dmabuf works.
	// For now we assert that the region size passed matches the size dmabuf reports.
	// In the future we might want to drop the size field for these types of buffers.
	mapping->region_size = region_size;
	mapping->dma_region_addr = sg_dma_address(buf->sgt->sgl);
	mapping->dma_region_offset = 0;
	mapping->dma_region_size = region_size;

	return 0;

free_sgt:
	dma_buf_unmap_attachment(buf->att, buf->sgt, mapping->dma_direction);
free_att:
	dma_buf_detach(buf->buf, buf->att);
free_buf:
	dma_buf_put(buf->buf);
	return err;
}

int arfw_map_region(const struct ar_mem_region *region,
		    struct arfw_client_region_mapping *mapping, bool map_kernel)
{
	int err = -EINVAL;

	AR_ASSERT(mapping);
	switch (mapping->type) {
	case ARFW_CLIENT_REGION_DIRECT:
		err = arfw_map_direct_region(region, mapping, map_kernel);
		break;
	case ARFW_CLIENT_REGION_DMABUF:
		// We will only support DMA BUF regions for user buffers.
		// Queues are not allowed for now to use this mapping type.
		AR_ASSERT(!map_kernel);
		err = arfw_map_dmabuf_region(region, mapping);
		break;
	default:
		AR_ASSERT(false);
		break;
	}

	return err;
}

void arfw_unmap_queue(struct arfw_client_queue *client)
{
	struct arfw_client_region_mapping *queue_mapping =
		&client->queue->queue_mapping;
	arfw_unmap_region(queue_mapping);
}

int arfw_map_queue(struct arfw_client_queue *client,
		   struct ar_mem_segment *queue_meta_segment)
{
	struct arfw_client_region_mapping *queue_mapping;
	struct ar_mem_region region;

	AR_ASSERT(client);
	AR_ASSERT(client->queue);
	AR_ASSERT(queue_meta_segment);
	AR_ASSERT(!lifetime_lock_is_torn_down(&client->lt_lock));

	queue_mapping = &client->queue->queue_mapping;
	queue_mapping->type = ARFW_CLIENT_REGION_DIRECT;
	queue_mapping->dma_direction =
		client->queue->queue_meta.queue_direction == SEND ?
			      DMA_TO_DEVICE :
			      DMA_FROM_DEVICE;

	AR_ASSERT(queue_mapping->direct.base_ptr == NULL);
	AR_ASSERT(queue_mapping->dma_region_addr == 0);

	// we only accept page aligned queue base addresses.
	if (!AR_IS_ALIGNED(queue_meta_segment->ptr, PAGE_SIZE)) {
		AR_LOG_ARFW_QUEUE_ERR(client->queue, AR_LOG_CREATE_QUEUE,
				      "Queue segment not page aligned",
				      "queue ptrs %p", queue_meta_segment->ptr);
		return -EINVAL;
	}

	// map the metadata into kernel memory as a contiguous chunk
	region.segments[0] = *queue_meta_segment;
	region.segment_count = 1;

	return arfw_map_region(&region, queue_mapping,
			       /* map_kernel = */ true);
}

static int do_dma_map(struct arfw_client_queue *client,
		      struct arfw_client_region_mapping *mapping)
{
	int err = 0;
	int page_offset = 0, page_count = 0;

	AR_ASSERT(client);
	AR_ASSERT(client->queue);
	AR_ASSERT(client->queue->driver_ops);

	if (client->queue->driver_ops->handle_dma_map_quirk) {
		AR_ASSERT(mapping);
		AR_ASSERT(mapping->type == ARFW_CLIENT_REGION_DIRECT);

		// calculate page offset
		AR_ASSERT(mapping->dma_region_offset % PAGE_SIZE == 0);
		page_offset = mapping->dma_region_offset / PAGE_SIZE;

		AR_ASSERT(page_offset < mapping->direct.num_pages);
		AR_ASSERT(mapping->dma_region_size % PAGE_SIZE == 0);

		// calculate page count
		page_count = mapping->dma_region_size / PAGE_SIZE;
		AR_ASSERT(page_offset + page_count <=
			  mapping->direct.num_pages);

		err = client->queue->driver_ops->handle_dma_map_quirk(
			client, client->queue->base_context,
			client->queue->queue_context, mapping->dma_direction,
			&mapping->direct.sgt,
			&mapping->direct.pages[page_offset], page_count,
			&mapping->dma_region_addr);

		if (!err)
			AR_LOG_ARFW_QUEUE_DBG(
				client->queue, AR_LOG_DMA_MAP_MEM,
				"dma map memory",
				"kptr (santized): %p, dma addr: %llx",
				(void *)((uintptr_t)mapping->direct.base_ptr +
					 mapping->dma_region_offset),
				mapping->dma_region_addr);
	}

	return err;
}

static void do_dma_unmap(struct arfw_client_queue *client,
			 struct arfw_client_region_mapping *mapping)
{
	AR_ASSERT(client);
	AR_ASSERT(client->queue);
	AR_ASSERT(client->queue->driver_ops);

	if (client->queue->driver_ops->handle_dma_unmap_quirk) {
		AR_ASSERT(mapping);
		AR_ASSERT(mapping->type == ARFW_CLIENT_REGION_DIRECT);

		AR_LOG_ARFW_QUEUE_DBG(
			client->queue, AR_LOG_DMA_MAP_MEM, "dma unmap memory",
			"kptr (santized): %p, dma addr: %llx",
			(void *)((uintptr_t)mapping->direct.base_ptr +
				 mapping->dma_region_offset),
			mapping->dma_region_addr);

		client->queue->driver_ops->handle_dma_unmap_quirk(
			client, client->queue->base_context,
			client->queue->queue_context, mapping->dma_direction,
			&mapping->direct.sgt,
			(void *)((uintptr_t)mapping->direct.base_ptr +
				 mapping->dma_region_offset),
			mapping->dma_region_size, mapping->dma_region_addr);
	}
}

int arfw_dma_map_region(struct arfw_client_queue *client, uint16_t region_id,
			struct arfw_client_region_mapping *mapping)
{
	int err = 0;

	AR_ASSERT(client);
	AR_ASSERT(mapping);
	AR_ASSERT(!lifetime_lock_is_torn_down(&client->lt_lock));

	switch (mapping->type) {
	case ARFW_CLIENT_REGION_DIRECT:
		err = do_dma_map(client, mapping);
		break;
	case ARFW_CLIENT_REGION_DMABUF:
		// DMA BUF backed regions need no additional dma mapping support.
		// They are already DMA-able by design.
		// Skipping...
		break;
	default:
		AR_ASSERT(false);
		break;
	}

	if (!err && client->queue->driver_ops->handle_region_add)
		err = client->queue->driver_ops->handle_region_add(
			client->queue->base_context,
			client->queue->queue_context, region_id,
			mapping->dma_region_size, mapping->dma_region_addr);

	return err;
}

void arfw_dma_unmap_region(struct arfw_client_queue *client, uint16_t region_id,
			   struct arfw_client_region_mapping *mapping)
{
	AR_ASSERT(mapping);

	if (!mapping->dma_region_addr)
		return;

	switch (mapping->type) {
	case ARFW_CLIENT_REGION_DIRECT:
		do_dma_unmap(client, mapping);
		break;
	case ARFW_CLIENT_REGION_DMABUF:
		// DMA BUF backed regions need no additional dma mapping support.
		// They are already DMA-able by design.
		// Skipping...
		break;
	default:
		AR_ASSERT(false);
		break;
	}

	if (client->queue->driver_ops->handle_region_del)
		client->queue->driver_ops->handle_region_del(
			client->queue->base_context,
			client->queue->queue_context, region_id);

	mapping->dma_region_addr = 0;
}
