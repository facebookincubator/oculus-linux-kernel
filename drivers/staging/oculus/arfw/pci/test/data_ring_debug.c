// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file data_ring_debug.c
 *
 * @brief AR PCI debug functions
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include "pci_debug.h"
#include "../ar_pci_bar.h"
#include "../data_ring.h"

static int ar_pci_debug_data_ring_list_print(ar_pci_driver_t *driver,
					     ar_pci_client_list_t *list,
					     char *buf)
{
	ar_client_queue_t *queue = NULL;
	int length = 0;
	int i = 0;

	list_for_each_entry(queue, &list->list_node, node) {
		uint16_t wr_index = arfw_mem_util_iomem_read_16(
			driver->mmio_memory_bar,
			queue->data_ring->wr_index_reg);
		uint16_t rd_index = arfw_mem_util_iomem_read_16(
			driver->mmio_memory_bar,
			queue->data_ring->rd_index_reg);

		length += sprintf(buf + length, "Queue %d\n", i);
		length += sprintf(buf + length, "\tSend Queue: %d\n",
				  queue->queue_params.queue_direction ==
					  AR_QUEUE_HLOS_TO_FW);
		length += sprintf(buf + length, "\tElement size: 0x%x\n",
				  queue->queue_params.element_size);
		length += sprintf(buf + length, "\tDepth: %u\n",
				  queue->queue_params.depth);
		length += sprintf(buf + length, "\tXROS endpoint: 0x%x\n",
				  queue->queue_params.hlos_endpoint_id);
		length += sprintf(buf + length, "\tFW endpoint: 0x%x\n",
				  queue->queue_params.fw_endpoint_id);
		length += sprintf(buf + length, "\tRD index: %u\n", rd_index);
		length += sprintf(buf + length, "\tWR index: %u\n", wr_index);

		i++;
	}

	return length;
}

#define DATA_RING_LIST_SHOW(direction)                                       \
	static ssize_t ar_pci_##direction##_ring_list_show(                  \
		struct device *dev, struct device_attribute *da, char *buf)  \
	{                                                                    \
		ar_pci_driver_t *driver = dev_get_drvdata(dev);              \
		ar_pci_client_list_t *list =                                 \
			&driver->client_##direction##_queues;                \
                                                                             \
		return ar_pci_debug_data_ring_list_print(driver, list, buf); \
	}                                                                    \
	static DEVICE_ATTR(direction##_ring_list, 0444,                      \
			   ar_pci_##direction##_ring_list_show, NULL)

DATA_RING_LIST_SHOW(send);
DATA_RING_LIST_SHOW(rcv);

static struct attribute *ar_data_ring_debug_attributes[] = {
	&dev_attr_send_ring_list.attr, &dev_attr_rcv_ring_list.attr, NULL
};

static const struct attribute_group ar_data_ring_debug_attr_group = {
	.name = "data_ring_debug",
	.attrs = ar_data_ring_debug_attributes,
};

int ar_pci_data_ring_sysfs_add(struct pci_dev *dev)
{
	return sysfs_create_group(&dev->dev.kobj,
				  &ar_data_ring_debug_attr_group);
}

void ar_pci_data_ring_sysfs_remove(struct pci_dev *dev)
{
	sysfs_remove_group(&dev->dev.kobj, &ar_data_ring_debug_attr_group);
}
