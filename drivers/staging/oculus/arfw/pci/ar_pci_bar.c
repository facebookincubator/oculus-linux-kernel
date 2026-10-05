// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file ar_pci_bar.c
 *
 * @brief PCI BAR implementation for AR accelorator
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include "ar_pci_bar.h"

#include <linux/moduleparam.h>
#include <linux/pm_runtime.h>

#include <arfw_log.h>
#include <arfw_mem_util.h>

#include "ar_future.h"
#include "arfw_device.h"
#include "control_rings.h"
#include "data_ring.h"
#include "ar_pci_int.h"
#include "arfw_pci.h"

#define USEC_TO_NSEC(x) ((x)*1000)

static bool ar_pci_patch_protocol_version(struct pci_dev *dev);
static int ar_pci_bar_get_arp_update_region(struct pci_dev *dev);
static int ar_pci_bar_set_ap_update_region(struct pci_dev *dev);
static int ar_pci_bar_handshake_bar_layout(struct pci_dev *dev);
static int ar_pci_bar_handshake_ram_layout(struct pci_dev *dev);
static void ar_pci_bar_release_bar_layout(struct pci_dev *dev);
static void ar_pci_bar_release_ram_layout(struct pci_dev *dev);
static void ar_pci_bar_init_doorbell_bar_layout(struct pci_dev *dev);
static void ar_pci_bar_init_doorbell_ram_layout(struct pci_dev *dev);

// Request and map a bar
// Return size of the bar or negative value on error
static ssize_t ar_pci_bar_map_request(struct pci_dev *dev, int bar, void **data)
{
	int ret = 0;
	phys_addr_t bar_addr;
	size_t bar_size;
	void *bar_data;

	// Request ownership of the BAR
	ret = pci_request_region(dev, bar, AR_DRIVER_NAME);
	if (ret)
		return ret;

	// Get the physical address and size
	bar_addr = pci_resource_start(dev, bar);
	bar_size = pci_resource_len(dev, bar);

	// Map the BAR to a virtual address
	bar_data = ioremap(bar_addr, bar_size);
	if (bar_data == NULL) {
		pci_release_region(dev, bar);
		return -ENOMEM;
	}

	*data = bar_data;
	return bar_size;
}

int ar_pci_bar_map(struct pci_dev *dev)
{
	ssize_t size = 0;
	void *bar_data;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	size = ar_pci_bar_map_request(dev, driver->mmio_memory_bar_region,
				      &bar_data);
	if (size < 0) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to map memory region [size: 0x%zx]",
				   size);
		return size;
	}

	driver->mmio_memory_bar = bar_data;
	driver->mmio_memory_bar_size = size;

	if (driver->mmio_interrupt_bar_region >= 0) {
		size = ar_pci_bar_map_request(
			dev, driver->mmio_interrupt_bar_region, &bar_data);
		if (size < 0) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Failed to map interrupt region [size: 0x%zx]",
				size);
			goto error_bar_irq_map;
		}

		driver->mmio_interrupt_bar = bar_data;
		driver->mmio_interrupt_bar_size = size;
	}

	return 0;

error_bar_irq_map:
	iounmap(driver->mmio_memory_bar);
	driver->mmio_memory_bar = NULL;
	pci_release_region(dev, driver->mmio_memory_bar_region);
	return size;
}

int ar_pci_bar_unmap(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	if (driver->mmio_memory_bar != NULL) {
		iounmap(driver->mmio_memory_bar);
		driver->mmio_memory_bar = NULL;
		pci_release_region(dev, driver->mmio_memory_bar_region);
	} else {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_SHUTDOWN,
				   "mmio_memory_bar is NULL");
	}

	if (driver->mmio_interrupt_bar != NULL) {
		iounmap(driver->mmio_interrupt_bar);
		driver->mmio_interrupt_bar = NULL;
		pci_release_region(dev, driver->mmio_interrupt_bar_region);
	} else {
		AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_SHUTDOWN,
				   "mmio_interrupt_bar is NULL");
	}

	return 0;
}

static void
ar_pci_bar_handshake_initiate_data_rings_bar_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout = driver->deprecated_bar_layout;
	size_t index = 0;

	AR_ASSERT(bar_layout);
	for (index = 0;
	     index < bar_layout->bar_memory_area.ap_src_data_ring_max;
	     index++) {
		driver->send_data_rings[index].in_use = false;
		driver->send_data_rings[index].cached_ring_index = 0;
		driver->send_data_rings[index].rd_index_reg =
			bar_layout->bar_memory_area.arp_update_addrs
				.ap_src_ring_rd_index_addr +
			index * sizeof(uint16_t);

		driver->send_data_rings[index].wr_index_reg =
			bar_layout->bar_memory_area.ap_update_addrs
				.ap_src_ring_wr_index_addr +
			index * sizeof(uint16_t);

		driver->send_data_rings[index].cached_arp_index = &(
			(uint16_t *)
				bar_layout->arp_update_rd_index_cached)[index];

		driver->send_data_rings[index].cached_ap_index =
			&((uint16_t *)bar_layout->ap_wr_index_cached)[index];
	}

	for (index = 0;
	     index < bar_layout->bar_memory_area.ap_dst_data_ring_max;
	     index++) {
		driver->rcv_data_rings[index].in_use = false;
		driver->rcv_data_rings[index].cached_ring_index = 0;
		driver->rcv_data_rings[index].rd_index_reg =
			bar_layout->bar_memory_area.ap_update_addrs
				.ap_dst_ring_rd_index_addr +
			index * sizeof(uint16_t);

		driver->rcv_data_rings[index].wr_index_reg =
			bar_layout->bar_memory_area.arp_update_addrs
				.ap_dst_ring_wr_index_addr +
			index * sizeof(uint16_t);

		driver->rcv_data_rings[index].cached_arp_index = &(
			(uint16_t *)
				bar_layout->arp_update_wr_index_cached)[index];

		driver->rcv_data_rings[index].cached_ap_index =
			&((uint16_t *)bar_layout->ap_rd_index_cached)[index];
	}
}

static void
ar_pci_bar_handshake_initiate_data_rings_ram_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	size_t offset;
	size_t offset_ap;
	size_t offset_cp;
	size_t i;

	// Write the device addresses of AP and CP blocks.
	arfw_mem_util_iomem_write_block(driver->mmio_memory_bar,
					offsetof(pcie_bar_data_t,
						 ap_update_block_addr),
					&driver->ap_update_block_dma,
					sizeof(driver->ap_update_block_dma));
	arfw_mem_util_iomem_write_block(driver->mmio_memory_bar,
					offsetof(pcie_bar_data_t,
						 cp_update_block_addr),
					&driver->cp_update_block_dma,
					sizeof(driver->cp_update_block_dma));

	offset_ap = sizeof(ap_update_block_t);
	offset_cp = sizeof(cp_update_block_t);
	for (i = 0; i < driver->bar_data.data_ring_max; i++) {
		driver->send_data_rings[i].in_use = false;
		driver->send_data_rings[i].cached_ring_index = 0;
		driver->rcv_data_rings[i].in_use = false;
		driver->rcv_data_rings[i].cached_ring_index = 0;

		// AP update block
		offset = offset_ap + offsetof(ap_data_ring_index_entry_t,
					      ap_src_data_wr_index);
		driver->send_data_rings[i].wr_index_reg = offset;
		driver->send_data_rings[i].cached_ap_index =
			driver->ap_update_block_addr + offset;
		offset = offset_ap + offsetof(ap_data_ring_index_entry_t,
					      ap_dst_data_rd_index);
		driver->rcv_data_rings[i].rd_index_reg = offset;
		driver->rcv_data_rings[i].cached_ap_index =
			driver->ap_update_block_addr + offset;

		// CP update block
		offset = offset_cp + offsetof(cp_data_ring_index_entry_t,
					      ap_src_data_rd_index);
		driver->send_data_rings[i].rd_index_reg = offset;
		driver->send_data_rings[i].cached_arp_index =
			driver->cp_update_block_addr + offset;
		offset = offset_cp + offsetof(cp_data_ring_index_entry_t,
					      ap_dst_data_wr_index);
		driver->rcv_data_rings[i].wr_index_reg = offset;
		driver->rcv_data_rings[i].cached_arp_index =
			driver->cp_update_block_addr + offset;

		offset_ap += sizeof(ap_data_ring_index_entry_t);
		offset_cp += sizeof(cp_data_ring_index_entry_t);
	}
}

static int ap_pci_bar_handshake_populate_control_rings(struct pci_dev *dev)
{
	uint64_t ap_src_control_ring_pa = 0;
	uint64_t ap_dst_control_ring_pa = 0;
	uint64_t ap_src_buffer_control_ring_pa = 0;

	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	ap_src_control_ring_pa = driver->ap_src_control_ring->dma_handle;
	ap_dst_control_ring_pa = driver->ap_dst_control_ring->dma_handle;
	ap_src_buffer_control_ring_pa =
		driver->ap_src_buffer_control_ring->dma_handle;

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			   "Control Queues addresses:");
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			   "        AP->FW: %llx", ap_src_control_ring_pa);
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			   "        FW->AP: %llx", ap_dst_control_ring_pa);
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			   "        BF_CTL: %llx",
			   ap_src_buffer_control_ring_pa);
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_CREATE_CTRL_QUEUE,
			   "        BL_STS: %x",
			   driver->deprecated_bar_layout->bar_memory_area
				   .status_block_addr);

	// Write the physical addresses of each control ring to bar space
	arfw_mem_util_iomem_write_64(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver, ap_updates.ap_src_ctrl_addr),
		ap_src_control_ring_pa);

	arfw_mem_util_iomem_write_64(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver, ap_updates.ap_dst_ctrl_addr),
		ap_dst_control_ring_pa);

	arfw_mem_util_iomem_write_64(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver, ap_updates.ap_src_buf_addr),
		ap_src_buffer_control_ring_pa);

	// Set the head room
	arfw_mem_util_iomem_write_8(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 ap_updates.ap_src_ctrl_head_room),
		0);
	arfw_mem_util_iomem_write_8(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 ap_updates.ap_dst_ctrl_head_room),
		0);
	arfw_mem_util_iomem_write_8(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 ap_updates.ap_src_buf_head_room),
		0);

	// Set the tail room
	arfw_mem_util_iomem_write_8(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 ap_updates.ap_src_ctrl_tail_room),
		0);
	arfw_mem_util_iomem_write_8(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 ap_updates.ap_dst_ctrl_tail_room),
		0);
	arfw_mem_util_iomem_write_8(
		driver->mmio_memory_bar,
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 ap_updates.ap_src_buf_tail_room),
		0);

	// Stash away the read/write register offsets for the rings
	driver->ap_src_control_ring->rd_index_reg = AR_BAR_STATUS_BLOCK_ADDR(
		driver, arp_updates.ap_src_ctrl_rd_index);
	driver->ap_src_control_ring->wr_index_reg = AR_BAR_STATUS_BLOCK_ADDR(
		driver, ap_updates.ap_src_ctrl_wr_index);

	driver->ap_dst_control_ring->rd_index_reg = AR_BAR_STATUS_BLOCK_ADDR(
		driver, ap_updates.ap_dst_ctrl_rd_index);
	driver->ap_dst_control_ring->wr_index_reg = AR_BAR_STATUS_BLOCK_ADDR(
		driver, arp_updates.ap_dst_ctrl_wr_index);

	driver->ap_src_buffer_control_ring->rd_index_reg =
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 arp_updates.ap_src_buf_rd_index);
	driver->ap_src_buffer_control_ring->wr_index_reg =
		AR_BAR_STATUS_BLOCK_ADDR(driver,
					 ap_updates.ap_src_buf_wr_index);

	return 0;
}

