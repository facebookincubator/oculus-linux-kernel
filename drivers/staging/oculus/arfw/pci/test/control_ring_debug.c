// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file pci_debug.h
 *
 * @brief AR PCI debug header
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include "pci_debug.h"
#include "../ar_pci_int.h"
#include "../ar_pci_bar.h"
#include "../control_rings.h"

static int
ar_pci_debug_print_cached_ring_status(ar_pci_control_ring_context_t *ring,
				      char *buf)
{
	int length = 0;

	length += sprintf(buf + length, "dma_address:       0x%p\n",
			  (void *)ring->dma_handle);
	length += sprintf(buf + length, "virtual addr:      0x%p\n", ring->va);
	length += sprintf(buf + length, "total size:        0x%lx\n",
			  ring->va_size);
	length += sprintf(buf + length, "receive_ring:      %d\n",
			  ring->receive_ring);
	length += sprintf(buf + length, "cached_ring_index: %u\n",
			  ring->cached_ring_index);
	length += sprintf(buf + length, "rd_index_reg:      0x%x\n",
			  ring->rd_index_reg);
	length += sprintf(buf + length, "wr_index_reg:      0x%x\n",
			  ring->wr_index_reg);
	length += sprintf(buf + length, "local_rd_index:    %u\n",
			  ring->local_rd_index);
	length += sprintf(buf + length, "local_wr_index:    %u\n",
			  ring->local_wr_index);

	return length;
}

#define CONTROL_RING_CACHED_STATUS_SHOW(ring_name)                          \
	static ssize_t ar_pci_##ring_name##_cached_show(                    \
		struct device *dev, struct device_attribute *da, char *buf) \
	{                                                                   \
		ar_pci_driver_t *driver = dev_get_drvdata(dev);             \
		ar_pci_control_ring_context_t *ring = driver->ring_name;    \
                                                                            \
		if (ring == NULL) {                                         \
			dev_err(dev, "%s: Null ring\n", __func__);          \
			return -EINVAL;                                     \
		}                                                           \
                                                                            \
		return ar_pci_debug_print_cached_ring_status(ring, buf);    \
	}                                                                   \
	static DEVICE_ATTR(ring_name##_cached, 0444,                        \
			   ar_pci_##ring_name##_cached_show, NULL)

CONTROL_RING_CACHED_STATUS_SHOW(ap_src_control_ring);
CONTROL_RING_CACHED_STATUS_SHOW(ap_dst_control_ring);
CONTROL_RING_CACHED_STATUS_SHOW(ap_src_buffer_control_ring);

/**
 * Warning, this macro takes advantage of the identical memory layout
 * between pcie_bar_status_block_area_t and pcie_bar_status_block_area_v2_t
 * for the control ring indexes.
 */
#define CONTROL_RING_SHARED_STATUS_SHOW(ring_name, struct_name)                \
	static ssize_t ar_pci_##ring_name##_shared_show(                       \
		struct device *dev, struct device_attribute *da, char *buf)    \
	{                                                                      \
		ar_pci_driver_t *driver = dev_get_drvdata(dev);                \
		ar_pci_control_ring_context_t *ring = driver->ring_name;       \
                                                                               \
		int length = 0;                                                \
		uint64_t ring_pa;                                              \
		uint8_t head_room;                                             \
		uint8_t tail_room;                                             \
		uint16_t rd_index;                                             \
		uint16_t wr_index;                                             \
                                                                               \
		if (ring == NULL) {                                            \
			dev_err(dev, "%s: Null ring\n", __func__);             \
			return -EINVAL;                                        \
		}                                                              \
                                                                               \
		ring_pa = arfw_mem_util_iomem_read_64(                         \
			driver->mmio_memory_bar,                               \
			AR_BAR_STATUS_BLOCK_V2_ADDR(driver,                    \
						    struct_name##_addr));      \
                                                                               \
		head_room = arfw_mem_util_iomem_read_8(                        \
			driver->mmio_memory_bar,                               \
			AR_BAR_STATUS_BLOCK_V2_ADDR(driver,                    \
						    struct_name##_head_room)); \
                                                                               \
		tail_room = arfw_mem_util_iomem_read_8(                        \
			driver->mmio_memory_bar,                               \
			AR_BAR_STATUS_BLOCK_V2_ADDR(driver,                    \
						    struct_name##_tail_room)); \
                                                                               \
		rd_index = arfw_mem_util_iomem_read_16(                        \
			driver->mmio_memory_bar,                               \
			AR_BAR_STATUS_BLOCK_V2_ADDR(driver,                    \
						    struct_name##_rd_index));  \
                                                                               \
		wr_index = arfw_mem_util_iomem_read_16(                        \
			driver->mmio_memory_bar,                               \
			AR_BAR_STATUS_BLOCK_V2_ADDR(driver,                    \
						    struct_name##_wr_index));  \
                                                                               \
		length +=                                                      \
			sprintf(buf + length, "ring pa:   0x%llx\n", ring_pa); \
		length +=                                                      \
			sprintf(buf + length, "head_room: 0x%x\n", head_room); \
		length +=                                                      \
			sprintf(buf + length, "tail_room: 0x%x\n", tail_room); \
		length += sprintf(buf + length, "rd_index:  %u\n", rd_index);  \
		length += sprintf(buf + length, "wr_index:  %u\n", wr_index);  \
                                                                               \
		return length;                                                 \
	}                                                                      \
	static DEVICE_ATTR(ring_name##_shared, 0444,                           \
			   ar_pci_##ring_name##_shared_show, NULL)

CONTROL_RING_SHARED_STATUS_SHOW(ap_src_control_ring, ap_src_ctrl);
CONTROL_RING_SHARED_STATUS_SHOW(ap_dst_control_ring, ap_dst_ctrl);
CONTROL_RING_SHARED_STATUS_SHOW(ap_src_buffer_control_ring, ap_src_buf);

static struct attribute *ar_control_ring_debug_attributes[] = {
	&dev_attr_ap_src_control_ring_cached.attr,
	&dev_attr_ap_dst_control_ring_cached.attr,
	&dev_attr_ap_src_buffer_control_ring_cached.attr,
	&dev_attr_ap_src_control_ring_shared.attr,
	&dev_attr_ap_dst_control_ring_shared.attr,
	&dev_attr_ap_src_buffer_control_ring_shared.attr,
	NULL
};

static const struct attribute_group ar_control_ring_debug_attr_group = {
	.name = "control_ring_debug",
	.attrs = ar_control_ring_debug_attributes,
};

int ar_pci_control_ring_sysfs_add(struct pci_dev *dev)
{
	return sysfs_create_group(&dev->dev.kobj,
				  &ar_control_ring_debug_attr_group);
}

void ar_pci_control_ring_sysfs_remove(struct pci_dev *dev)
{
	sysfs_remove_group(&dev->dev.kobj, &ar_control_ring_debug_attr_group);
}
