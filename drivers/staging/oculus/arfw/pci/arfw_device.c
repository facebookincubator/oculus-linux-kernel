// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_device.c
 *
 * @brief AR driver for PCI based AR accelerator
 *
 * @details
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include "arfw_device.h"

#include <linux/err.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <arfw_log.h>
#include <ar_utils.h>

#include "ar_pci_bar.h"
#include "data_ring.h"
#include "ar_future.h"

bool ar_dev_handle_send_queue_request(void *queue_context,
				      const struct arfw_io_request *req,
				      bool *consumed)
{
	int ret;
	ar_client_queue_t *client_queue = (ar_client_queue_t *)queue_context;

	AR_ASSERT(client_queue);
	AR_ASSERT(req);

	ret = ar_pci_send_ring_data(client_queue, req, consumed);

	AR_LOG_PCI_QUEUE_DBG(client_queue, AR_LOG_WRITE,
			     "Send [queue idx: %d, err: %d]", req->index, !ret);

	return ret;
}

void ar_dev_handle_rcv_queue_consume(arfw_client_queue_t queue,
				     void *queue_context)
{
	ar_client_queue_t *client_queue = (ar_client_queue_t *)queue_context;

	ar_client_rcv_ring_update_read_ptr(queue, client_queue);
}

struct arfw_queue_mem_region *ar_dev_handle_queue_data_alloc(
	void *base_context,
	const struct arfw_client_queue_create_params *queue_params)
{
	struct pci_dev *dev = (struct pci_dev *)base_context;
	struct arfw_queue_mem_region *queue_data;
	ar_pci_queue_data_t *data_context;

	AR_ASSERT(dev);
	AR_ASSERT(queue_params);

	queue_data = kzalloc(sizeof(*queue_data), GFP_KERNEL);
	if (!queue_data)
		goto fail_queue_data;
	data_context =
		devm_kzalloc(&dev->dev, sizeof(*data_context), GFP_KERNEL);
	if (!data_context)
		goto fail_data_context;
	queue_data->context = data_context;

	pci_dev_get(dev);
	queue_data->size = AR_ROUNDUP(
		queue_params->element_size * queue_params->depth, PAGE_SIZE);
	queue_data->ptr = kzalloc(queue_data->size, GFP_USER);
	if (!queue_data->ptr)
		goto fail_dma_alloc;
	data_context->dma_addr = dma_map_single(&dev->dev, queue_data->ptr,
						queue_data->size,
						DMA_BIDIRECTIONAL);
	if (dma_mapping_error(&dev->dev, data_context->dma_addr))
		goto fail_dma_map;
	kref_init(&queue_data->refcnt);

	return queue_data;

fail_dma_map:
	kfree(queue_data->ptr);
fail_dma_alloc:
	pci_dev_put(dev);
	devm_kfree(&dev->dev, data_context);
fail_data_context:
	kfree(queue_data);
fail_queue_data:
	return NULL;
}

void ar_dev_handle_queue_data_free(void *base_context,
				   struct arfw_queue_mem_region *queue_data)
{
	struct pci_dev *dev = (struct pci_dev *)base_context;
	ar_pci_queue_data_t *data_context;

	AR_ASSERT(dev);
	AR_ASSERT(queue_data);
	data_context = queue_data->context;
	AR_ASSERT(data_context);

	dma_unmap_single(&dev->dev, data_context->dma_addr, queue_data->size,
			 DMA_BIDIRECTIONAL);
	pci_dev_put(dev);

	devm_kfree(&dev->dev, data_context);
	// queue_data and queue_data->ptr should be freed outside the device specific
	// context. The generic layer decide when to clean up it, based on the last
	// reference in the user space.
}

int ar_dev_handle_queue_create_request(
	arfw_client_queue_t queue, void *base_context,
	const struct arfw_client_queue_create_params *queue_params,
	struct arfw_queue_mem_region *queue_data, void **queue_context)
{
	ar_future_t *result;
	struct pci_dev *dev = (struct pci_dev *)base_context;

	AR_ASSERT(dev);
	AR_ASSERT(queue_context);

	result = ar_pci_create_data_ring(dev, queue_params, queue, queue_data,
					 ctrl_ring_timeout_ms_param);

	return IS_ERR(result) ? PTR_ERR(result) :
				      ar_future_wait_data(result, queue_context);
}