static int
ap_pci_bar_handshake_populate_control_rings_ram_layout(struct pci_dev *dev)
{
	uint64_t ap_src_control_ring_dma = 0;
	uint64_t ap_dst_control_ring_dma = 0;
	uint64_t ap_src_buffer_control_ring_dma = 0;

	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	ap_src_control_ring_dma = driver->ap_src_control_ring->dma_handle;
	ap_dst_control_ring_dma = driver->ap_dst_control_ring->dma_handle;
	ap_src_buffer_control_ring_dma =
		driver->ap_src_buffer_control_ring->dma_handle;

	// Write the physical addresses of each control ring to bar space
	arfw_mem_util_iomem_write_block(
		driver->mmio_memory_bar,
		offsetof(pcie_bar_data_t, ap_src_ctrl_addr),
		&ap_src_control_ring_dma, sizeof(ap_src_control_ring_dma));
	arfw_mem_util_iomem_write_block(
		driver->mmio_memory_bar,
		offsetof(pcie_bar_data_t, ap_dst_ctrl_addr),
		&ap_dst_control_ring_dma, sizeof(ap_dst_control_ring_dma));
	arfw_mem_util_iomem_write_block(driver->mmio_memory_bar,
					offsetof(pcie_bar_data_t,
						 ap_src_buf_addr),
					&ap_src_buffer_control_ring_dma,
					sizeof(ap_src_buffer_control_ring_dma));

	// Stash away the read/write register offsets for the rings
	driver->ap_src_control_ring->wr_index_reg =
		offsetof(ap_update_block_t, ap_src_ctrl_wr_index);
	driver->ap_dst_control_ring->rd_index_reg =
		offsetof(ap_update_block_t, ap_dst_ctrl_rd_index);
	driver->ap_src_buffer_control_ring->wr_index_reg =
		offsetof(ap_update_block_t, ap_src_buf_wr_index);

	driver->ap_src_control_ring->rd_index_reg =
		offsetof(cp_update_block_t, ap_src_ctrl_rd_index);
	driver->ap_dst_control_ring->wr_index_reg =
		offsetof(cp_update_block_t, ap_dst_ctrl_wr_index);
	driver->ap_src_buffer_control_ring->rd_index_reg =
		offsetof(cp_update_block_t, ap_src_buf_rd_index);

	return 0;
}

// Helper to populate the AP protocol version to the BAR
static void ar_pci_bar_handshake_populate_ap_version(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	arfw_mem_util_iomem_write_32(driver->mmio_memory_bar,
				     offsetof(pcie_bar_memory_area_t,
					      ap_protocol_version),
				     driver->ap_ver.as_uint32);
}

static void
ar_pci_bar_handshake_populate_ap_version_ram_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	arfw_mem_util_iomem_write_32(driver->mmio_memory_bar,
				     offsetof(pcie_bar_header_t,
					      ap_protocol_version),
				     driver->ap_ver.as_uint32);
}

// Helper to allocate data ring entries based on Avo max
static bool
ar_pci_bar_handshake_allocate_data_rings_bar_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout = driver->deprecated_bar_layout;

	AR_ASSERT(bar_layout);
	// deallocated when driver is destroyed
	driver->send_data_rings = devm_kzalloc(
		&dev->dev,
		sizeof(ar_pci_bar_data_ring_t) *
			bar_layout->bar_memory_area.ap_src_data_ring_max,
		GFP_KERNEL);

	if (!driver->send_data_rings) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate send_data_rings");
		return false;
	}

	driver->rcv_data_rings = devm_kzalloc(
		&dev->dev,
		sizeof(ar_pci_bar_data_ring_t) *
			bar_layout->bar_memory_area.ap_dst_data_ring_max,
		GFP_KERNEL);

	if (!driver->rcv_data_rings) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate rcv_data_rings");
		goto failed_rcv_data_rings;
	}

	bar_layout->arp_update_wr_index_cached = devm_kzalloc(
		&dev->dev,
		sizeof(uint16_t) *
			bar_layout->bar_memory_area.ap_dst_data_ring_max,
		GFP_KERNEL);
	if (!bar_layout->arp_update_wr_index_cached) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to allocate arp_update_wr_index_cached");
		goto failed_arp_update_wr_index;
	}

	bar_layout->arp_update_rd_index_cached = devm_kzalloc(
		&dev->dev,
		sizeof(uint16_t) *
			bar_layout->bar_memory_area.ap_src_data_ring_max,
		GFP_KERNEL);
	if (!bar_layout->arp_update_rd_index_cached) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to allocate arp_update_rd_index_cached");
		goto failed_arp_update_rd_index;
	}

	bar_layout->ap_wr_index_cached = devm_kzalloc(
		&dev->dev,
		sizeof(uint16_t) *
			bar_layout->bar_memory_area.ap_src_data_ring_max,
		GFP_KERNEL);
	if (!bar_layout->ap_wr_index_cached) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate ap_wr_index_cached");
		goto failed_ap_wr_index_cached;
	}

	bar_layout->ap_rd_index_cached = devm_kzalloc(
		&dev->dev,
		sizeof(uint16_t) *
			bar_layout->bar_memory_area.ap_dst_data_ring_max,
		GFP_KERNEL);
	if (!bar_layout->ap_rd_index_cached) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate ap_rd_index_cached");
		goto failed_ap_rd_index_cached;
	}

	// Populate the cached values
	ar_pci_bar_get_arp_update_region(dev);
	return true;

failed_ap_rd_index_cached:
	devm_kfree(&dev->dev, bar_layout->ap_wr_index_cached);
	bar_layout->ap_wr_index_cached = NULL;
failed_ap_wr_index_cached:
	devm_kfree(&dev->dev, bar_layout->arp_update_rd_index_cached);
	bar_layout->arp_update_rd_index_cached = NULL;
failed_arp_update_rd_index:
	devm_kfree(&dev->dev, bar_layout->arp_update_wr_index_cached);
	bar_layout->arp_update_wr_index_cached = NULL;
failed_arp_update_wr_index:
	devm_kfree(&dev->dev, driver->rcv_data_rings);
	driver->rcv_data_rings = NULL;
failed_rcv_data_rings:
	devm_kfree(&dev->dev, driver->send_data_rings);
	driver->send_data_rings = NULL;
	return false;
}

static bool
ar_pci_bar_handshake_allocate_data_rings_ram_layout(struct pci_dev *dev)
{
	size_t size;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	size = sizeof(ar_pci_bar_data_ring_t) * driver->bar_data.data_ring_max;
	driver->send_data_rings = devm_kzalloc(&dev->dev, size, GFP_KERNEL);
	if (!driver->send_data_rings) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate send_data_rings");
		return false;
	}
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"Allocated send data rings array [vaddr: %p, ring_depth: %d, ring_size: %zu]",
		driver->send_data_rings, driver->bar_data.data_ring_max, size);

	size = sizeof(ar_pci_bar_data_ring_t) * driver->bar_data.data_ring_max;
	driver->rcv_data_rings = devm_kzalloc(&dev->dev, size, GFP_KERNEL);
	if (!driver->rcv_data_rings) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate rcv_data_rings");
		goto failed_rcv_data_rings;
	}
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"Allocated rcv data rings array [vaddr: %p, ring_depth: %d, ring_size: %zu]",
		driver->rcv_data_rings, driver->bar_data.data_ring_max, size);

	driver->ap_update_block_size = sizeof(ap_update_block_t) +
				       sizeof(ap_data_ring_index_entry_t) *
					       driver->bar_data.data_ring_max;
	driver->ap_update_block_addr =
		dma_alloc_coherent(&dev->dev, driver->ap_update_block_size,
				   &driver->ap_update_block_dma, GFP_KERNEL);
	if (!driver->ap_update_block_addr) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate AP update block");
		goto failed_ap_update_block;
	}
	memset(driver->ap_update_block_addr, 0, driver->ap_update_block_size);
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"Allocated AP update block [vaddr: %p, dma: 0x%llx, size: 0x%zx]",
		driver->ap_update_block_addr, driver->ap_update_block_dma,
		driver->ap_update_block_size);

	driver->cp_update_block_size = sizeof(cp_update_block_t) +
				       sizeof(cp_data_ring_index_entry_t) *
					       driver->bar_data.data_ring_max;
	driver->cp_update_block_addr =
		dma_alloc_coherent(&dev->dev, driver->cp_update_block_size,
				   &driver->cp_update_block_dma, GFP_KERNEL);
	if (!driver->cp_update_block_addr) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate CP update block");
		goto failed_cp_update_block;
	}
	memset(driver->cp_update_block_addr, 0, driver->cp_update_block_size);
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"Allocated CP update block [vaddr: %p, dma: 0x%llx, size: 0x%zx]",
		driver->cp_update_block_addr, driver->cp_update_block_dma,
		driver->cp_update_block_size);

	return true;

failed_cp_update_block:
	dma_free_coherent(&dev->dev, driver->ap_update_block_size,
			  driver->ap_update_block_addr,
			  driver->ap_update_block_dma);
	driver->ap_update_block_addr = NULL;
	driver->ap_update_block_dma = 0;
	driver->ap_update_block_size = 0;
failed_ap_update_block:
	devm_kfree(&dev->dev, driver->rcv_data_rings);
	driver->rcv_data_rings = NULL;
failed_rcv_data_rings:
	devm_kfree(&dev->dev, driver->send_data_rings);
	driver->send_data_rings = NULL;
	return false;
}

