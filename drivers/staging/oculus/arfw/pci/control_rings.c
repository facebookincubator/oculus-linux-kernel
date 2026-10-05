// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file control_rings.c
 *
 * @brief Control ring implementation for AR accelorator
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include "control_rings.h"
#include "ar_pci_int.h"

#include <arfw_log.h>

static int allocate_ring_memory(struct pci_dev *dev, size_t produce_size,
				ar_pci_control_ring_context_t *ring)
{
	ring->va = dma_alloc_coherent(&dev->dev, produce_size,
				      &ring->dma_handle, GFP_KERNEL);
	if (ring->va == NULL) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
				   "%s: Failed to alloc DMA region", __func__);
		return -ENOMEM;
	}

	ring->va_size = produce_size;

	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
		"Allocated ring memory [vaddr: %p, dma: 0x%llx, size: 0x%zx]",
		ring->va, ring->dma_handle, ring->va_size);

	return 0;
}

static int
ar_pci_control_ring_create(struct pci_dev *dev, size_t element_size,
			   uint16_t element_count, bool receive_ring,
			   ar_pci_control_ring_context_t **result_ring)
{
	int ret = 0;
	size_t produce_size = 0;
	ar_pci_control_ring_context_t *ring;

	ring = devm_kzalloc(&dev->dev, sizeof(ar_pci_control_ring_context_t),
			    GFP_KERNEL);
	if (ring == NULL) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
				   "Failed to alloc ring structure");
		return -ENOMEM;
	}

	ring->element_size = element_size;
	ring->element_count = element_count;
	ring->receive_ring = receive_ring;
	ring->local_rd_index = 0;
	ring->local_wr_index = 0;
	mutex_init(&ring->lock);

	// Compute the total size of the ring buffer
	// TODO(dpredmore): This probably isn't the correct size
	produce_size = element_size * element_count;

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			    "Control queue [produce_size: %lu]", produce_size);

	ret = allocate_ring_memory(dev, produce_size, ring);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
				   "Failed to alloc ring memory [err: %d]",
				   ret);
		devm_kfree(&dev->dev, ring);
		return ret;
	}

	*result_ring = ring;
	return 0;
}

static void ar_pci_control_ring_destroy(struct pci_dev *dev,
					ar_pci_control_ring_context_t *ring)
{
	dma_free_coherent(&dev->dev, ring->va_size, ring->va, ring->dma_handle);
	devm_kfree(&dev->dev, ring);

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_DESTORY_CTRL_QUEUE,
			    "Control Queue destroy OK");
}

int ar_pci_control_rings_create(struct pci_dev *dev)
{
	int ret = 0;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	ret = ar_pci_control_ring_create(dev, PCIE_AP_SRC_CTRL_RING_SIZE,
					 AR_PCI_AP_SRC_RING_DEPTH, false,
					 &driver->ap_src_control_ring);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			"Failed to create ap_src_control_ring [err: %d]", ret);
		return ret;
	}
	AR_LOG_PCI_DEV_INFO(
		&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
		"Create AP SRC Ring OK [ring_depth: %d, ring_size: %zu]",
		AR_PCI_AP_SRC_RING_DEPTH,
		(size_t)(PCIE_AP_SRC_CTRL_RING_SIZE *
			 AR_PCI_AP_SRC_RING_DEPTH));

	ret = ar_pci_control_ring_create(dev, PCIE_AP_DST_CTRL_RING_SIZE,
					 AR_PCI_AP_DST_RING_DEPTH, true,
					 &driver->ap_dst_control_ring);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			"Failed to create ap_dst_control_ring [err: %d]", ret);
		goto error_dst_control;
	}
	AR_LOG_PCI_DEV_INFO(
		&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
		"Create AP DST Ring OK [ring_depth: %d, ring_size: %zu]",
		AR_PCI_AP_DST_RING_DEPTH,
		(size_t)(PCIE_AP_DST_CTRL_RING_SIZE *
			 AR_PCI_AP_DST_RING_DEPTH));

	ret = ar_pci_control_ring_create(dev, sizeof(pcie_ap_src_buf_item_t),
					 AR_PCI_AP_SRC_BUFFER_RING_DEPTH, false,
					 &driver->ap_src_buffer_control_ring);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			"Failed to create ap_src_buffer_control_ring [err: %d]",
			ret);
		goto error_src_buffer_control;
	}
	AR_LOG_PCI_DEV_INFO(
		&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
		"Create AP SRC Buffer CTRL OK [ring_depth: %d, ring_size: %zu]",
		AR_PCI_AP_SRC_BUFFER_RING_DEPTH,
		sizeof(pcie_ap_src_buf_item_t) *
			AR_PCI_AP_SRC_BUFFER_RING_DEPTH);

	return 0;

error_src_buffer_control:
	ar_pci_control_ring_destroy(dev, driver->ap_dst_control_ring);
error_dst_control:
	ar_pci_control_ring_destroy(dev, driver->ap_src_control_ring);
	return ret;
}