int ar_dev_handle_queue_destroy_request(void *base_context, void *queue_context)
{
	ar_future_t *result;
	struct pci_dev *dev = (struct pci_dev *)base_context;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	AR_ASSERT(queue_context);
	AR_ASSERT(dev);

	/*
	 * Skip individual queue destruction if group shutdown was already performed.
	 * WHY: This functions is already completed by ar_pci_destroy_all_data_rings()
	 * during group shutdown.
	 */
	if (driver->group_shutdown_supported &&
	    driver->client_send_queues.released &&
	    driver->client_rcv_queues.released)
		return 0;

	result = ar_pci_destroy_data_ring(dev,
					  (ar_client_queue_t *)queue_context,
					  ctrl_ring_timeout_ms_param);

	return IS_ERR(result) ? PTR_ERR(result) : ar_future_wait(result);
}

int ar_dev_handle_get_device_information(
	void *base_context,
	struct arfw_device_information_req *device_information)
{
	struct pci_dev *dev = (struct pci_dev *)base_context;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	AR_ASSERT(dev);
	AR_ASSERT(driver);
	AR_ASSERT(device_information);
	AR_ASSERT(driver->arp_info.valid);

	device_information->transport_header_size =
		sizeof(ar_pci_client_ring_entry_t);
	// inline data can be placed right after the headers
	device_information->inline_data_offset = offsetof(
		ar_pci_client_ring_entry_t, ap_src_data.data_msg.inline_data);

	device_information->require_contiguous_memory_for_queues = true;

	// The maximum number of supported rings
	device_information->send_ring_max = driver->ap_src_data_ring_max;
	device_information->rcv_ring_max = driver->ap_dst_data_ring_max;
	device_information->rcv_ring_pend_buff_count_max =
		driver->arp_info.rcv_ring_pend_buff_count_max;

	return 0;
}

int ar_dev_handle_receive_payload_pend(void *queue_context,
				       const struct arfw_payload_pend_req *req)
{
	int err;
	struct pci_dev *dev;
	ar_client_queue_t *client_queue = (ar_client_queue_t *)queue_context;

	AR_ASSERT(client_queue);
	AR_ASSERT(client_queue->driver);

	dev = client_queue->driver->dev;
	err = ar_pci_submit_receive_buffer(dev, queue_context, req);

	if (err)
		AR_LOG_PCI_QUEUE_ERR(client_queue, AR_LOG_PEND,
				     "Handle Pend Request failed [err: %d]",
				     err);
	else
		AR_LOG_PCI_QUEUE_DBG(client_queue, AR_LOG_PEND,
				     "Handle Pend Request OK");

	return err;
}

/*
 * Sometimes even if there are a few elements in the sgt table, the memory
 * space is sequential. For instance this is the case for the mako target:
 * q35 device + intel-iommu.
 *
 * If this is the case check it and return true, otherwise return false.
 */
static bool ar_dev_is_sgt_map_sequential(struct sg_table *sgt)
{
	struct scatterlist *sg;
	int i;
	dma_addr_t addr;

	addr = sg_dma_address(sgt->sgl);
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		if (addr != sg_dma_address(sg))
			return false;
		addr = sg_dma_address(sg) + sg_dma_len(sg);
	}

	return true;
}

static int ar_dev_default_dma_map(void *base_context, void *queue_context,
				  enum dma_data_direction direction,
				  struct sg_table *sgt, struct page **pages,
				  int num_pages, dma_addr_t *dma_addr)
{
	int err;
	struct pci_dev *dev = (struct pci_dev *)base_context;
	struct ar_client_queue *queue = (struct ar_client_queue *)queue_context;
	unsigned int nents;

	AR_ASSERT(dev);
	AR_ASSERT(dma_addr);

	err = sg_alloc_table_from_pages(sgt, pages, num_pages, 0,
					num_pages * PAGE_SIZE, GFP_KERNEL);
	if (err) {
		if (queue_context)
			AR_LOG_PCI_QUEUE_ERR(
				queue, AR_LOG_DMA_MAP_MEM,
				"Failed to create sg table, err %d", err);
		else
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_DMA_MAP_MEM,
					   "Failed to create sg table, err %d",
					   err);
		goto fail_allocate;
	}

	nents = dma_map_sg(&dev->dev, sgt->sgl, sgt->nents, direction);
	if (nents == 0) {
		if (queue_context)
			AR_LOG_PCI_QUEUE_ERR(
				queue, AR_LOG_DMA_MAP_MEM,
				"Failed to dma map sg list, err %d", err);
		else
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_DMA_MAP_MEM,
					   "Failed to dma map sg list, err %d",
					   err);
		err = -EFAULT;
		goto fail_free_sgt;
	}

	if (queue_context)
		AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_DMA_MAP_MEM,
				     "dma_map_sg args, len %lu nents %u",
				     num_pages * PAGE_SIZE, nents);
	else
		AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_DMA_MAP_MEM,
				   "dma_map_sg args, len %lu nents %u",
				   num_pages * PAGE_SIZE, nents);

	if ((nents > 1) && !ar_dev_is_sgt_map_sequential(sgt)) {
		if (queue_context)
			AR_LOG_PCI_QUEUE_ERR(
				queue, AR_LOG_DMA_MAP_MEM,
				"Unexpected nents > 1, nents = %u, non-sequential, mapping size %lu",
				nents, num_pages * PAGE_SIZE);
		else
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_DMA_MAP_MEM,
				"Unexpected nents > 1, nents = %u, non-sequential, mapping size %lu",
				nents, num_pages * PAGE_SIZE);
		err = -ENOSPC;
		goto fail_unmap;
	}

	*dma_addr = sg_dma_address(sgt->sgl);

	return 0;