// quick helper to check overlap of 2 ranges
static bool ranges_overlap(uint32_t a_base, size_t a_size, uint32_t b_base,
			   size_t b_size)
{
	uint32_t a1 = a_base;
	uint32_t a2 = a1 + a_size - 1;
	uint32_t b1 = b_base;
	uint32_t b2 = b1 + b_size - 1;

	return max(a1, b1) <= min(a2, b2);
}

// Helper to validate the location of ring indexes in shared mem
static bool ar_pci_bar_handshake_validate_ring_offsets(struct pci_dev *dev)
{
	int i;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout = driver->deprecated_bar_layout;
	uint16_t send_ring_count =
		bar_layout->bar_memory_area.ap_src_data_ring_max;
	uint16_t rcv_ring_count =
		bar_layout->bar_memory_area.ap_dst_data_ring_max;

	typedef struct {
		uint32_t offset;
		size_t element_size;
		uint16_t count;
		const char *name;
	} range_check_t;

	// All the dynamic offsets for ring indexes
	range_check_t ranges[] = {
		{ bar_layout->bar_memory_area.arp_update_addrs
			  .ap_src_ring_rd_index_addr,
		  sizeof(uint16_t), send_ring_count,
		  "ap_src_ring_rd_index_addr" },
		{ bar_layout->bar_memory_area.arp_update_addrs
			  .ap_dst_ring_wr_index_addr,
		  sizeof(uint16_t), rcv_ring_count,
		  "ap_dst_ring_wr_index_addr" },
		{ bar_layout->bar_memory_area.arp_update_addrs
			  .ap_src_ring_status_addr,
		  sizeof(uint16_t), send_ring_count,
		  "ap_src_ring_status_addr" },
		{ bar_layout->bar_memory_area.arp_update_addrs
			  .ap_dst_ring_status_addr,
		  sizeof(uint16_t), rcv_ring_count, "ap_dst_ring_status_addr" },
		{ bar_layout->bar_memory_area.ap_update_addrs
			  .ap_dst_ring_rd_index_addr,
		  sizeof(uint16_t), rcv_ring_count,
		  "ap_dst_ring_rd_index_addr" },
		{ bar_layout->bar_memory_area.ap_update_addrs
			  .ap_src_ring_wr_index_addr,
		  sizeof(uint16_t), send_ring_count,
		  "ap_src_ring_wr_index_addr" },
		{ bar_layout->bar_memory_area.arp_stats_addrs
			  .ap_src_data_stats_addr,
		  sizeof(pcie_ap_src_ring_stats_t), send_ring_count,
		  "ap_src_data_stats_addr" },
		{ bar_layout->bar_memory_area.arp_stats_addrs
			  .ap_dst_data_stats_addr,
		  sizeof(pcie_ap_dst_ring_stats_t), rcv_ring_count,
		  "ap_dst_data_stats_addr" },
	};

	for (i = 0; i < ARRAY_SIZE(ranges); i++) {
		size_t total_size = ranges[i].element_size * ranges[i].count;
		uint32_t offset = ranges[i].offset;
		const char *name = ranges[i].name;

		AR_LOG_PCI_DEV_INFO(
			&dev->dev, AR_LOG_INIT,
			"CTRL Ring offset %d [name: %s, offset:0x%x, size: %zu]",
			i, name, offset, total_size);
	}

	// V2 and earlier include the indexes in the status block, no check required
	if (bar_layout->bar_memory_area.arp_protocol_version.major <=
	    PCIE_ARP_PROTOCOL_V2)
		return true;

	for (i = 0; i < ARRAY_SIZE(ranges); i++) {
		int j;
		size_t total_size = ranges[i].element_size * ranges[i].count;
		uint32_t offset = ranges[i].offset;
		const char *name = ranges[i].name;

		// Make sure the offset is after the pcie_bar_memory_area_t at the start of smem
		if (offset < sizeof(pcie_bar_memory_area_t)) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Bad ring offset:0x%x for index entry %d. (%s) Overlaps pcie_bar_memory_area_t",
				offset, i, name);
			return false;
		}

		// Make sure the offset is within the smem region completely
		if (offset + total_size > driver->mmio_memory_bar_size) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Bad ring offset:0x%x (%s) for index entry %d. (%s) Overruns bar region",
				offset, name, i, name);
			return false;
		}

		// Make sure this does not overlap the status block
		if (ranges_overlap(offset, total_size,
				   bar_layout->bar_memory_area.status_block_addr,
				   sizeof(pcie_bar_status_block_area_t))) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Bad ring offset:0x%x (%s) for index entry %d. Overlaps status block",
				offset, name, i);
			return false;
		}

		// check for overlap with subsequent ranges
		for (j = i + 1; j < ARRAY_SIZE(ranges); j++) {
			size_t total_size2 =
				ranges[j].element_size * ranges[j].count;
			uint32_t offset2 = ranges[j].offset;
			const char *name2 = ranges[j].name;

			if (ranges_overlap(offset, total_size, offset2,
					   total_size2)) {
				AR_LOG_PCI_DEV_ERR(
					&dev->dev, AR_LOG_INIT,
					"Bad ring offset:0x%x (%s) size:%zu for index entry %d. Overlaps with [entry: %d, offset: 0x%x (%s), size: %zu]",
					offset, name, total_size, i, j, offset2,
					name2, total_size2);
				return false;
			}
		}
	}

	return true;
}

// Helper to validate the location of the status block in shared mem
static bool ar_pci_bar_handshake_validate_status_block_addr(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout = driver->deprecated_bar_layout;

	AR_ASSERT(bar_layout);
	/*
	 * Check for a reasonable status_block_addr offset
	 * It cannot start before the end of the pcie_bar_memory_area_v2_t
	 * It cannot run past the end of the mapped memory region
	 */
	if (bar_layout->bar_memory_area.arp_protocol_version.major <=
	    PCIE_ARP_PROTOCOL_V2) {
		if ((bar_layout->bar_memory_area.status_block_addr <
		     sizeof(pcie_bar_memory_area_v2_t)) ||
		    ((bar_layout->bar_memory_area.status_block_addr +
		      sizeof(pcie_bar_status_block_area_v2_t)) >
		     driver->mmio_memory_bar_size)) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Bad status_block_addr:0x%x",
				bar_layout->bar_memory_area.status_block_addr);
			return false;
		}
	} else {
		if ((bar_layout->bar_memory_area.status_block_addr <
		     sizeof(pcie_bar_memory_area_t)) ||
		    ((bar_layout->bar_memory_area.status_block_addr +
		      sizeof(pcie_bar_status_block_area_t)) >
		     driver->mmio_memory_bar_size)) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Bad status_block [addr: 0x%x]",
				bar_layout->bar_memory_area.status_block_addr);
			return false;
		}
	}

	/// Validate the address is cached aligned for our uncached writes
	if (!L1_CACHE_ALIGN(bar_layout->bar_memory_area.status_block_addr)) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Unaligned status_block [addr: 0x%x]",
			bar_layout->bar_memory_area.status_block_addr);

		return false;
	}

	return true;
}

static void ar_pci_bar_set_pci_ops(ar_pci_driver_t *driver,
				   pcie_bar_header_t *hdr)
{
	if ((hdr->cp_protocol_version.major == PCIE_ARP_PROTOCOL_V4) &&
	    (hdr->cp_protocol_version.minor >= PCIE_ARP_PROTOCOL_MINOR_V4))
		driver->proto_ops = ar_pci_get_ar_pci_unified_ops();
	else if (hdr->cp_protocol_version.major <= PCIE_ARP_PROTOCOL_V4)
		driver->proto_ops = ar_pci_get_ar_pci_multiple_ops();
	else
		driver->proto_ops = ar_pci_get_ar_pci_unified_ops();

	AR_ASSERT(driver->proto_ops->arfw_pci_proto_set_payload);
	AR_ASSERT(driver->proto_ops->arfw_pci_proto_service_ring_entry);
}

void ar_pci_bar_set_handshake_irq_handler(
	struct pci_dev *dev,
	ar_pci_handshake_irq_handler_t handshake_irq_handler)
{
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);
	AR_ASSERT(handshake_irq_handler);

	driver->handshake_irq_handler = handshake_irq_handler;
}