void ar_pci_control_rings_destroy(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	ar_pci_control_ring_destroy(dev, driver->ap_dst_control_ring);
	ar_pci_control_ring_destroy(dev, driver->ap_src_control_ring);
	ar_pci_control_ring_destroy(dev, driver->ap_src_buffer_control_ring);

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_DESTORY_CTRL_QUEUE,
			    "Destroy Control Queues");
}

static inline uint32_t
ap_control_ring_get_next_rd_index(ar_pci_control_ring_context_t *ring)
{
	uint32_t index = ring->local_rd_index + 1;

	return index % ring->element_count;
}

static inline uint32_t
ap_control_ring_get_next_wr_index(ar_pci_control_ring_context_t *ring)
{
	uint32_t index = ring->local_wr_index + 1;

	return index % ring->element_count;
}

static inline int ap_control_ring_get_count(ar_pci_control_ring_context_t *ring)
{
	uint32_t rd_index = ring->local_rd_index;
	uint32_t wr_index = ring->local_wr_index;

	if (rd_index <= wr_index)
		return wr_index - rd_index;
	else
		return ring->element_count + wr_index - rd_index;
}

int ap_src_ring_produce(ar_pci_control_ring_context_t *ring, uint32_t *index,
			void *data, size_t data_size)
{
	uintptr_t start_addr;

	if (ring->receive_ring) {
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL,
			       "Can't send on a receive ring");
		return -EINVAL;
	}
	if (data_size > ring->element_size) {
		AR_LOG_PCI_ERR(
			AR_LOG_WRITE_CTRL,
			"Incorrect size [data size: %zu, element_size %zu]",
			data_size, ring->element_size);
		return -EINVAL;
	}

	if (ap_control_ring_get_count(ring) >= ring->element_count)
		return -ENOMEM;

	start_addr = ((uintptr_t)ring->va) +
		     ring->element_size * ring->local_wr_index;
	memcpy((void *)start_addr, data, data_size);

	*index = ring->local_wr_index;

	ring->local_wr_index = ap_control_ring_get_next_wr_index(ring);
	return 0;
}

int ap_src_ring_mark_consumed(ar_pci_control_ring_context_t *ring,
			      uint32_t index)
{
	if (ring->receive_ring) {
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL,
			       "Can't mark consumed on a receive ring");
		return -EINVAL;
	}
	if (index != ring->local_rd_index) {
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL,
			       "Incorrect index [index: %u, local_rd_index %u]",
			       index, ring->local_rd_index);
		return -EINVAL;
	}

	if (ap_control_ring_get_count(ring) == 0) {
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL, "AP Control ring empty");
		return -ENOMEM;
	}

	ring->local_rd_index = ap_control_ring_get_next_rd_index(ring);

	return 0;
}

int ap_dst_ring_reserve_for_produce(ar_pci_control_ring_context_t *ring,
				    uint32_t *index)
{
	if (!ring->receive_ring) {
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL,
			       "Can't reserve on a send ring");
		return -EINVAL;
	}

	*index = ring->local_wr_index;
	return 0;
}

int ap_dst_ring_mark_produced(ar_pci_control_ring_context_t *ring,
			      uint32_t index)
{
	if (!ring->receive_ring) {
		AR_LOG_PCI_ERR(AR_LOG_READ_CTRL,
			       "Can't mark produced on a send ring");
		return -EINVAL;
	}
	if (index != ring->local_wr_index) {
		AR_LOG_PCI_ERR(AR_LOG_READ_CTRL,
			       "Incorrect index [index: %u, local_wr_index %u]",
			       index, ring->local_wr_index);
		return -EINVAL;
	}

	if (ap_control_ring_get_count(ring) >= ring->element_count) {
		AR_LOG_PCI_ERR(
			AR_LOG_READ_CTRL,
			"Incorrect element count [count: %u, local_count %u]",
			ap_control_ring_get_count(ring), ring->element_count);
		return -ENOMEM;
	}

	ring->local_wr_index = ap_control_ring_get_next_wr_index(ring);

	return 0;
}

int ap_dst_ring_consume(ar_pci_control_ring_context_t *ring, uint32_t *index,
			void *data, size_t data_size)
{
	uintptr_t start_addr;

	if (!ring->receive_ring) {
		AR_LOG_PCI_ERR(AR_LOG_READ_CTRL,
			       "Can't consume on a send ring");
		return -EINVAL;
	}
	if (ring->element_size != data_size) {
		AR_LOG_PCI_ERR(
			AR_LOG_READ_CTRL,
			"Incorrect size [data size: %zu, element_size %zu]",
			data_size, ring->element_size);
		return -EINVAL;
	}

	if (ap_control_ring_get_count(ring) == 0) {
		AR_LOG_PCI_ERR(AR_LOG_READ_CTRL, "AP Control ring empty");
		return -ENOMEM;
	}

	start_addr = ((uintptr_t)ring->va) +
		     ring->element_size * ring->local_rd_index;
	memcpy(data, (void *)start_addr, data_size);

	ring->local_rd_index = ap_control_ring_get_next_rd_index(ring);

	*index = ring->local_rd_index;

	return 0;
}