fail_unmap:
	dma_unmap_sg(&dev->dev, sgt->sgl, sgt->nents, direction);
fail_free_sgt:
	sg_free_table(sgt);
fail_allocate:
	return err;
}

int ar_dev_handle_dma_map_quirk(arfw_client_queue_t arfw_queue,
				void *base_context, void *queue_context,
				enum dma_data_direction direction,
				struct sg_table *sgt, struct page **pages,
				int num_pages, dma_addr_t *dma_addr)
{
	int err;

	(void)arfw_queue;
	AR_ASSERT(base_context);
	AR_ASSERT(sgt);
	AR_ASSERT(pages);
	AR_ASSERT(dma_addr);
	err = ar_dev_default_dma_map(base_context, queue_context, direction,
				     sgt, pages, num_pages, dma_addr);

	return err;
}

static void ar_dev_default_dma_unmap(void *base_context, void *queue_context,
				     enum dma_data_direction direction,
				     struct sg_table *sgt)
{
	struct pci_dev *dev = (struct pci_dev *)base_context;

	AR_ASSERT(sgt);
	AR_ASSERT(dev);

	dma_unmap_sg(&dev->dev, sgt->sgl, sgt->nents, direction);
	sg_free_table(sgt);
}

void ar_dev_handle_dma_unmap_quirk(arfw_client_queue_t arfw_queue,
				   void *base_context, void *queue_context,
				   enum dma_data_direction direction,
				   struct sg_table *sgt, void *va, size_t size,
				   dma_addr_t dma_addr)
{
	(void)arfw_queue;
	(void)va;
	(void)size;
	(void)dma_addr;
	AR_ASSERT(base_context);
	AR_ASSERT(sgt);
	ar_dev_default_dma_unmap(base_context, queue_context, direction, sgt);
}

void ar_dev_handle_notify_queue_ready(void *queue_context)
{
	ar_client_queue_t *client_queue = (ar_client_queue_t *)queue_context;

	AR_ASSERT(client_queue);
	AR_ASSERT(client_queue->driver);

	ar_atomic_store(&client_queue->ready, 1, AR_MEMORY_ORDER_SEQ_CST);
	ar_client_rcv_ring_list_service(
		&client_queue->driver->client_rcv_queues);
}

int ar_dev_handle_queue_data_mmap(void *base_context,
				  struct arfw_queue_mem_region *queue_data,
				  struct vm_area_struct *vma)
{
	int ret;
	struct pci_dev *dev = (struct pci_dev *)base_context;

	ret = vm_iomap_memory(vma, virt_to_phys(queue_data->ptr),
			      queue_data->size);
	if (ret)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_MMAP,
			"Can't mmap queue data to the user space, error = %d",
			ret);

	return ret;
}