int ar_pci_bar_handshake(struct pci_dev *dev)
{
	int ret = 0;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	pcie_bar_header_t hdr;

	arfw_mem_util_iomem_read_block(driver->mmio_memory_bar, 0, &hdr,
				       sizeof(hdr));

	// Check for a valid handshake value
	if (hdr.initial_handshake_magic == PCIE_DEFERRED_MAGIC) {
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
				    "Handshake magic check is deferred");
		return -EAGAIN;
	} else if (hdr.initial_handshake_magic != PCIE_HANDSHAKE_MAGIC) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Bad handshake [magic: 0x%x]",
				   hdr.initial_handshake_magic);
		return -EIO;
	}

	if ((hdr.cp_protocol_version.major <
	     MIN_PCIE_ARP_PROTOCOL_MAJOR_VERSION) ||
	    (hdr.cp_protocol_version.major > PCIE_CP_PROTOCOL_MAJOR_VERSION)) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Unsupported PCIe [major: 0x%x, minor: 0x%x], min: 0x%x, max: 0x%x",
			hdr.cp_protocol_version.major,
			hdr.cp_protocol_version.minor,
			MIN_PCIE_ARP_PROTOCOL_MAJOR_VERSION,
			PCIE_CP_PROTOCOL_MAJOR_VERSION);
		return -EINVAL;
	}

	ar_pci_bar_set_pci_ops(driver, &hdr);

	switch (hdr.cp_protocol_version.major) {
	case PCIE_ARP_PROTOCOL_V1:
	case PCIE_ARP_PROTOCOL_V2:
	case PCIE_ARP_PROTOCOL_V3:
	case PCIE_ARP_PROTOCOL_V4:
		ret = ar_pci_bar_handshake_bar_layout(dev);
		break;
	case PCIE_ARP_PROTOCOL_V5:
		ret = ar_pci_bar_handshake_ram_layout(dev);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ar_pci_bar_handshake_bar_layout(struct pci_dev *dev)
{
	int err;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout;

	bar_layout = devm_kzalloc(&dev->dev, sizeof(*bar_layout), GFP_KERNEL);
	if (!bar_layout) {
		err = -ENOMEM;
		goto return_error;
	}
	driver->deprecated_bar_layout = bar_layout;
	arfw_mem_util_iomem_read_block(driver->mmio_memory_bar, 0,
				       &bar_layout->bar_memory_area,
				       sizeof(bar_layout->bar_memory_area));

	driver->ap_ver.major = PCIE_AP_LEGACY_BAR_MAJOR_VERSION;
	driver->ap_ver.minor = PCIE_AP_LEGACY_BAR_MINOR_VERSION;
	// Queue indexes are placed inside the BAR space.
	driver->queue_indexes_in_bar = true;
	driver->ap_update_block_addr = driver->mmio_memory_bar;
	driver->cp_update_block_addr = driver->mmio_memory_bar;

	// Set data ring max before the patch call, since it could be updated
	// for some specific version.
	driver->ap_src_data_ring_max =
		bar_layout->bar_memory_area.ap_src_data_ring_max;
	driver->ap_dst_data_ring_max =
		bar_layout->bar_memory_area.ap_dst_data_ring_max;

	if (!ar_pci_patch_protocol_version(dev)) {
		err = -EINVAL;
		goto fail_handshake;
	}

	// Set the doorbell offset only if the AP's version is at least V4.0
	if (bar_layout->bar_memory_area.arp_protocol_version.major >=
	    PCIE_ARP_PROTOCOL_V4)
		ar_pci_bar_init_doorbell(dev);

	// Validate ring index offsets are sane.
	if (!ar_pci_bar_handshake_validate_ring_offsets(dev)) {
		err = -EINVAL;
		goto fail_handshake;
	}

	// Allocate the per-ring data structures
	if (!ar_pci_bar_handshake_allocate_data_rings_bar_layout(dev)) {
		err = -EINVAL;
		goto fail_handshake;
	}

	// Populate the AP protocol version
	ar_pci_bar_handshake_populate_ap_version(dev);

	// Program the address of the control rings into the status block
	ap_pci_bar_handshake_populate_control_rings(dev);

	// Initialize the data rings structure
	ar_pci_bar_handshake_initiate_data_rings_bar_layout(dev);

	// Need this version even after linkdown, so have to copy now.
	driver->arp_ver = bar_layout->bar_memory_area.arp_protocol_version;

	// Interrupt the ARP
	err = ar_pci_bar_ring_doorbell(dev);
	if (err)
		goto fail_handshake;

	return 0;

fail_handshake:
	devm_kfree(&dev->dev, bar_layout);
return_error:
	return err;
}

static int ar_pci_bar_handshake_ram_layout(struct pci_dev *dev)
{
	int ret = 0;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "RAM layout initialization starting");

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "MMIO memory BAR [vaddr: %p, size: 0x%zx]",
			   driver->mmio_memory_bar,
			   driver->mmio_memory_bar_size);

	arfw_mem_util_iomem_read_block(driver->mmio_memory_bar, 0,
				       &driver->bar_data,
				       sizeof(driver->bar_data));

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Read BAR data block [size: %zu]",
			   sizeof(driver->bar_data));

	driver->ap_ver.major = PCIE_AP_LEGACY_BAR_MAJOR_VERSION;
	driver->ap_ver.minor = PCIE_AP_LEGACY_BAR_MINOR_VERSION;

	driver->ap_src_data_ring_max = arfw_mem_util_iomem_read_16(
		&driver->bar_data, offsetof(pcie_bar_data_t, data_ring_max));
	driver->ap_dst_data_ring_max = arfw_mem_util_iomem_read_16(
		&driver->bar_data, offsetof(pcie_bar_data_t, data_ring_max));

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "AP version [major: %d, minor: %d]",
			   driver->ap_ver.major, driver->ap_ver.minor);
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Data ring max [src: %d, dst: %d]",
			   driver->ap_src_data_ring_max,
			   driver->ap_dst_data_ring_max);

	// Queue indexes are placed in the RAM space.
	driver->queue_indexes_in_bar = false;

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Queue indexes in RAM [in_bar: %d]",
			   driver->queue_indexes_in_bar);

	ar_pci_bar_init_doorbell(dev);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Doorbell initialized [offset: 0x%x]",
			   driver->ar_pcie_doorbell_offset);

	// Allocate the per-ring data structures
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Allocating data rings for RAM layout");
	if (!ar_pci_bar_handshake_allocate_data_rings_ram_layout(dev)) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to allocate data rings for RAM layout");
		return -ENOMEM;
	}

	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"AP update block [vaddr: %p, dma: 0x%llx, size: 0x%zx]",
		driver->ap_update_block_addr, driver->ap_update_block_dma,
		driver->ap_update_block_size);
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"CP update block [vaddr: %p, dma: 0x%llx, size: 0x%zx]",
		driver->cp_update_block_addr, driver->cp_update_block_dma,
		driver->cp_update_block_size);

	// Populate the AP protocol version
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Populating AP protocol version for RAM layout");
	ar_pci_bar_handshake_populate_ap_version_ram_layout(dev);

	// Program the address of the control rings into the status block
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Populating control rings for RAM layout");
	ap_pci_bar_handshake_populate_control_rings_ram_layout(dev);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "AP SRC control ring [vaddr: %p, dma: 0x%llx]",
			   driver->ap_src_control_ring->va,
			   driver->ap_src_control_ring->dma_handle);
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "AP DST control ring [vaddr: %p, dma: 0x%llx]",
			   driver->ap_dst_control_ring->va,
			   driver->ap_dst_control_ring->dma_handle);
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"AP SRC buffer control ring [vaddr: %p, dma: 0x%llx]",
		driver->ap_src_buffer_control_ring->va,
		driver->ap_src_buffer_control_ring->dma_handle);

	// Initialize the data rings structure
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Initiating data rings for RAM layout");
	ar_pci_bar_handshake_initiate_data_rings_ram_layout(dev);

	// Need this version even after linkdown, so have to copy now.
	driver->arp_ver = driver->bar_data.header.cp_protocol_version;

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "ARP version [major: %d, minor: %d]",
			   driver->arp_ver.major, driver->arp_ver.minor);

	// Interrupt the ARP
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_INIT,
		"Ringing doorbell to complete RAM layout initialization");
	ret = ar_pci_bar_ring_doorbell(dev);

	if (ret)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to ring doorbell [err: %d]", ret);
	else
		AR_LOG_PCI_DEV_DBG(
			&dev->dev, AR_LOG_INIT,
			"RAM layout initialization completed successfully");

	return ret;
}

void ar_pci_bar_release(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	switch (driver->arp_ver.major) {
	case PCIE_ARP_PROTOCOL_V1:
	case PCIE_ARP_PROTOCOL_V2:
	case PCIE_ARP_PROTOCOL_V3:
	case PCIE_ARP_PROTOCOL_V4:
		ar_pci_bar_release_bar_layout(dev);
		break;
	case PCIE_ARP_PROTOCOL_V5:
		ar_pci_bar_release_ram_layout(dev);
		break;
	default:
		break;
	}
}

static void ar_pci_bar_release_bar_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout;

	AR_ASSERT(driver);
	devm_kfree(&dev->dev, driver->rcv_data_rings);
	devm_kfree(&dev->dev, driver->send_data_rings);
	bar_layout = driver->deprecated_bar_layout;
	AR_ASSERT(bar_layout);
	devm_kfree(&dev->dev, bar_layout->ap_rd_index_cached);
	devm_kfree(&dev->dev, bar_layout->ap_wr_index_cached);
	devm_kfree(&dev->dev, bar_layout->arp_update_rd_index_cached);
	devm_kfree(&dev->dev, bar_layout->arp_update_wr_index_cached);
	devm_kfree(&dev->dev, driver->deprecated_bar_layout);

	driver->rcv_data_rings = NULL;
	driver->send_data_rings = NULL;
	driver->deprecated_bar_layout = NULL;
}

static void ar_pci_bar_release_ram_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	dma_free_coherent(&dev->dev, driver->cp_update_block_size,
			  driver->cp_update_block_addr,
			  driver->cp_update_block_dma);
	dma_free_coherent(&dev->dev, driver->ap_update_block_size,
			  driver->ap_update_block_addr,
			  driver->ap_update_block_dma);
	devm_kfree(&dev->dev, driver->rcv_data_rings);
	devm_kfree(&dev->dev, driver->send_data_rings);

	driver->cp_update_block_addr = NULL;
	driver->cp_update_block_dma = 0;
	driver->cp_update_block_size = 0;
	driver->ap_update_block_addr = NULL;
	driver->ap_update_block_dma = 0;
	driver->ap_update_block_size = 0;
	driver->rcv_data_rings = NULL;
	driver->send_data_rings = NULL;
}

// Enable the doorbell-defer feature
static unsigned long doorbell_defer;
module_param(doorbell_defer, ulong, 0664);

// Ring the doorbell only once during the window
static unsigned long doorbell_window_us;
module_param(doorbell_window_us, ulong, 0664);

// The time to wait before triggering the doorbell
static unsigned long doorbell_delay_us = 1;
module_param(doorbell_delay_us, ulong, 0664);

// Debug counter, total pending doorbells
static unsigned long doorbell_pending;
module_param(doorbell_pending, ulong, 0664);

bool ar_pci_bar_doorbell_deferred(void)
{
	return doorbell_defer;
}

static void ar_pci_do_ring_doorbell(ar_pci_driver_t *driver)
{
	AR_ASSERT(driver);

#ifdef CONFIG_ARFIRMWARE_PCI_GPIO_DOORBELL
	// Special case for Acro where we use a GPIO as the doorbell mechanism
	unsigned long flags;
	spin_lock_irqsave(&driver->doorbell_lock, flags);
	gpiod_set_value(driver->acro_doorbell_gpio, 1);
	gpiod_set_value(driver->acro_doorbell_gpio, 0);
	spin_unlock_irqrestore(&driver->doorbell_lock, flags);
#else
	arfw_mem_util_iomem_write_8(driver->mmio_interrupt_bar,
				    driver->ar_pcie_doorbell_offset,
				    MMIO_INTERRUPT_VALUE);
#endif
}

enum hrtimer_restart ar_pci_doorbell_timer_callback(struct hrtimer *timer)
{
	unsigned long flags;
	ar_pci_driver_t *driver;

	AR_ASSERT(timer);

	driver = container_of(timer, ar_pci_driver_t, doorbell_timer);
	AR_ASSERT(driver);

	spin_lock_irqsave(&driver->doorbell_lock, flags);

	driver->doorbell_timer_active = false;
	doorbell_pending = driver->doorbell_pending;
	driver->doorbell_pending = 0;
	driver->last_doorbell = ktime_get();

	spin_unlock_irqrestore(&driver->doorbell_lock, flags);

	ar_pci_bar_set_ap_update_region(driver->dev);
	ar_pci_do_ring_doorbell(driver);

	return HRTIMER_NORESTART;
}

