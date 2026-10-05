// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file pci_debug.c
 *
 * @brief AR PCI debug functions
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include "../ar_pci_bar.h"
#include "../data_ring.h"
#include "ar_future.h"
#include "pci_debug.h"
#include "debug_int.h"

static ssize_t ar_pci_debug_read_bar_store(struct device *dev,
					   struct device_attribute *da,
					   const char *buf, size_t count)
{
	int ret = 0;
	uint64_t offset;
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	ret = kstrtoull(buf, 16, &offset);
	if (ret) {
		dev_err(dev, "%s: Failed to parse input %d\n", __func__, ret);
		return ret;
	}

	if (offset >= driver->debug_data.bar_size) {
		dev_err(dev, "%s: Offset is not in bar region\n", __func__);
		return -EINVAL;
	}

	driver->debug_data.offset = offset;
	return count;
}

#define READ_BAR_BITS(bits)                                                              \
	static ssize_t ar_pci_debug_bar_read_bar_##bits##_show(                          \
		struct device *dev, struct device_attribute *da, char *buf)              \
	{                                                                                \
		uint64_t value;                                                          \
		ar_pci_driver_t *driver = dev_get_drvdata(dev);                          \
                                                                                         \
		if (driver->debug_data.bar_base == NULL) {                               \
			dev_err(dev, "%s: Invalid bar set\n", __func__);                 \
			return -EINVAL;                                                  \
		}                                                                        \
                                                                                         \
		if (driver->debug_data.offset % sizeof(uint##bits##_t) != 0) {           \
			dev_err(dev,                                                     \
				"%s: Stored offset (0x%zx) is not aligned to %d bits\n", \
				__func__, driver->debug_data.offset, bits);              \
			return -EINVAL;                                                  \
		}                                                                        \
                                                                                         \
		value = arfw_mem_util_iomem_read_##bits(                                 \
			driver->debug_data.bar_base,                                     \
			driver->debug_data.offset);                                      \
                                                                                         \
		return sprintf(buf, "%llx\n", value);                                    \
	}                                                                                \
	static DEVICE_ATTR(read_bar_##bits, 0644,                                        \
			   ar_pci_debug_bar_read_bar_##bits##_show,                      \
			   ar_pci_debug_read_bar_store)

READ_BAR_BITS(8);
READ_BAR_BITS(16);
READ_BAR_BITS(32);
READ_BAR_BITS(64);