int ar_dev_handle_aperture_alloc(void *base_context, void *queue_context,
				 size_t size, void **vaddr,
				 dma_addr_t *dma_addr)
{
	ar_client_queue_t *client_queue = (ar_client_queue_t *)queue_context;
	ar_pci_driver_t *driver;
	int err = 0, nbits, start, offset;
	unsigned long flags;
	struct pci_dev *dev = (struct pci_dev *)base_context;
	struct ar_pci_device_aperture_region *aperture;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	aperture = &driver->aperture;

	AR_ASSERT(vaddr);
	AR_ASSERT(dma_addr);
	AR_ASSERT(client_queue);

	if (aperture->addr == NULL) {
		AR_LOG_PCI_QUEUE_ERR(client_queue, AR_LOG_APERTURE_ALLOC,
				     "Aperture not supported");
		return -EOPNOTSUPP;
	}

	if (size % ARFW_APERTURE_ALLOC_CHUNK_SIZE) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_APERTURE_ALLOC,
			"Invalid aperture alloc alignment. Align to %u bytes [size: %zu]",
			ARFW_APERTURE_ALLOC_CHUNK_SIZE, size);
		return -EINVAL;
	}

	AR_ASSERT(aperture->locked_allocs.bitmap);

	// calculate bits needed, quick sanity check on size
	nbits = size / ARFW_APERTURE_ALLOC_CHUNK_SIZE;
	if (nbits > aperture->bits) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_APERTURE_ALLOC,
			"Aperture alloc size too large. [size: %zu, aperture size: %zu]",
			size, driver->aperture.size);
		return -ENOMEM;
	}

	// use bitmap apis to allocate a region
	spin_lock_irqsave(&aperture->locked_allocs.lock, flags);
	start = bitmap_find_next_zero_area(aperture->locked_allocs.bitmap,
					   aperture->bits, 0, nbits, 0);
	if (start != aperture->bits)
		bitmap_set(aperture->locked_allocs.bitmap, start, nbits);
	else
		err = -ENOMEM;
	spin_unlock_irqrestore(&aperture->locked_allocs.lock, flags);

	if (err) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_APERTURE_ALLOC,
			"Failed to allocate aperture region [err %d]", err);
		return err;
	}

	offset = start * ARFW_APERTURE_ALLOC_CHUNK_SIZE;

	*vaddr = VOID_PTR_OFFSET(aperture->addr, offset);
	*dma_addr = aperture->dma_addr + offset;

	AR_LOG_PCI_QUEUE_DBG(client_queue, AR_LOG_APERTURE_ALLOC,
			     "Allocated aperture buffer [offset %d]", start);

	memset(*vaddr, 0, size);

	return 0;
}

int ar_dev_handle_aperture_mmap(void *base_context, void *queue_context,
				void *va, size_t size,
				struct vm_area_struct *vma)
{
	ar_pci_driver_t *driver;
	int ret;
	struct pci_dev *dev = (struct pci_dev *)base_context;
	ar_client_queue_t *client_queue = (ar_client_queue_t *)queue_context;
	unsigned long offset;
	struct ar_pci_device_aperture_region *aperture;

	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	aperture = &driver->aperture;
	offset = (uintptr_t)va - (uintptr_t)aperture->addr;
	if (offset % PAGE_SIZE) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_MMAP,
			"The aperture offset should be aligned to page size, offset 0x%lx",
			offset);
		return -EINVAL;
	}
	/*
	 * According to API dma_mmap_coherent should use arguments returned by
	 * dma_alloc_coherent. The vm_pgoff field is used for the offset to
	 * mmap proper pages.
	 */
	vma->vm_pgoff = offset >> PAGE_SHIFT;
	ret = dma_mmap_coherent(&dev->dev, vma, aperture->addr,
				aperture->dma_addr, aperture->size);
	if (ret)
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_MMAP,
			"Can't mmap queue data to the user space, error = %d",
			ret);

	return ret;
}

void ar_dev_handle_aperture_free(void *base_context, void *queue_context,
				 void *vaddr, size_t size)
{
	ar_pci_driver_t *driver;
	struct pci_dev *dev = (struct pci_dev *)base_context;
	ar_client_queue_t *client_queue = (ar_client_queue_t *)queue_context;
	struct ar_pci_device_aperture_region *aperture;
	int offset, start, nbits;
	unsigned long flags;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	/**
	 * README: There be footguns here.
	 * Note that the queue_context could be null, since aperture buffers are capable of
	 * outliving the queue. Since aperture buffers are allocated from the device, this
	 * is fine, but we need to account for the case where client_queue is NULL vs
	 * an actual queue in this function. We really only use the queue for logging, so
	 * consider updating this in the future. (T209630342)
	 */

	aperture = &driver->aperture;

	offset = vaddr - aperture->addr;
	start = offset / ARFW_APERTURE_ALLOC_CHUNK_SIZE;
	nbits = size / ARFW_APERTURE_ALLOC_CHUNK_SIZE;

	if (start < 0 || start + nbits > aperture->bits) {
		if (client_queue) {
			AR_LOG_PCI_QUEUE_ERR(
				client_queue, AR_LOG_APERTURE_FREE,
				"Out of bounds request to aperture free, offset %d size %zu",
				offset, size);
		} else {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_APERTURE_FREE,
				"Out of bounds request to aperture free, offset %d size %zu",
				offset, size);
		}
	}

	// clear the region
	spin_lock_irqsave(&aperture->locked_allocs.lock, flags);
	bitmap_clear(aperture->locked_allocs.bitmap, start, nbits);
	spin_unlock_irqrestore(&aperture->locked_allocs.lock, flags);

	if (client_queue) {
		AR_LOG_PCI_QUEUE_DBG(client_queue, AR_LOG_APERTURE_FREE,
				     "Free aperture buffer [offset %d]", start);
	} else {
		AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_APERTURE_FREE,
				   "Free aperture buffer [offset %d]", start);
	}
}