void ar_pci_bar_init_doorbell(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	pcie_bar_header_t hdr;

	AR_ASSERT(driver);
	arfw_mem_util_iomem_read_block(driver->mmio_memory_bar, 0, &hdr,
				       sizeof(hdr));

	switch (hdr.cp_protocol_version.major) {
	case PCIE_ARP_PROTOCOL_V1:
	case PCIE_ARP_PROTOCOL_V2:
	case PCIE_ARP_PROTOCOL_V3:
	case PCIE_ARP_PROTOCOL_V4:
		ar_pci_bar_init_doorbell_bar_layout(dev);
		break;
	case PCIE_ARP_PROTOCOL_V5:
		ar_pci_bar_init_doorbell_ram_layout(dev);
		break;
	default:
		break;
	}
}

static void ar_pci_bar_init_doorbell_bar_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver;
	pcie_bar_memory_area_t bar_memory;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);
	arfw_mem_util_iomem_read_block(driver->mmio_memory_bar, 0, &bar_memory,
				       sizeof(bar_memory));
	driver->ar_pcie_doorbell_offset = bar_memory.doorbell_offset;
}

static void ar_pci_bar_init_doorbell_ram_layout(struct pci_dev *dev)
{
	ar_pci_driver_t *driver;
	pcie_bar_data_t bar_memory;

	driver = pci_get_drvdata(dev);
	arfw_mem_util_iomem_read_block(driver->mmio_memory_bar, 0, &bar_memory,
				       sizeof(bar_memory));
	driver->ar_pcie_doorbell_offset = bar_memory.doorbell_offset;
}

int ar_pci_bar_ring_doorbell(struct pci_dev *dev)
{
	unsigned long flags;
	int64_t delta_us;
	ktime_t delay = 0;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);

	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	if (doorbell_defer) {
		spin_lock_irqsave(&driver->doorbell_lock, flags);

		if (!driver->doorbell_timer_active) {
			delta_us = ktime_to_us(
				ktime_sub(ktime_get(), driver->last_doorbell));

			if (doorbell_window_us > 0 && delta_us > 0 &&
			    delta_us < doorbell_window_us)
				// avoid multiple doorbells within doorbell_window_us window
				delay = ktime_set(
					0, USEC_TO_NSEC(doorbell_window_us -
							delta_us));
			else
				// ensure the doorbell is not triggered too soon
				delay = ktime_set(
					0, USEC_TO_NSEC(doorbell_delay_us));

			driver->doorbell_timer_active = true;
			driver->doorbell_pending++;
		}

		driver->doorbell_pending++;
		spin_unlock_irqrestore(&driver->doorbell_lock, flags);

		if (delay > 0)
			hrtimer_start(&driver->doorbell_timer, delay,
				      HRTIMER_MODE_REL);
	} else {
		ar_pci_do_ring_doorbell(driver);
	}

	return 0;
}

// Must return true when packet is handled to signify a change of memory ownership.
static bool ar_pci_bar_handle_reply_packet(struct pci_dev *dev,
					   pcie_ctrl_resp_t *packet)
{
	unsigned long flags;
	ar_promise_t *promise, *tmp;
	ar_pci_cmd_reply_promise_t *cmd_promise;
	ar_pci_driver_t *driver;
	bool promise_found = false;

	AR_ASSERT(dev);
	AR_ASSERT(packet);

	driver = pci_get_drvdata(dev);

	spin_lock_irqsave(&driver->cmd_reply_lock, flags);
	list_for_each_entry_safe(promise, tmp, &driver->cmd_reply_list, list) {
		cmd_promise =
			container_of(promise, ar_pci_cmd_reply_promise_t, base);
		if (cmd_promise->seq_num == packet->common_header.seq_num) {
			promise_found = true;
			list_del_init(&cmd_promise->base.list);
			break;
		}
	}
	spin_unlock_irqrestore(&driver->cmd_reply_lock, flags);

	if (!promise_found)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
				   "Response received without a request");
	else
		// At this point the promise now owns the packet memory
		// and is responsible for freeing it.
		cmd_promise->complete(cmd_promise, packet);

	return promise_found;
}

static bool ar_pci_bar_handle_buf_get_msg(struct pci_dev *dev,
					  pcie_ctrl_resp_t *packet)
{
	int ready;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	pcie_dst_buf_get_msg_t *msg = &packet->buf_req;
	pcie_ipc_header_t *header = &msg->ipc_header;
	ar_client_queue_t *queue = NULL;

	mutex_lock(&driver->client_rcv_queues.lock);

	if (AR_UNLIKELY(driver->client_rcv_queues.released))
		goto done;

	list_for_each_entry(queue, &driver->client_rcv_queues.list_node, node) {
		ready = ar_atomic_load(&queue->ready, AR_MEMORY_ORDER_SEQ_CST);
		if (AR_UNLIKELY(!ready)) {
			AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_PEND,
					     "Queue not ready");
			continue;
		} else if (AR_UNLIKELY(queue->released)) {
			AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_PEND,
					     "Queue is released and skipped");
			continue;
		} else if (header->ap_ep_id ==
				   queue->queue_params.hlos_endpoint_id &&
			   header->arp_ep_id ==
				   queue->queue_params.fw_endpoint_id) {
			int ret;

			AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_PEND,
					     "Request pend buffer [len: %d]",
					     msg->len);

			ret = queue->driver->client_ops
				      ->handle_client_payload_pend_required(
					      queue->arfw_queue, msg->len);

			if (ret)
				AR_LOG_PCI_QUEUE_ERR(
					queue, AR_LOG_PEND,
					"Failed to request pend buffer of length [len: %d, err: %d]",
					msg->len, ret);

			goto done;
		}
	}

	AR_LOG_PCI_DEV_ERR(
		&dev->dev, AR_LOG_PEND,
		"No matching session for pend buffer required message [AP: 0x%x, FW: 0x%x, len: %d]",
		header->ap_ep_id, header->arp_ep_id, msg->len);

done:
	mutex_unlock(&driver->client_rcv_queues.lock);
	kfree(packet);

	return true;
}

static bool ar_pci_handle_d3_req(struct pci_dev *dev, pcie_ctrl_resp_t *packet)
{
	int err;

	(void)packet;

	err = pm_runtime_put(&dev->dev);
	if (err)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_READ_CTRL,
				   "Failed to suspend device [err: %d]", err);

	return true;
}

static bool ar_pci_handle_wake_req(struct pci_dev *dev,
				   pcie_ctrl_resp_t *packet)
{
	ar_cp_req_wake_t *wake_req = &packet->ar_cp_req_wake;
	int err;

	err = ar_pci_wake_functions(wake_req->func_bit_mask);
	if (err)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_READ_CTRL,
				   "Failed to wake functions [err: %d]", err);

	return true;
}

// This function must return true if the message has been handled.
// This lets the ar_pci_bar_service_control_rings function know that it
// no longer owns the packet memory.
static bool ar_pci_bar_handle_ctrl_packet(struct pci_dev *dev,
					  pcie_ctrl_resp_t *packet)
{
	switch (packet->common_header.msg_type) {
	case CTRL_PING_RSP:
		/* FALLTHROUGH */
	case DELETE_RING_RSP:
		/* FALLTHROUGH */
	case CREATE_RING_RSP:
		/* FALLTHROUGH */
	case DISABLE_RING_RSP:
		/* FALLTHROUGH */
	case ARP_INFO_RSP:
		/* FALLTHROUGH */
	case APERTURE_MAP_RSP:
		/* FALLTHROUGH */
	case DELETE_ALL_RINGS_RSP:
		/* FALLTHROUGH */
	case AR_DUTY_CYCLE_RSP:
		/* FALLTHROUGH */
	case CP_D3_RSP:
		return ar_pci_bar_handle_reply_packet(dev, packet);
	case DEST_DATA_BUF_GET:
		return ar_pci_bar_handle_buf_get_msg(dev, packet);
	case CP_D3_REQ:
		return ar_pci_handle_d3_req(dev, packet);
	case CP_WAKE_REQ:
		return ar_pci_handle_wake_req(dev, packet);
	default:
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_READ_CTRL,
				   "Unexpected packet [msg_type: %u]",
				   packet->common_header.msg_type);

		// return false to let the caller know that it still owns the packet.
		return false;
	}
}

// Increment the cached index of a ctrl ring
// This should only be called if the index in the bar has been checked
static inline void
ar_pci_bar_increment_cached_index(ar_pci_control_ring_context_t *ring)
{
	ring->cached_ring_index++;
	ring->cached_ring_index = ring->cached_ring_index % ring->element_count;
}