#define WRITE_BAR_BITS(bits)                                                       \
	static ssize_t ar_pci_debug_write_bar_##bits##_store(                      \
		struct device *dev, struct device_attribute *da,                   \
		const char *buf, size_t count)                                     \
	{                                                                          \
		int ret = 0;                                                       \
		uint64_t offset, value;                                            \
		ar_pci_driver_t *driver = dev_get_drvdata(dev);                    \
                                                                                   \
		if (driver->debug_data.bar_base == NULL) {                         \
			dev_err(dev, "%s: Invalid bar set\n", __func__);           \
			return -EINVAL;                                            \
		}                                                                  \
                                                                                   \
		ret = sscanf(buf, "%llx %llx", &offset, &value);                   \
		if (ret != 2) {                                                    \
			dev_err(dev, "%s: Failed to parse input\n", __func__);     \
			return -EINVAL;                                            \
		}                                                                  \
                                                                                   \
		if (offset >=                                                      \
		    driver->debug_data.bar_size - sizeof(uint##bits##_t)) {        \
			dev_err(dev,                                               \
				"%s: Requested offset (%llx) not in bar region\n", \
				__func__, offset);                                 \
			return -EINVAL;                                            \
		}                                                                  \
                                                                                   \
		driver->debug_data.offset = offset;                                \
                                                                                   \
		arfw_mem_util_iomem_write_##bits(driver->debug_data.bar_base,      \
						 offset,                           \
						 (uint##bits##_t)value);           \
                                                                                   \
		return count;                                                      \
	}                                                                          \
	static DEVICE_ATTR(write_bar_##bits, 0200, NULL,                           \
			   ar_pci_debug_write_bar_##bits##_store)

WRITE_BAR_BITS(8);
WRITE_BAR_BITS(16);
WRITE_BAR_BITS(32);
WRITE_BAR_BITS(64);

static ssize_t ar_pci_debug_mmio_memory_bar_raw_dump_show(
	struct device *dev, struct device_attribute *da, char *buf)
{
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	print_hex_dump(KERN_INFO, "MMIO Memory BAR:", DUMP_PREFIX_OFFSET, 16, 1,
		       driver->mmio_memory_bar, driver->mmio_memory_bar_size,
		       true);

	return 0;
}

static ssize_t ar_pci_debug_mmio_interrupt_bar_raw_dump_show(
	struct device *dev, struct device_attribute *da, char *buf)
{
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	print_hex_dump(KERN_INFO, "MMIO Interrupt BAR: ", DUMP_PREFIX_OFFSET,
		       16, 1, driver->mmio_interrupt_bar,
		       driver->mmio_interrupt_bar_size, true);

	return 0;
}

static ssize_t ar_pci_debug_ring_doorbell_store(struct device *dev,
						struct device_attribute *da,
						const char *buf, size_t count)
{
	int ret = 0;
	unsigned int value;
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(dev, "%s: Failed to parse input %d\n", __func__, ret);
		return ret;
	}

	if (value) {
		ret = ar_pci_bar_ring_doorbell(driver->dev);
		if (ret) {
			dev_err(dev, "%s: Failed to ring doorbell %d\n",
				__func__, ret);
			return ret;
		}
		dev_info(dev, "%s: Rang doorbell\n", __func__);
	}

	return count;
}

// Sets the bar to be used by the sysfs read and write nodes
static int ar_pci_set_bar(struct device *dev, int bar)
{
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	if (bar == driver->mmio_memory_bar_region) {
		driver->debug_data.bar_base = driver->mmio_memory_bar;
		driver->debug_data.bar_size = driver->mmio_memory_bar_size;
	} else if (bar == driver->mmio_interrupt_bar_region) {
		driver->debug_data.bar_base = driver->mmio_interrupt_bar;
		driver->debug_data.bar_size = driver->mmio_interrupt_bar_size;
	} else {
		dev_err(dev, "%s: Error unknown bar %d\n", __func__, bar);
		return -EINVAL;
	}

	// Reset the offset to avoid accessing beyond the end of the bar
	driver->debug_data.offset = 0;

	return 0;
}

static ssize_t ar_pci_set_bar_show(struct device *dev,
				   struct device_attribute *da, char *buf)
{
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	if (driver->debug_data.bar_base == driver->mmio_interrupt_bar)
		return sprintf(buf, "MMIO Interrupt bar (%d) size: 0x%lx\n",
			       driver->mmio_interrupt_bar_region,
			       driver->debug_data.bar_size);

	if (driver->debug_data.bar_base == driver->mmio_memory_bar)
		return sprintf(buf, "MMIO Memory bar (%d) size: 0x%lx\n",
			       driver->mmio_memory_bar_region,
			       driver->debug_data.bar_size);

	dev_err(dev, "%s: Error unknown bar\n", __func__);
	return -EINVAL;
}

static ssize_t ar_pci_set_bar_store(struct device *dev,
				    struct device_attribute *da,
				    const char *buf, size_t count)
{
	int ret = 0;
	int bar;

	ret = kstrtouint(buf, 10, &bar);
	if (ret) {
		dev_err(dev, "%s: Failed to parse input %d\n", __func__, ret);
		return ret;
	}

	ret = ar_pci_set_bar(dev, bar);
	if (ret) {
		dev_err(dev, "%s: Failed to set bar(%d) %d\n", __func__, bar,
			ret);
		return ret;
	}

	return count;
}

static DEVICE_ATTR(set_bar, 0644, ar_pci_set_bar_show, ar_pci_set_bar_store);
static DEVICE_ATTR(mmio_memory_bar_raw_dump, 0444,
		   ar_pci_debug_mmio_memory_bar_raw_dump_show, NULL);
static DEVICE_ATTR(mmio_interrupt_bar_raw_dump, 0444,
		   ar_pci_debug_mmio_interrupt_bar_raw_dump_show, NULL);
static DEVICE_ATTR(ring_doorbell, 0200, NULL, ar_pci_debug_ring_doorbell_store);

// Only create the FW ping node if the FW protocal is brought up
static void ar_pci_ping_reply(ar_pci_cmd_reply_promise_t *promise,
			      pcie_ctrl_resp_t *packet)
{
	AR_ASSERT(promise);
	AR_ASSERT(packet);

	if (packet->common_header.msg_type != CTRL_PING_RSP) {
		kfree(packet);
		ar_future_complete(promise->base.future, -EINVAL);
	} else {
		ar_future_complete_data(promise->base.future, packet);
	}
}

static ar_future_t *ar_pci_ping_fw(struct pci_dev *dev)
{
	int ret;
	ar_pci_driver_t *driver;
	unsigned long flags;
	uint16_t seq_num;
	ar_pci_cmd_reply_promise_t *promise;
	ar_future_t *future;
	pcie_ctrl_req_t packet;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	seq_num = ar_pci_bar_next_cmd_sequence_number(dev);
	packet.ping.common_header.msg_type = CTRL_PING_REQ;
	packet.ping.common_header.seq_num = seq_num;

	future = ar_create_future(&dev->dev);
	if (future == NULL) {
		dev_err(&dev->dev, "%s: Failed to create future\n", __func__);
		return NULL;
	}

	promise = devm_kzalloc(&dev->dev, sizeof(ar_pci_cmd_reply_promise_t),
			       GFP_KERNEL);
	if (promise == NULL) {
		dev_err(&dev->dev, "%s: Failed to allocate promise\n",
			__func__);
		kfree(future);
		return NULL;
	}

	promise->seq_num = seq_num;
	promise->queue = NULL;
	promise->complete = ar_pci_ping_reply;
	promise->base.lock = &driver->cmd_reply_lock;

	ar_future_set_promise(future, &promise->base);
	ar_future_set_timeout(future, 1000);

	spin_lock_irqsave(promise->base.lock, flags);
	list_add_tail(&promise->base.list, &driver->cmd_reply_list);
	spin_unlock_irqrestore(promise->base.lock, flags);

	ret = ar_pci_bar_submit_control_packet(dev, &packet, true);
	if (ret) {
		dev_err(&dev->dev, "%s: Failed to send ping %d\n", __func__,
			ret);
		spin_lock_irqsave(promise->base.lock, flags);
		list_del(&promise->base.list);
		spin_unlock_irqrestore(promise->base.lock, flags);
		devm_kfree(&dev->dev, future);
		devm_kfree(&dev->dev, promise);
		return NULL;
	}

	return future;
}

static ssize_t ar_pci_debug_ping_show(struct device *dev,
				      struct device_attribute *da, char *buf)
{
	ar_pci_driver_t *driver = dev_get_drvdata(dev);
	ar_future_t *future = ar_pci_ping_fw(driver->dev);
	pcie_ctrl_resp_t *packet = NULL;
	int ret = 0;

	if (future == NULL) {
		dev_err(dev, "%s: Failed to send ping\n", __func__);
		return -EIO;
	}

	ret = ar_future_wait_data(future, (void **)&packet);
	if (ret || packet == NULL) {
		dev_err(dev, "%s: Ping failed %d\n", __func__, ret);
		return ret ? ret : -EIO;
	}

	kfree(packet);
	return sprintf(buf, "Pong!!!\n");
}

static DEVICE_ATTR(ping, 0444, ar_pci_debug_ping_show, NULL);

static struct attribute *ar_debug_attributes[] = {
	&dev_attr_read_bar_8.attr,
	&dev_attr_read_bar_16.attr,
	&dev_attr_read_bar_32.attr,
	&dev_attr_read_bar_64.attr,
	&dev_attr_write_bar_8.attr,
	&dev_attr_write_bar_16.attr,
	&dev_attr_write_bar_32.attr,
	&dev_attr_write_bar_64.attr,
	&dev_attr_set_bar.attr,
	&dev_attr_mmio_memory_bar_raw_dump.attr,
	&dev_attr_mmio_interrupt_bar_raw_dump.attr,
	&dev_attr_ring_doorbell.attr,
	&dev_attr_ping.attr,
	NULL
};

static const struct attribute_group ar_debug_attr_group = {
	.name = "debug",
	.attrs = ar_debug_attributes,
};

int ar_pci_debug_init(struct pci_dev *dev)
{
	int ret = 0;
	ar_pci_driver_t *driver = dev_get_drvdata(&dev->dev);

	ar_pci_set_bar(&dev->dev, driver->mmio_memory_bar_region);

	ret = sysfs_create_group(&dev->dev.kobj, &ar_debug_attr_group);
	if (ret) {
		dev_err(&dev->dev,
			"%s: Failed to create debug sysfs group %d\n", __func__,
			ret);
		return ret;
	}

	ret = ar_pci_control_ring_sysfs_add(dev);
	if (ret) {
		dev_err(&dev->dev,
			"%s: Failed to create control ring debug sysfs group %d\n",
			__func__, ret);
		goto error_control_ring;
	}

	ret = ar_pci_data_ring_sysfs_add(dev);
	if (ret) {
		dev_err(&dev->dev,
			"%s: Failed to create data ring debug sysfs group %d\n",
			__func__, ret);
		goto error_data_ring;
	}

	return 0;

error_data_ring:
	ar_pci_control_ring_sysfs_remove(dev);
error_control_ring:
	sysfs_remove_group(&dev->dev.kobj, &ar_debug_attr_group);
	return ret;
}

void ar_pci_debug_remove(struct pci_dev *dev)
{
	ar_pci_data_ring_sysfs_remove(dev);
	ar_pci_control_ring_sysfs_remove(dev);
	sysfs_remove_group(&dev->dev.kobj, &ar_debug_attr_group);
}