static void ar_pci_bar_service_control_rings(struct pci_dev *dev)
{
	uint16_t ap_src_ctrl_rd_index;
	uint16_t ap_dst_ctrl_wr_index;
	uint16_t ap_src_buf_rd_index;
	uint32_t index, old_idx;
	int ret;
	pcie_ctrl_resp_t *data;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	// Update control ring indexes
	ap_src_ctrl_rd_index = arfw_mem_util_iomem_read_16(
		driver->cp_update_block_addr,
		driver->ap_src_control_ring->rd_index_reg);

	ap_dst_ctrl_wr_index = arfw_mem_util_iomem_read_16(
		driver->cp_update_block_addr,
		driver->ap_dst_control_ring->wr_index_reg);

	ap_src_buf_rd_index = arfw_mem_util_iomem_read_16(
		driver->cp_update_block_addr,
		driver->ap_src_buffer_control_ring->rd_index_reg);

	/*
	 * First, check on consumed payload space in the ap_src rings.
	 * That operation is cheap.
	 */
	// ap_src_control ring
	while (driver->link_is_up &&
	       driver->ap_src_control_ring->cached_ring_index !=
		       ap_src_ctrl_rd_index) {
		/*
		 * The index read from bar is advanced by 1, compared to internal ring
		 * semantics. Update after the call to consumed.
		 */
		ret = ap_src_ring_mark_consumed(
			driver->ap_src_control_ring,
			driver->ap_src_control_ring->cached_ring_index);
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_WRITE_CTRL,
				"AP_SRC_CTRL_RING failed to mark consumed, skipping ring [err %d]",
				ret);
			return;
		}

		AR_LOG_PCI_DEV_DBG(
			&dev->dev, AR_LOG_WRITE_CTRL,
			"AP_SRC_CTRL_RING Consumed by FW [idx: %u]",
			driver->ap_src_control_ring->cached_ring_index);

		// increment, modulo ring size
		ar_pci_bar_increment_cached_index(driver->ap_src_control_ring);
	}

	// ap_src_buf_control ring
	while (driver->link_is_up &&
	       driver->ap_src_buffer_control_ring->cached_ring_index !=
		       ap_src_buf_rd_index) {
		/*
		 * The index read from bar is advanced by 1, compared to internal ring
		 * semantics. Update after the call to consumed.
		 */
		ret = ap_src_ring_mark_consumed(
			driver->ap_src_buffer_control_ring,
			driver->ap_src_buffer_control_ring->cached_ring_index);
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_WRITE_CTRL,
				"AP_SRC_BUF_RING failed to mark consumed, skipping ring [err %d]",
				ret);
			return;
		}

		AR_LOG_PCI_DEV_DBG(
			&dev->dev, AR_LOG_WRITE_CTRL,
			"AP_SRC_BUF_RING Consumed by FW [idx: %u]",
			driver->ap_src_buffer_control_ring->cached_ring_index);

		// increment, modulo ring size
		ar_pci_bar_increment_cached_index(
			driver->ap_src_buffer_control_ring);
	}

	// Finally the ap_dst_control_ring, these will require work being done
	while (driver->link_is_up &&
	       driver->ap_dst_control_ring->cached_ring_index !=
		       ap_dst_ctrl_wr_index) {
		ret = ap_dst_ring_reserve_for_produce(
			driver->ap_dst_control_ring, &index);
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_WRITE_CTRL,
				"AP_DST_CTRL_RING failed to reserve for produce, skipping ring [err %d]",
				ret);
			return;
		}

		ret = ap_dst_ring_mark_produced(driver->ap_dst_control_ring,
						index);
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_WRITE_CTRL,
				"AP_DST_CTRL_RING failed to mark produced, skipping ring [err %d]",
				ret);
			return;
		}

		// Allocate space for the message initally to avoid
		// copying twice when delivering to a promise
		data = kzalloc(sizeof(pcie_ctrl_resp_t), GFP_KERNEL);
		if (data == NULL) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_READ_CTRL,
				"Failed to allocate memory for ctrl_resp");
			return;
		}

		old_idx = index;

		ret = ap_dst_ring_consume(driver->ap_dst_control_ring, &index,
					  data, sizeof(pcie_ctrl_resp_t));
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_WRITE_CTRL,
				"AP_DST_CTRL_RING failed to consume, skipping ring [err %d]",
				ret);
			return;
		}

		AR_LOG_PCI_DEV_DBG(
			&dev->dev, AR_LOG_READ_CTRL,
			"AP_DST_CTRL_RING Produced by FW [idx: %u, type: %s, flag: %u, seq_num: %u]",
			old_idx, // advanced by ap_dst_ring_consume
			msg_type_name(data->common_header.msg_type),
			data->common_header.flag, data->common_header.seq_num);

		if (!ar_pci_bar_handle_ctrl_packet(dev, data)) {
			// If we are unable to handle the packet, we still own its memory
			// and must free it
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_READ_CTRL,
				"Failed to handle packet. Freeing packet");
			kfree(data);
		}

		// increment, modulo ring size
		ar_pci_bar_increment_cached_index(driver->ap_dst_control_ring);
		arfw_mem_util_iomem_write_16(
			driver->ap_update_block_addr,
			driver->ap_dst_control_ring->rd_index_reg,
			driver->ap_dst_control_ring->cached_ring_index);
	}
}

static irqreturn_t ar_pci_bar_data_irq_thread(int irq, void *ptr)
{
	ar_pci_driver_t *driver = (ar_pci_driver_t *)ptr;

	int local_irq_count;

	might_sleep();

	do {
		local_irq_count = atomic_read(&driver->data_irq_count);

		// Interrupts can be enabled before link is up. In this case, wrong interrupt, so log and ignore.
		if (!driver->link_is_up) {
			AR_LOG_PCI_DEV_ERR(
				&driver->dev->dev, AR_LOG_INTERRUPT,
				"Data interrupt (%d) received before link is up",
				irq);
			continue;
		}

		ar_pci_bar_get_arp_update_region(driver->dev);
		ar_pci_bar_service_control_rings(driver->dev);
		ar_client_send_ring_list_service(&driver->client_send_queues);
		ar_client_rcv_ring_list_service(&driver->client_rcv_queues);

		// Keep servicing rings until no new irq arrives
	} while (atomic_read(&driver->data_irq_count) != local_irq_count);

	return IRQ_HANDLED;
}

static irqreturn_t ar_pci_bar_data_irq_handler(int irq, void *ptr)
{
	ar_pci_driver_t *driver = (ar_pci_driver_t *)ptr;
	AR_ASSERT(driver);

	// increment irq count so irq thread can check
	// for irq that happened while running
	atomic_inc_return(&driver->data_irq_count);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ar_pci_bar_handshake_irq_thread(int irq, void *ptr)
{
	int err;
	ar_pci_driver_t *driver = (ar_pci_driver_t *)ptr;

	might_sleep();
	AR_ASSERT(driver);
	AR_LOG_PCI_DEV_DBG(&driver->dev->dev, AR_LOG_INTERRUPT,
			   "Handshake interrupt irq (%d) received", irq);

	// TODO(T221248887): refactor handshake code to be more generic (not live in hw specific driver code)
	if (!driver->handshake_irq_handler) {
		AR_LOG_PCI_DEV_ERR(&driver->dev->dev, AR_LOG_INTERRUPT,
				   "Handshake interrupt handler not set!");
		return IRQ_HANDLED;
	}

	/**
	 * Handshake does "power ops" so grab this lock.
	 * This mutex also protects against getting the interrupt while we are doing the handshake in probe
	 */
	mutex_lock(&driver->power.lock);
	err = driver->handshake_irq_handler(driver->dev);
	if (err)
		AR_LOG_PCI_DEV_ERR(
			&driver->dev->dev, AR_LOG_INTERRUPT,
			"Handshake interrupt handler failed [err: %d]", err);
	mutex_unlock(&driver->power.lock);

	return IRQ_HANDLED;
}

static irqreturn_t ar_pci_bar_handshake_irq_handler(int irq, void *ptr)
{
	ar_pci_driver_t *driver = (ar_pci_driver_t *)ptr;

	(void)irq;
	AR_ASSERT(driver);

	// Only allow one handshake IRQ to wake up the handshake thread.
	// Ignore the rest and keep marking IRQs as handled.
	// There is no need to handle handshake past it's first occurrence.
	// Also helps to prevent issues with PERST on the deprobe path triggering this MSI.

	return atomic_cmpxchg(&driver->handshake_irq_count, 0, 1) ?
			     IRQ_HANDLED :
			     IRQ_WAKE_THREAD;
}

// There seems to be an issue with allocating 32 per function.
// For now reduce the numbers and accomodate backwards compat for oatmeal.
// See: T220776885. Set the default to 4 which is the minimum required for
// Oatmeal, but allow it to be overridden by a module param.
static unsigned int msi_num_min = 4;
module_param(msi_num_min, uint, 0664);
static unsigned int msi_num_max = 4;
module_param(msi_num_max, uint, 0664);

int ar_pci_bar_enable_interrupt(struct pci_dev *dev)
{
	int ret = 0;
	int num_int, i, irq_vector;
	int num_int_min = msi_num_min, num_int_max = msi_num_max;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	// We try to allocate the MAX number that PCI supports (by default 32).
	// But num_int will return/allocate the number firmware actually allows.
	// Normally arfw requires 2 MSI to work, so MIN is 2 (by default), we thread it.
	// If firmware supports more on top, they can be used by drivers sharing
	// the same function as arfw_pci (via arfw_shim).
	num_int = pci_alloc_irq_vectors(dev, num_int_min, num_int_max,
					PCI_IRQ_MSI);
	if (num_int < 0) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to allocate MSI vectors, err=%d, min=%d, max=%d",
			num_int, num_int_min, num_int_max);
		return num_int;
	}

	if (num_int < num_int_min) {
		pci_free_irq_vectors(dev);
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Needed %d interrupts but got %d",
				   num_int_min, num_int);
		return -EFAULT;
	}

	driver->msi_int_num = 0;
	for (i = 0; i < num_int_min; i++) {
		irq_vector = pci_irq_vector(dev, i);
		// handshake interrupt irq handler
		if (i == driver->handshake_irq_index) {
			// handshake interrupt handler
			AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
					   "Handshake IRQ request: %d (%d)", i,
					   irq_vector);
			ret = devm_request_threaded_irq(
				&dev->dev, irq_vector,
				ar_pci_bar_handshake_irq_handler,
				ar_pci_bar_handshake_irq_thread,
				IRQF_TRIGGER_NONE, AR_DRIVER_NAME, driver);
			if (ret) {
				AR_LOG_PCI_DEV_ERR(
					&dev->dev, AR_LOG_INIT,
					"Handshake IRQ request failed [msi vec: %d, irq: %d, err: %d]",
					i, irq_vector, ret);
				goto free_irqs;
			}

			driver->msi_int_num++;
			continue;
		}

		// data interrupt handler(s)
		AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
				   "Data IRQ request: %d (%d)", i, irq_vector);
		ret = devm_request_threaded_irq(&dev->dev, irq_vector,
						ar_pci_bar_data_irq_handler,
						ar_pci_bar_data_irq_thread,
						IRQF_SHARED, AR_DRIVER_NAME,
						driver);
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Data IRQ request failed [msi vec: %d, irq: %d, err: %d]",
				i, irq_vector, ret);
			goto free_irqs;
		}

		driver->msi_int_num++;
	}

	return 0;

free_irqs:
	ar_pci_bar_disable_interrupt(dev);

	return ret;
}

/*
 * Clear MSI address registers in PCI config space.
 *
 * Custom firmware reads MSI addresses directly from config space.
 * pci_free_irq_vectors() does not zero these registers, leaving stale
 * values. This function zeroes the MSI address registers before IRQs
 * are freed.
 */
static void ar_pci_bar_clear_msi_address(struct pci_dev *dev)
{
	int pos;
	u16 msi_flags;

	pos = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (!pos)
		return;

	pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &msi_flags);

	pci_write_config_dword(dev, pos + PCI_MSI_ADDRESS_LO, 0);

	if (msi_flags & PCI_MSI_FLAGS_64BIT)
		pci_write_config_dword(dev, pos + PCI_MSI_ADDRESS_HI, 0);
}

void ar_pci_bar_disable_interrupt(struct pci_dev *dev)
{
	int i;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	ar_pci_bar_clear_msi_address(dev);

	for (i = 0; i < driver->msi_int_num; i++) {
		AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT, "Free IRQ: %d", i);
		devm_free_irq(&dev->dev, pci_irq_vector(dev, i), driver);
	}
	driver->msi_int_num = 0;

	pci_free_irq_vectors(dev);
}

static int ar_pci_bar_submit_packet(ar_pci_driver_t *driver,
				    ar_pci_control_ring_context_t *ring,
				    void *packet, size_t size,
				    bool ring_doorbell)
{
	int ret;
	uint32_t index;

	mutex_lock(&ring->lock);
	ret = ap_src_ring_produce(ring, &index, packet, size);
	if (ret) {
		mutex_unlock(&ring->lock);
		return ret;
	}

	// Update the write index in the bar
	index = (index + 1) % ring->element_count;
	arfw_mem_util_iomem_write_16(driver->ap_update_block_addr,
				     ring->wr_index_reg, (uint16_t)index);

	mutex_unlock(&ring->lock);

	if (ring_doorbell)
		ar_pci_bar_ring_doorbell(driver->dev);

	return 0;
}

int ar_pci_bar_submit_control_packet(struct pci_dev *dev,
				     pcie_ctrl_req_t *packet,
				     bool ring_doorbell)
{
	int ret;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_WRITE_CTRL,
			   "Submit control packet [type: %s]",
			   msg_type_name(packet->common_header.msg_type));

	ret = ar_pci_bar_submit_packet(driver, driver->ap_src_control_ring,
				       packet, sizeof(pcie_ctrl_req_t),
				       ring_doorbell);

	if (ret)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
				   "Failed to submit control packet [err: %d]",
				   ret);

	return ret;
}

int ar_pci_bar_submit_ap_src_buffer(struct pci_dev *dev,
				    pcie_ap_src_buf_item_t *packet,
				    bool ring_doorbell)
{
	int ret;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	ret = ar_pci_bar_submit_packet(driver,
				       driver->ap_src_buffer_control_ring,
				       packet, sizeof(pcie_ap_src_buf_item_t),
				       ring_doorbell);

	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE,
				   "Failed to submit control packet [err: %d]",
				   ret);
	}

	return ret;
}

uint16_t ar_pci_bar_next_cmd_sequence_number(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	return atomic_inc_return(&driver->next_seq_num);
}

// Helper to point "new" dynamic offsets back into protocol v2 fixed offsets
static void ar_pci_patch_ring_data_offsets(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout = driver->deprecated_bar_layout;

	AR_ASSERT(bar_layout);
	bar_layout->bar_memory_area.ap_src_data_ring_max =
		PCIE_AP_SRC_DATA_MAX_V2;
	bar_layout->bar_memory_area.ap_dst_data_ring_max =
		PCIE_AP_DST_DATA_MAX_V2;

	// Arp Update Fields
	bar_layout->bar_memory_area.arp_update_addrs.ap_src_ring_rd_index_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver,
					    arp_update.ap_src_ring_rd_index);
	bar_layout->bar_memory_area.arp_update_addrs.ap_dst_ring_wr_index_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver,
					    arp_update.ap_dst_ring_wr_index);
	bar_layout->bar_memory_area.arp_update_addrs.ap_src_ring_status_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver,
					    arp_update.ap_src_ring_status);
	bar_layout->bar_memory_area.arp_update_addrs.ap_dst_ring_status_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver,
					    arp_update.ap_dst_ring_status);

	// Ap Update Fields
	bar_layout->bar_memory_area.ap_update_addrs.ap_dst_ring_rd_index_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver,
					    ap_update.ap_dst_ring_rd_index);
	bar_layout->bar_memory_area.ap_update_addrs.ap_src_ring_wr_index_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver,
					    ap_update.ap_src_ring_wr_index);

	// Statistic Fields
	bar_layout->bar_memory_area.arp_stats_addrs.ap_src_data_stats_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver, ap_src_data_stats);
	bar_layout->bar_memory_area.arp_stats_addrs.ap_dst_data_stats_addr =
		AR_BAR_STATUS_BLOCK_V2_ADDR(driver, ap_dst_data_stats);
}

/*
 * ARP V3.0 has bad logic in it's version check.
 * Because of this the AP must always tell the ARP it is version 0.1 to work
 */
static void ar_pci_patch_arp_v3_0(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	driver->ap_ver.major = PCIE_AP_PROTOCOL_V0;
	driver->ap_ver.minor = PCIE_AP_PROTOCOL_MINOR_V1;
}

static void ar_pci_patch_arp_info(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	arp_info_t *arp_info = &driver->arp_info;

	arp_info->rcv_ring_pend_buff_count_max =
		ARP_DEFAULT_NUM_BUF_PER_SESSION;
	arp_info->valid = true;
}

static bool ar_pci_patch_protocol_version(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	ar_pci_bar_layout_t *bar_layout = driver->deprecated_bar_layout;

	AR_ASSERT(bar_layout);
	switch (bar_layout->bar_memory_area.arp_protocol_version.major) {
	case PCIE_ARP_PROTOCOL_V1:
		// Version 0x10000 requires a diffrent doorbell address
		driver->ar_pcie_doorbell_offset = PCIE_ARP_DOORBELL_OFFSET_V1;
		fallthrough;
	case PCIE_ARP_PROTOCOL_V2:
		ar_pci_patch_ring_data_offsets(dev);
		ar_pci_patch_arp_info(dev);
		driver->ar_pcie_doorbell_offset = PCIE_ARP_DOORBELL_OFFSET_V2;
		break;
	case PCIE_ARP_PROTOCOL_V3:
		if (bar_layout->bar_memory_area.arp_protocol_version.minor ==
		    PCIE_ARP_PROTOCOL_MINOR_V0) {
			// If the ARP is V3.0 we must lie about our version due to bad
			// versioning logic on the ARP
			ar_pci_patch_arp_v3_0(dev);
			ar_pci_patch_arp_info(dev);
		}
		driver->ar_pcie_doorbell_offset = PCIE_ARP_DOORBELL_OFFSET_V2;
		break;
	default:
		// No changes needed for nonlisted versions
		break;
	}

	// Sanity check status block offset per version
	return ar_pci_bar_handshake_validate_status_block_addr(dev);
}

static void ar_pci_get_arp_info_cmd_reply(ar_pci_cmd_reply_promise_t *promise,
					  pcie_ctrl_resp_t *packet)
{
	AR_ASSERT(promise);
	AR_ASSERT(packet);

	if (packet->common_header.msg_type != ARP_INFO_RSP) {
		AR_LOG_PCI_ERR(
			AR_LOG_WRITE_CTRL,
			"Response type is not ARP_INFO_RSP [msg_type: %d]",
			packet->common_header.msg_type);
		kfree(packet);
		ar_future_complete(promise->base.future, -EINVAL);
	} else {
		ar_future_complete_data(promise->base.future, packet);
	}
}

static ar_future_t *ar_pci_get_arp_info_req(struct pci_dev *dev,
					    uint32_t timeout_ms)
{
	int err;
	unsigned long flags;
	ar_pci_driver_t *driver;
	ar_pci_cmd_reply_promise_t *promise;
	ar_future_t *future;
	uint16_t seq_num;
	pcie_ctrl_req_t packet;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	seq_num = ar_pci_bar_next_cmd_sequence_number(dev);
	packet.arp_info.common_header.msg_type = ARP_INFO_REQ;
	packet.arp_info.common_header.seq_num = seq_num;

	future = ar_create_future(&dev->dev);
	if (!future) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
				   "Failed to allocate future");
		goto error_no_future;
	}

	promise = devm_kzalloc(&dev->dev, sizeof(ar_pci_cmd_reply_promise_t),
			       GFP_KERNEL);
	if (!promise) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
				   "Failed to allocate promise");
		goto error_no_promise;
	}

	ar_future_set_promise(future, &promise->base);

	promise->base.lock = &driver->cmd_reply_lock;
	promise->seq_num = seq_num;
	promise->complete = ar_pci_get_arp_info_cmd_reply;

	spin_lock_irqsave(promise->base.lock, flags);
	list_add_tail(&promise->base.list, &driver->cmd_reply_list);
	spin_unlock_irqrestore(promise->base.lock, flags);

	err = ar_pci_bar_submit_control_packet(dev, &packet, true);
	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
				   "Failed to send ARP info request [err: %d]",
				   err);
		goto error_no_submit;
	}

	return future;

error_no_submit:
	spin_lock_irqsave(promise->base.lock, flags);
	list_del(&promise->base.list);
	spin_unlock_irqrestore(promise->base.lock, flags);
	devm_kfree(&dev->dev, promise);
error_no_promise:
	devm_kfree(&dev->dev, future);
error_no_future:
	return NULL;
}

arp_info_t *ar_pci_get_arp_info(struct pci_dev *dev, uint32_t timeout_ms)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	arp_info_t *arp_info = &driver->arp_info;

	// Check to see if we have gotten the arp info yet
	if (!arp_info->valid) {
		int err;
		ar_future_t *future;
		pcie_arp_info_resp_t *resp = NULL;

		future = ar_pci_get_arp_info_req(dev, timeout_ms);
		if (future == NULL) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_WRITE_CTRL,
				"Failed to request ARP info request");
			return NULL;
		}

		err = ar_future_wait_data(future, (void **)&resp);
		if (err) {
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
					   "Failed to get ARP info [err: %d]",
					   err);
			arp_info = NULL;
		} else {
			arp_info->rcv_ring_pend_buff_count_max =
				resp->rcv_ring_pend_buff_count_max;
			if (driver->sys_name_supported) {
				AR_ASSERT(
					resp->sys_name_len +
						sizeof(ARFW_COL_DEVID_PREFIX) <=
					ARFW_COL_DEVID_LEN);

				snprintf(arp_info->devid, ARFW_COL_DEVID_LEN,
					 "%s-%s", ARFW_COL_DEVID_PREFIX,
					 resp->sys_name);
			} else {
				snprintf(arp_info->devid, ARFW_COL_DEVID_LEN,
					 "%s-%d", ARFW_COL_DEVID_PREFIX,
					 dev->devfn & PCI_FUNCTION_MASK);
			}

			arp_info->valid = true;
		}

		kfree(resp);
	}

	return arp_info;
}

static int ar_pci_bar_get_arp_update_region(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	size_t arp_update_rd_size;
	size_t arp_update_wr_size;
	ar_pci_bar_layout_t *bar_layout;

	AR_ASSERT(driver);
	// Nothing to update if queue indexes are placed in RAM.
	if (!driver->queue_indexes_in_bar)
		return 0;

	bar_layout = driver->deprecated_bar_layout;
	AR_ASSERT(bar_layout);
	arp_update_rd_size = driver->ap_src_data_ring_max * sizeof(uint16_t);
	arp_update_wr_size = driver->ap_dst_data_ring_max * sizeof(uint16_t);
	if (bar_layout->arp_update_wr_index_cached == NULL ||
	    bar_layout->arp_update_rd_index_cached == NULL) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_READ,
				   "Invalid cached ARP update region");
		return -EINVAL;
	}

	arfw_mem_util_iomem_read_block(
		driver->mmio_memory_bar,
		bar_layout->bar_memory_area.arp_update_addrs
			.ap_src_ring_rd_index_addr,
		bar_layout->arp_update_rd_index_cached, arp_update_rd_size);
	arfw_mem_util_iomem_read_block(
		driver->mmio_memory_bar,
		bar_layout->bar_memory_area.arp_update_addrs
			.ap_dst_ring_wr_index_addr,
		bar_layout->arp_update_wr_index_cached, arp_update_wr_size);
	return 0;
}

static int ar_pci_bar_set_ap_update_region(struct pci_dev *dev)
{
	ar_pci_driver_t *driver = pci_get_drvdata(dev);
	size_t ap_update_wr_size, ap_update_rd_size;
	ar_pci_bar_layout_t *bar_layout;

	AR_ASSERT(driver);
	// Nothing to update if queue indexes are placed in RAM.
	if (!driver->queue_indexes_in_bar)
		return 0;

	bar_layout = driver->deprecated_bar_layout;
	AR_ASSERT(bar_layout);
	ap_update_wr_size = driver->ap_src_data_ring_max * sizeof(uint16_t);
	ap_update_rd_size = driver->ap_dst_data_ring_max * sizeof(uint16_t);

	arfw_mem_util_iomem_write_block(
		driver->mmio_memory_bar,
		bar_layout->bar_memory_area.ap_update_addrs
			.ap_src_ring_wr_index_addr,
		bar_layout->ap_wr_index_cached, ap_update_wr_size);

	arfw_mem_util_iomem_write_block(
		driver->mmio_memory_bar,
		bar_layout->bar_memory_area.ap_update_addrs
			.ap_dst_ring_rd_index_addr,
		bar_layout->ap_rd_index_cached, ap_update_rd_size);

	return 0;
}

static int ar_pci_map_aperture_process_nack(uint8_t flags)
{
	int err;
	switch (flags) {
	case PCIE_APERTURE_MAP_REQ_FAIL_DUP:
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL, "Duplicate aperture request");
		err = -EEXIST;
		break;
	case PCIE_APERTURE_MAP_REQ_FAIL_INVALID_LEN:
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL, "Invalid aperture length");
		err = -E2BIG;
		break;
	case PCIE_APERTURE_MAP_REQ_FAIL_UNKNOWN:
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL, "Unknown aperture error");
		err = -EIO;
		break;
	default:
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL, "Unknown aperture error");
		err = -EIO;
		break;
	}

	return err;
}

static void ar_pci_map_aperture_cmd_reply(ar_pci_cmd_reply_promise_t *promise,
					  pcie_ctrl_resp_t *packet)
{
	int err;

	AR_ASSERT(promise);
	AR_ASSERT(packet);

	if (packet->common_header.msg_type != APERTURE_MAP_RSP) {
		AR_LOG_PCI_ERR(
			AR_LOG_WRITE_CTRL,
			"Response type is not APERTURE_MAP_RSP [msg_type: %d]",
			packet->common_header.msg_type);
		ar_future_complete(promise->base.future, -EINVAL);
	} else if (packet->common_header.flag) {
		err = ar_pci_map_aperture_process_nack(
			packet->common_header.flag);
		ar_future_complete(promise->base.future, err);
	} else {
		ar_future_complete(promise->base.future, 0);
	}
}

int ar_pci_map_aperture_cmd(struct pci_dev *dev, uint32_t timeout_ms)
{
	int err;
	unsigned long flags;
	ar_pci_driver_t *driver;
	ar_pci_cmd_reply_promise_t *promise;
	ar_future_t *future;
	uint16_t seq_num;
	pcie_ctrl_req_t packet;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver->aperture.dma_addr);
	AR_ASSERT(driver->aperture.size);
	AR_ASSERT(driver->aperture.id);
	seq_num = ar_pci_bar_next_cmd_sequence_number(dev);
	packet.aperture_map_req.common_header.msg_type = APERTURE_MAP_REQ;
	packet.aperture_map_req.common_header.seq_num = seq_num;
	packet.aperture_map_req.buf_addr = driver->aperture.dma_addr;
	packet.aperture_map_req.len = driver->aperture.size;
	packet.aperture_map_req.aperture_id = driver->aperture.id;

	future = ar_create_future(&dev->dev);
	if (!future) {
		err = -ENOMEM;
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
				   "Failed to allocate future (aperture msg)");
		goto error_no_future;
	}

	promise = devm_kzalloc(&dev->dev, sizeof(ar_pci_cmd_reply_promise_t),
			       GFP_KERNEL);
	if (!promise) {
		err = -ENOMEM;
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_WRITE_CTRL,
				   "Failed to allocate promise (aperture msg)");
		goto error_no_promise;
	}

	ar_future_set_promise(future, &promise->base);
	ar_future_set_timeout(future, timeout_ms);

	promise->base.lock = &driver->cmd_reply_lock;
	promise->seq_num = seq_num;
	promise->complete = ar_pci_map_aperture_cmd_reply;

	spin_lock_irqsave(promise->base.lock, flags);
	list_add_tail(&promise->base.list, &driver->cmd_reply_list);
	spin_unlock_irqrestore(promise->base.lock, flags);

	err = ar_pci_bar_submit_control_packet(dev, &packet, true);
	if (err) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_WRITE_CTRL,
			"Failed to send ARP info request (aperture msg) [err: %d]",
			err);
		goto error_no_submit;
	}

	return ar_future_wait(future);

error_no_submit:
	spin_lock_irqsave(promise->base.lock, flags);
	list_del(&promise->base.list);
	spin_unlock_irqrestore(promise->base.lock, flags);
	devm_kfree(&dev->dev, promise);
error_no_promise:
	devm_kfree(&dev->dev, future);
error_no_future:
	return err;
}

static int ar_pci_bar_enable_duty_cycle_process_nack(uint8_t flags)
{
	int err;
	switch (flags) {
	case AR_DUTY_CYCLE_REQ_FAIL:
		AR_LOG_PCI_ERR(AR_LOG_WRITE_CTRL,
			       "Duty cycle enablement failed");
		err = -EIO;
		break;
	default:
		AR_LOG_PCI_ERR(
			AR_LOG_WRITE_CTRL,
			"Duty cycle enablement failed with unknown error");
		err = -EIO;
		break;
	}

	return err;
}

static void
ar_pci_bar_enable_duty_cycle_reply(ar_pci_cmd_reply_promise_t *promise,
				   pcie_ctrl_resp_t *packet)
{
	int err;

	AR_ASSERT(promise);
	AR_ASSERT(packet);

	if (packet->common_header.msg_type != AR_DUTY_CYCLE_RSP) {
		AR_LOG_PCI_ERR(
			AR_LOG_WRITE_CTRL,
			"Response type is not AR_DUTY_CYCLE_RSP [msg_type: %d]",
			packet->common_header.msg_type);
		ar_future_complete(promise->base.future, -EINVAL);
	} else if (packet->common_header.flag) {
		err = ar_pci_bar_enable_duty_cycle_process_nack(
			packet->common_header.flag);
		ar_future_complete(promise->base.future, err);
	} else {
		ar_future_complete(promise->base.future, 0);
	}
}

int ar_pci_bar_enable_duty_cycle(struct pci_dev *dev, int freq_hz,
				 uint32_t timeout_ms)
{
	int err;
	unsigned long flags;
	ar_pci_driver_t *driver;
	ar_pci_cmd_reply_promise_t *promise;
	ar_future_t *future;
	uint16_t seq_num;
	pcie_ctrl_req_t packet;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	seq_num = ar_pci_bar_next_cmd_sequence_number(dev);
	packet.ar_duty_cycle_req.common_header.msg_type = AR_DUTY_CYCLE_REQ;
	packet.ar_duty_cycle_req.common_header.seq_num = seq_num;
	packet.ar_duty_cycle_req.duty_cycle_freq_millihz =
		freq_hz * 1000 /* millihz */;
	packet.ar_duty_cycle_req.duty_cycle_cmd =
		freq_hz ? AR_DUTY_CYCLE_CMD_ENABLE : AR_DUTY_CYCLE_CMD_DISABLE;

	// prevent multiple calls to enable/disable from racing with each other
	mutex_lock(&driver->duty_cycle.lock);

	if (freq_hz == driver->duty_cycle.freq_hz) {
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_WRITE_CTRL,
				    "Duty cycle already set to %d", freq_hz);
		mutex_unlock(&driver->duty_cycle.lock);
		return 0;
	}

	future = ar_create_future(&dev->dev);
	if (!future) {
		err = -ENOMEM;
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_WRITE_CTRL,
			"Failed to allocate future (duty cycle msg)");
		goto error_no_future;
	}

	promise = devm_kzalloc(&dev->dev, sizeof(ar_pci_cmd_reply_promise_t),
			       GFP_KERNEL);
	if (!promise) {
		err = -ENOMEM;
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_WRITE_CTRL,
			"Failed to allocate promise (duty cycle msg)");
		goto error_no_promise;
	}

	ar_future_set_promise(future, &promise->base);
	ar_future_set_timeout(future, timeout_ms);

	promise->base.lock = &driver->cmd_reply_lock;
	promise->seq_num = seq_num;
	promise->complete = ar_pci_bar_enable_duty_cycle_reply;

	spin_lock_irqsave(promise->base.lock, flags);
	list_add_tail(&promise->base.list, &driver->cmd_reply_list);
	spin_unlock_irqrestore(promise->base.lock, flags);

	/**
	 * If we are attempting to disable duty cycle, we need to ensure that doorbells are not missed.
	 * We do this by disabling the duty cycle first, then sending the request to FW.
	 */
	atomic_set_release(&driver->duty_cycle.enabled, 0);

	err = ar_pci_bar_submit_control_packet(dev, &packet, true);
	if (err) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_WRITE_CTRL,
			"Failed to send ARP info request (duty cycle msg) [err: %d]",
			err);
		goto error_no_submit;
	}

	err = ar_future_wait(future);
	if (!err) {
		driver->duty_cycle.freq_hz = freq_hz;
		if (freq_hz)
			atomic_set_release(&driver->duty_cycle.enabled, 1);
	} else if (!freq_hz) {
		// re-enable duty cycle if we failed to disable it
		atomic_set_release(&driver->duty_cycle.enabled, 1);
	}

	mutex_unlock(&driver->duty_cycle.lock);
	return err;

error_no_submit:
	spin_lock_irqsave(promise->base.lock, flags);
	list_del(&promise->base.list);
	spin_unlock_irqrestore(promise->base.lock, flags);
	devm_kfree(&dev->dev, promise);
error_no_promise:
	devm_kfree(&dev->dev, future);
error_no_future:
	mutex_unlock(&driver->duty_cycle.lock);
	return err;
}

int ar_pci_bar_disable_duty_cycle(struct pci_dev *dev, uint32_t timeout_ms)
{
	return ar_pci_bar_enable_duty_cycle(dev, 0, timeout_ms);
}
