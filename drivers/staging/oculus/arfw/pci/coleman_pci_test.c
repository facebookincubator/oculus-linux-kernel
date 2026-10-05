// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file coleman_pci_test.c
 *
 * @brief PCIe driver for testing firmware PCIe driver.
 *
 * @details This driver is used to test the firmware PCIe driver. At device
 *          open time, the driver allocates a DMA buffer no larger than
 *          COLEMAN_DMA_BUFFER_SIZE_MAX bytes that is present for the life of
 *          the open and freed at device close time.  The driver supports
 *          mmap'ing the DMA buffer to the user process.
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "coleman_pci_test.h"
#include "fb_pcie_coleman_shared.h" // For PCI/PCIE IDs
#include "ar_pci_int.h"
#include "arfw_mem_util.h"

#define MISC_DYNAMIC_MINOR 255
#define DEVICE_NAME "Coleman_PCI"
#define DEVICE_NAME_LEN 17

#define COLEMAN_DOORBELL_MASK 0xFFFFFFFE
#define COLEMAN_DOORBELL_GRP0_SLICE0_OFFSET 0L
#define DOORBELL_RING_VALUE 0x1

#define COLEMAN_MSI_MIN_VECTORS 1
#define COLEMAN_MSI_MAX_VECTORS 2 // 2 vectors for doorbell, 1 for pcievalidator
#define COLEMAN_BAR_COUNT 6
#define COLEMAN_DMA_SIZE 0x4000
#define COLEMAN_DMA_CHECK_SIZE 0x20
#define COLEMAN_DMA_SOURCE 0x80000000
#define COLEMAN_DMA_START 1
#define COLEMAN_DMA_ACK 2
#define COLEMAN_DMA_CLEAR 0
#define COLEMAN_DMA_BUFFER_SIZE_MAX (64 * 1024 * 1024) // 64MB
#define COLEMAN_APERTURE_BUFFER_SIZE_BYTES (PAGE_SIZE)

#define PCI_FUNCTION_MASK 0x7
#define PCI_DEVICE_SHIFT 3

#define PCI_IOMAP_BAR_MASK 0x3F

#define TEST_BUFFER_SIZE PAGE_SIZE

#define ALFRED_DOORBELL_BAR 0
#define DEFAULT_PCIE_VALIDATOR_BAR_NUMBER 1

typedef enum uint32_t {
	E_PINNED_BUFFER_DMA_SELECT = 0x0,
	E_PINNED_BUFFER_APERTURE_SELECT,

	// Keep Last
	E_PINNED_BUFFER_SELECT_COUNT
} pinned_buffer_map_select_t;

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple Driver for Meta Co-processor");

static struct pci_device_id coleman_pci_ids[] = {
	// For HAPS
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F0) },
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F1) },
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F2) },
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F3) },
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F4) },
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F5) },
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F6) },
	{ PCI_DEVICE(META_PCI_SIG_VENDOR_ID, COLEMAN_PCIE_DEVICE_ID_F7) },

	// For FERM
	{ PCI_DEVICE(FERM_COLEMAN_PCI_FAKE_VENDOR_ID,
		     FERM_COLEMAN_PCIE_DEVICE_ID) },

	// Keep Last
	{ /* end: zero */ }
};
MODULE_DEVICE_TABLE(pci, coleman_pci_ids);

/// Interrupt number for signalling that pcievalidator tests are done. Set during intialization.
static uint32_t pcie_validator_test_msi;

struct bar_offsets {
	uint32_t read; /*!< Offset into the BAR where reads should occur */
	uint32_t write; /*!< Offset into the BAR where writes should occur */

	uint32_t control_request; /*!< Offset into the BAR where control requests should be written */
	uint32_t control_response; /*!< Offset into the BAR where control responses should be read */
};

struct dma_buffer {
	dma_addr_t bus_address; /*!< Bus address to use for DMA access */
	void *virtual_address; /*!< Virtual address of allocated DMA buffer */
	size_t size; /*!< Size of allocated buffer */
};

struct pcie_validator_data {
	uint32_t bar_number; /*!< BAR number that hosts the pcievalidator interface*/
	struct bar_offsets bar_offsets; /*!< Offsets into the BARs */
	struct dma_buffer dma_buffer; /*!< DMA buffer for transfers */
	struct dma_buffer aperture_buffer; /*!< Memory buffer for Aperture use */
	uint32_t pinned_buffer_map_select; /*!< Selects which buffer to map to the host application for the next mmap() call */
};

struct coleman_pci_drvdata {
	void __iomem *ptr_bar[6];
	struct miscdevice misc_device;
	struct pci_dev *coleman_pci_dev;
	int msi_vector_count; /*!< Number of MSI vectors allocated */

	uint16_t allocated_irqs
		[COLEMAN_MSI_MAX_VECTORS]; /*!< IRQ numbers for MSI vectors */
	uint16_t received_irq_numbers
		[COLEMAN_MSI_MAX_VECTORS]; /*!< IRQ numbers Received */
	uint16_t received_irq_count
		[COLEMAN_MSI_MAX_VECTORS]; /*!< Number of IRQs received per MSI */
	uint16_t pcie_validator_test_done_flag; /// Flag raised when pcievalidator MSI is received. Device reads this via IOCTL.

	/* Test parameters.  Maybe give this its own sub-struct: */
	dma_addr_t io_dma_addr;
	uint8_t io_buffer[TEST_BUFFER_SIZE];
	/* TODO(T174915710): Delete io_buffer, allocate io_vaddr
	 * from the start, and write to io_vaddr directly.
	 */
	void *io_vaddr;
	size_t io_len;
	int io_perform_param;

	struct pcie_validator_data
		pcie_validator; /*!< Driver data for implementing pcievalidator. */
};

struct pci_dev *g_coleman_pci_dev;

struct coleman_dma_regs {
	uint32_t control;
	uint32_t size;
	uint64_t src;
	uint64_t dst;
	uint32_t tx_size;
	uint32_t padding;
};

static int test_dma_read(struct device *dev);
static int test_dma_write(struct device *dev);
static int test_dma_free(struct device *dev);

enum {
	IO_DMA_INIT = -1,
	IO_DMA_READ,
	IO_DMA_WRITE,
	IO_RESET,
	IO_LAST,
};

static inline struct coleman_pci_drvdata *device_to_coleman(struct device *dev)
{
	return dev_get_drvdata(dev);
}

static ssize_t io_dma_addr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);

	return sysfs_emit(buf, "0x%016llx\n", drvdata->io_dma_addr);
}
static struct device_attribute io_dma_addr_dev_attr = __ATTR_RO(io_dma_addr);

static ssize_t io_buffer_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);
	int i;

	char *vaddr = drvdata->io_vaddr;
	int ret = sysfs_emit(buf, "io_buffer = \"%s\"\n",
			     (char *)drvdata->io_buffer);
	ret += sysfs_emit_at(buf, ret, "io_buffer:\n");

	for (i = 0; i < drvdata->io_len; i++)
		ret += sysfs_emit_at(buf, ret, "%02x ", vaddr[i]);

	ret += sysfs_emit_at(buf, ret, "\n");

	return ret;
}

static ssize_t io_buffer_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);

	if (count >= TEST_BUFFER_SIZE)
		count = TEST_BUFFER_SIZE - 1;
	memcpy(drvdata->io_buffer, buf, count);
	((char *)drvdata->io_buffer)[TEST_BUFFER_SIZE - 1] = '\0';
	drvdata->io_len = count;

	return count;
}
static struct device_attribute io_buffer_dev_attr = __ATTR_RW(io_buffer);

static ssize_t dma_buffer_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);
	struct dma_buffer *dma_buffer;
	int ret;

	dma_buffer = &drvdata->pcie_validator.dma_buffer;
	if (!dma_buffer)
		return 0;

	ret = sysfs_emit(buf, "dma_buffer = %s\n",
			 (char *)dma_buffer->virtual_address);

	return ret;
}

static ssize_t dma_buffer_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);
	struct dma_buffer *dma_buffer;

	dma_buffer = &drvdata->pcie_validator.dma_buffer;
	if (!dma_buffer)
		return 0;

	if (count >= TEST_BUFFER_SIZE)
		count = TEST_BUFFER_SIZE - 1;

	memcpy(dma_buffer->virtual_address, buf, count);
	((char *)dma_buffer->virtual_address)[count] = '\0';

	return count;
}
static struct device_attribute dma_buffer_dev_attr = __ATTR_RW(dma_buffer);

static ssize_t io_perform_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);
	int op = drvdata->io_perform_param;
	int ret;

	if ((op < IO_DMA_READ) || (op >= IO_LAST))
		ret = sysfs_emit(buf,
				 "Operation: %d, not defined, use:\n"
				 "\t%d - dma read\n"
				 "\t%d - dma write\n"
				 "\t%d - reset the operation\n",
				 op, IO_DMA_READ, IO_DMA_WRITE, IO_RESET);
	else if (op == IO_RESET)
		ret = sysfs_emit(buf, "Operation: %d (reset)\n", op);
	else
		ret = sysfs_emit(buf,
				 "Operation: %d (%s)\n"
				 "\tdma addr = 0x%016llx\n"
				 "\tbuffer = \"%s\"\n"
				 "\tlength = %zu\n",
				 op,
				 (op == IO_DMA_READ) ? "dma read" : "dma write",
				 drvdata->io_dma_addr, drvdata->io_buffer,
				 drvdata->io_len);

	return ret;
}

static ssize_t io_perform_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);

	int op;
	int ret;

	ret = kstrtoint(buf, 10, &op);
	if (ret != 0) {
		dev_err(&g_coleman_pci_dev->dev,
			"%s:%d: wrong value for the operation.\n", __func__,
			__LINE__);
		return -EINVAL;
	}

	switch (op) {
	case IO_DMA_READ:
		test_dma_read(&g_coleman_pci_dev->dev);
		break;
	case IO_DMA_WRITE:
		test_dma_write(&g_coleman_pci_dev->dev);
		break;
	case IO_RESET:
		test_dma_free(&g_coleman_pci_dev->dev);
		break;
	}

	drvdata->io_perform_param = op;
	return count;
}
static struct device_attribute io_perform_dev_attr = __ATTR_RW(io_perform);

static int test_dma_read(struct device *dev)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);

	dev_info(dev, "%s:%d\n", __func__, __LINE__);
	if (drvdata->io_len <= 0) {
		dev_err(dev,
			"%s:%d: io_len is less or equal than 0, can't read to such buffer\n",
			__func__, __LINE__);
		return -1;
	}
	if (drvdata->io_dma_addr)
		test_dma_free(dev);

	drvdata->io_vaddr = dma_alloc_coherent(dev, drvdata->io_len,
					       &drvdata->io_dma_addr, GFP_DMA);
	dev_info(
		dev,
		"%s:%d: prepared successfully, io_vaddr = 0x%16p, io_dma_addr = 0x%16llx, size = %zu\n",
		__func__, __LINE__, drvdata->io_vaddr,
		(unsigned long long)drvdata->io_dma_addr, drvdata->io_len);

	return 0;
}

static int test_dma_write(struct device *dev)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);

	dev_info(dev, "%s:%d\n", __func__, __LINE__);
	drvdata->io_len = strlen(drvdata->io_buffer);
	if (drvdata->io_len <= 0) {
		dev_err(dev, "%s:%d: io_len is less or equal than 0\n",
			__func__, __LINE__);
		return -1;
	}

	if (drvdata->io_dma_addr)
		test_dma_free(dev);

	drvdata->io_vaddr = dma_alloc_coherent(dev, drvdata->io_len,
					       &drvdata->io_dma_addr, GFP_DMA);
	memcpy(drvdata->io_vaddr, drvdata->io_buffer, drvdata->io_len);
	dev_info(
		dev,
		"%s:%d: prepared successfully, io_vaddr = 0x%16p, io_dma_addr = 0x%16llx, size = %zu\n",
		__func__, __LINE__, drvdata->io_vaddr,
		(unsigned long long)drvdata->io_dma_addr, drvdata->io_len);

	return 0;
}

static int test_dma_free(struct device *dev)
{
	struct coleman_pci_drvdata *drvdata = device_to_coleman(dev);

	dev_info(dev, "%s:%d\n", __func__, __LINE__);
	dma_free_coherent(dev, drvdata->io_len, drvdata->io_vaddr,
			  drvdata->io_dma_addr);
	drvdata->io_dma_addr = 0;
	dev_info(dev, "%s:%d: success\n", __func__, __LINE__);

	return 0;
}

static irqreturn_t coleman_pci_interrupt_handler(int irq, void *dev_id)
{
	struct pci_dev *dev = (struct pci_dev *)dev_id;
	struct coleman_pci_drvdata *data = device_to_coleman(&dev->dev);

	volatile uint32_t *doorbell_regs =
		(volatile uint32_t *)data->ptr_bar[ALFRED_DOORBELL_BAR];
	uint32_t doorbell_value = 0;

	dev_info(&dev->dev, "PCI IRQ %d\n", irq);

	// Track received MSIs - Use COLEMAN_MSI_MAX_VECTORS to check all allocated MSIs
	for (int i = 0; i < data->msi_vector_count; i++) {
		if (irq == data->allocated_irqs[i]) {
			data->received_irq_numbers[i] = irq;
			data->received_irq_count[i] += 1;
		}
	}
	if (irq == data->allocated_irqs[0]) {
		data->pcie_validator_test_done_flag = 1;
	}

	doorbell_value =
		readl(doorbell_regs + COLEMAN_DOORBELL_GRP0_SLICE0_OFFSET);
	doorbell_value = doorbell_value & COLEMAN_DOORBELL_MASK;
	writel(doorbell_value,
	       doorbell_regs + COLEMAN_DOORBELL_GRP0_SLICE0_OFFSET);

	return IRQ_HANDLED;
}

/**
 * @brief Function is called, when a PCI device is opened
 *		  Proof of concept for DMA transfer to Coleman
 *
 * @param inode   pointer to the PCI device file
 * @param filp    pointer to the open file
 *
 * @return      0
 *
 */
static int coleman_pci_misc_device_open(struct inode *inode, struct file *filp)
{
	size_t try_alloc_size;
	struct dma_buffer *dma_buffer;

	struct coleman_pci_drvdata *data = container_of(
		filp->private_data, struct coleman_pci_drvdata, misc_device);

	dev_info(&data->coleman_pci_dev->dev, "Misc device open\n");

	//
	// Allocate host memory for access by device.
	//

	// Allocate Aperture buffer that exists for the life of the open
	dma_buffer = &data->pcie_validator.aperture_buffer;

	dma_buffer->size = COLEMAN_APERTURE_BUFFER_SIZE_BYTES;
	dma_buffer->virtual_address = dma_alloc_coherent(
		&data->coleman_pci_dev->dev, dma_buffer->size,
		&dma_buffer->bus_address, GFP_USER);

	if (dma_buffer->virtual_address == NULL)
		return -ENOMEM;

	dev_info(
		&data->coleman_pci_dev->dev,
		"Allocated Aperture buffer of size %zX at bus address 0x%llX, virtual address %p\n",
		dma_buffer->size, dma_buffer->bus_address,
		dma_buffer->virtual_address);

	// Allocate DMA buffer that exists for life of the open
	try_alloc_size = COLEMAN_DMA_BUFFER_SIZE_MAX;
	dma_buffer = &data->pcie_validator.dma_buffer;

	do {
		dma_buffer->size = try_alloc_size;
		dma_buffer->virtual_address = dma_alloc_coherent(
			&data->coleman_pci_dev->dev, dma_buffer->size,
			&dma_buffer->bus_address, GFP_USER);
		try_alloc_size /= 2;
	} while (dma_buffer->virtual_address == NULL);
	dev_info(
		&data->coleman_pci_dev->dev,
		"Allocated DMA buffer of size %zX at bus address 0x%llX, virtual address %p\n",
		dma_buffer->size, dma_buffer->bus_address,
		dma_buffer->virtual_address);

	return 0;
}

static int coleman_pci_misc_device_close(struct inode *inode, struct file *filp)
{
	struct coleman_pci_drvdata *data = container_of(
		filp->private_data, struct coleman_pci_drvdata, misc_device);

	// Deallocate the DMA buffer that was allocated in open
	struct dma_buffer *dma_buffer = &data->pcie_validator.dma_buffer;

	dev_info(&data->coleman_pci_dev->dev, "Misc device close\n");

	dev_info(&data->coleman_pci_dev->dev,
		 "Deallocating DMA buffer at 0x%llX\n",
		 dma_buffer->bus_address);
	dma_free_coherent(&data->coleman_pci_dev->dev, dma_buffer->size,
			  dma_buffer->virtual_address, dma_buffer->bus_address);
	memset(dma_buffer, 0, sizeof(*dma_buffer));

	// Deallocate the Aperture buffer that was allocated in open
	dma_buffer = &data->pcie_validator.aperture_buffer;
	dev_info(&data->coleman_pci_dev->dev,
		 "Deallocating Aperture buffer at 0x%llX\n",
		 dma_buffer->bus_address);
	dma_free_coherent(&data->coleman_pci_dev->dev, dma_buffer->size,
			  dma_buffer->virtual_address, dma_buffer->bus_address);
	memset(dma_buffer, 0, sizeof(*dma_buffer));

	return 0;
}

static long coleman_pci_misc_device_ioctl(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	int status = 0;
	struct coleman_pci_drvdata *data = container_of(
		filp->private_data, struct coleman_pci_drvdata, misc_device);

	dev_info(&data->coleman_pci_dev->dev, "ioctl call, 0x%X, 0x%lX\n", cmd,
		 arg);

	// Validate the request
	if (_IOC_TYPE(cmd) != PCIE_VALIDATOR_IOC_MAGIC) {
		dev_err(&data->coleman_pci_dev->dev,
			"Invalid magic received.\n");
		return -ENOTTY;
	}

	if (!access_ok((void *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;
	dev_info(&data->coleman_pci_dev->dev, "access_ok");
	// Handle the request
	switch (cmd) {
	case IOCTL_SET_RW_OFFSETS: {
		int err;

		// On Meg2 as opposed to senryu, the copies across the kernel boundary are not automatic
		const struct ioctl_set_rw_offsets __user *user_rw_offsets =
			(const struct ioctl_set_rw_offsets __user *)arg;
		struct ioctl_set_rw_offsets
			rw_offsets; // kernel local copy to receive user data

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_SET_RW_OFFSETS");

		// copy user request to kernel space
		err = copy_from_user(&rw_offsets, user_rw_offsets,
				     sizeof(rw_offsets));
		if (err) {
			// copy from user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy from user returns %d\n", err);
			return -EIO;
		}

		dev_info(
			&data->coleman_pci_dev->dev,
			"ioctl set rw offsets (%p), bar_number:%u, read:%u, write:%u, control_request:%u, control_response:%u\n",
			user_rw_offsets, rw_offsets.bar_number,
			rw_offsets.read_offset, rw_offsets.write_offset,
			rw_offsets.control.request,
			rw_offsets.control.response);

		// copy settings from kernel space data to driver space
		data->pcie_validator.bar_number = rw_offsets.bar_number;
		data->pcie_validator.bar_offsets.read = rw_offsets.read_offset;
		data->pcie_validator.bar_offsets.write =
			rw_offsets.write_offset;
		data->pcie_validator.bar_offsets.control_request =
			rw_offsets.control.request;
		data->pcie_validator.bar_offsets.control_response =
			rw_offsets.control.response;
	} break;

	case IOCTL_GET_DMA_BUFFER_INFO: {
		int err;
		// On Meg2 as opposed to senryu, the copies across the kernel boundary are not automatic

		struct ioctl_get_pinned_buffer_info
			*user_ioctl_get_dma_buffer_info =
				(struct ioctl_get_pinned_buffer_info __user *)
					arg;

		struct ioctl_get_pinned_buffer_info
			dma_buffer_info; // kernel local copy for temp storage

		struct dma_buffer *dma_buffer =
			&data->pcie_validator.dma_buffer;

		// populate kernel copy of data
		dma_buffer_info.buffer_index = E_PINNED_BUFFER_DMA_SELECT;
		dma_buffer_info.bus_address = dma_buffer->bus_address;
		dma_buffer_info.size = dma_buffer->size;

		// copy kernel data to user space
		err = copy_to_user(user_ioctl_get_dma_buffer_info,
				   &dma_buffer_info, sizeof(dma_buffer_info));

		if (err) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d\n", err);
			return -EIO;
		}
	} break;

	case IOCTL_GET_DMA_BUFFER: {
		int err;
		// On Meg2 as opposed to senryu, the copies across the kernel boundary are not automatic
		struct ioctl_get_pinned_buffer *user_ioctl_get_dma_buffer =
			(struct ioctl_get_pinned_buffer __user *)arg;

		struct ioctl_get_pinned_buffer
			get_dma_buffer; // kernel local copy for temp storage

		struct dma_buffer *dma_buffer =
			&data->pcie_validator.dma_buffer;

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_GET_DMA_BUFFER");

		// populate kernel copy of data
		get_dma_buffer.bus_address = dma_buffer->bus_address;
		get_dma_buffer.size = dma_buffer->size;

		// copy kernel data to user space
		err = copy_to_user(user_ioctl_get_dma_buffer, &get_dma_buffer,
				   sizeof(get_dma_buffer));

		if (err) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d\n", err);
			return -EIO;
		}
	} break;

	case IOCTL_GET_APERTURE_BUFFER_INFO: {
		struct ioctl_get_pinned_buffer_info __user *user_aperture_buffer =
			(struct ioctl_get_pinned_buffer_info __user *)
				arg; // accessor for request in user space

		struct ioctl_get_pinned_buffer_info
			aperture_buffer; // kernel local copy for temp storage

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_GET_APERTURE_BUFFER_INFO");

		// populate kernel copy of data
		aperture_buffer.buffer_index = E_PINNED_BUFFER_APERTURE_SELECT;
		aperture_buffer.bus_address =
			data->pcie_validator.aperture_buffer.bus_address;
		aperture_buffer.size =
			data->pcie_validator.aperture_buffer.size;

		// copy kernel data to user space
		int err = copy_to_user(user_aperture_buffer, &aperture_buffer,
				       sizeof(aperture_buffer));

		if (err) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d @%s:%d\n", err,
				__FILE__, __LINE__);
			return -EIO;
		}
	} break;

	case IOCTL_GET_APERTURE_BUFFER: {
		int err;
		// Copy request from user space to kernel space
		struct ioctl_get_pinned_buffer __user *user_aperture_buffer =
			(struct ioctl_get_pinned_buffer __user *)
				arg; // accessor for request in user space

		struct ioctl_get_pinned_buffer
			aperture_buffer; // kernel local copy for temp storage

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_GET_APERTURE_BUFFER");

		// populate kernel copy of data
		aperture_buffer.bus_address =
			data->pcie_validator.aperture_buffer.bus_address;
		aperture_buffer.size =
			data->pcie_validator.aperture_buffer.size;

		// copy kernel data to user space
		err = copy_to_user(user_aperture_buffer, &aperture_buffer,
				   sizeof(aperture_buffer));

		if (err) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d @%s:%d\n", err,
				__FILE__, __LINE__);
			return -EIO;
		}
	} break;

	case IOCTL_SELECT_MMAP_BUFFER: {
		int err;
		// On Meg2 as opposed to senryu, the copies across the kernel boundary are not automatic
		const struct ioctl_select_mmap_buffer __user *user_select_mmap =
			(const struct ioctl_select_mmap_buffer __user *)arg;
		struct ioctl_select_mmap_buffer select_mmap; // kernel local copy
		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_SELECT_MMAP_BUFFER");

		// copy user request to kernel space
		err = copy_from_user(&select_mmap, user_select_mmap,
				     sizeof(select_mmap));
		if (err) {
			// copy from user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_from_user returns %d\n", err);
			return -EIO;
		}

		// validate requested buffer index.
		if (select_mmap.buffer_index >= E_PINNED_BUFFER_SELECT_COUNT) {
			return -EINVAL;
		}

		// copy settings from kernel space data to driver space
		data->pcie_validator.pinned_buffer_map_select =
			select_mmap.buffer_index;

	} break;

	case IOCTL_SEND_VALIDATOR_CONTROL_REQUEST: {
		int err;
		size_t offset =
			data->pcie_validator.bar_offsets.control_request;

		// On Meg2 as opposed to senryu, the copies across the kernel boundary are not automatic
		const struct ioctl_validator_control_request __user
			*user_request =
				(const struct ioctl_validator_control_request
					 __user *)arg;

		struct ioctl_validator_control_request
			request; // kernel local copy for temp storage

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_SEND_VALIDATOR_CONTROL_REQUEST");
		dev_info(&data->coleman_pci_dev->dev, "user_request %p",
			 user_request);

		// copy user request to kernel space
		err = copy_from_user(&request, user_request, sizeof(request));
		if (err) {
			// copy from user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_from_user returns %d\n", err);
			return -EIO;
		}

		// Write data from kernel local buffer to the device
		arfw_mem_util_iomem_write_block(
			data->ptr_bar[data->pcie_validator.bar_number], offset,
			&request.data, request.size);
	} break;

	case IOCTL_GET_VALIDATOR_CONTROL_RESPONSE: {
		int err;
		int bytes_left;
		size_t offset =
			data->pcie_validator.bar_offsets.control_response;

		// On Meg2 as opposed to senryu, the copies across the kernel boundary are not automatic
		struct ioctl_validator_control_response __user *user_response =
			(struct ioctl_validator_control_response __user *)
				arg; // user space pointer to a response buffer

		struct ioctl_validator_control_response
			response; // kernel local copy for temp storage

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_GET_VALIDATOR_CONTROL_RESPONSE");

		// copy user space response to kernel space
		err = copy_from_user(&response, user_response,
				     sizeof(response));
		if (err) {
			// copy from user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_from_user returns %d\n", err);
			return -EIO;
		}

		dev_info(
			&data->coleman_pci_dev->dev,
			"ioctl get validator control response %lX, offset %zu, size %u\n",
			arg, offset, response.size);

		// read data from device to kernel space buffer
		arfw_mem_util_iomem_read_block(
			data->ptr_bar[data->pcie_validator.bar_number], offset,
			&response.data, response.size);

		// copy kernel space buffer to user space buffer
		bytes_left = copy_to_user(&user_response->data, &response.data,
					  response.size);

		if (bytes_left) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d\n", bytes_left);
			return -EIO;
		}

	} break;

	case IOCTL_SEND_VALIDATOR_DOORBELL_REQUEST: {
		int err;
		static uint32_t ring_value = DOORBELL_RING_VALUE;

		// On Meg2 as opposed to senryu, the copies across the kernel boundary are not automatic
		const struct ioctl_validator_doorbell_request __user
			*user_request =
				(const struct ioctl_validator_doorbell_request
					 __user *)arg;

		struct ioctl_validator_doorbell_request
			request; // kernel local copy for temp storage

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_SEND_VALIDATOR_DOORBELL_REQUEST");

		// copy user request to kernel space
		err = copy_from_user(&request, user_request, sizeof(request));
		if (err) {
			// copy from user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_from_user returns %d\n", err);
			return -EIO;
		}

		arfw_mem_util_iomem_write_block(
			data->ptr_bar[ALFRED_DOORBELL_BAR],
			request.doorbell_reg_offset, &ring_value,
			sizeof(ring_value));
	} break;

	case IOCTL_GET_ALLOCATED_IRQS: {
		int err;

		// Copy request from user space to kernel space
		struct ioctl_get_msi_info __user *user_msi_info =
			(struct ioctl_get_msi_info __user *)
				arg; // accessor for request in user space

		struct ioctl_get_msi_allocation_info
			msi_info; // kernel local copy for temp storage

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_GET_ALLOCATED_IRQS");

		// populate kernel copy of data
		memcpy(msi_info.allocated_irqs, data->allocated_irqs,
		       sizeof(data->allocated_irqs));

		// copy kernel data to user space
		err = copy_to_user(user_msi_info, &msi_info, sizeof(msi_info));

		if (err) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d @%s:%d\n", err,
				__FILE__, __LINE__);
			return -EIO;
		}

		// Reset the MSI IRQ received statuses
		memset(data->received_irq_numbers, 0,
		       sizeof(data->received_irq_numbers));
		memset(data->received_irq_count, 0,
		       sizeof(data->received_irq_count));
	} break;

	case IOCTL_GET_RECEIVED_IRQS: {
		int err;
		// Copy request from user space to kernel space
		struct ioctl_get_msi_info __user *user_msi_info =
			(struct ioctl_get_msi_info __user *)
				arg; // accessor for request in user space

		struct ioctl_get_msi_received_info
			msi_info; // kernel local copy for temp storage

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_GET_RECEIVED_IRQS");

		// populate kernel copy of data
		memcpy(msi_info.received_irq_numbers,
		       data->received_irq_numbers,
		       sizeof(data->received_irq_numbers));
		memcpy(msi_info.received_irq_count, data->received_irq_count,
		       sizeof(data->received_irq_count));

		// copy kernel data to user space
		err = copy_to_user(user_msi_info, &msi_info, sizeof(msi_info));

		if (err) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d @%s:%d\n", err,
				__FILE__, __LINE__);
			return -EIO;
		}
	} break;

	case IOCTL_CHECK_TEST_DONE_FLAG: {
		int err;
		// Copy request from user space to kernel space
		struct ioctl_check_test_done_flag __user *user_test_status =
			(struct ioctl_check_test_done_flag __user *)
				arg; // accessor for request in user space

		struct ioctl_check_test_done_flag
			k_test_status; // kernel local copy for temp storage
		k_test_status.done = 0;

		dev_info(&data->coleman_pci_dev->dev,
			 "IOCTL command IOCTL_CHECK_TEST_DONE_FLAG");

		// populate kernel copy of data
		memcpy(&k_test_status.done,
		       &data->pcie_validator_test_done_flag, sizeof(uint32_t));

		// copy kernel data to user space
		err = copy_to_user(user_test_status, &k_test_status,
				   sizeof(k_test_status));

		if (err) {
			// copy to user space failed, return early
			dev_err(&data->coleman_pci_dev->dev,
				"copy_to_user returns %d @%s:%d\n", err,
				__FILE__, __LINE__);
			return -EIO;
		}

		// Reset the flag
		data->pcie_validator_test_done_flag = 0;
	} break;

	default:
		dev_err(&data->coleman_pci_dev->dev,
			"Unknown ioctl command: 0x%X\n", cmd);
		status = -ENOTTY;
	}
	return status;
}

static ssize_t coleman_pci_read(struct file *filp, char __user *buffer,
				size_t size, loff_t *position)
{
	struct coleman_pci_drvdata *data = container_of(
		filp->private_data, struct coleman_pci_drvdata, misc_device);

	size_t offset = data->pcie_validator.bar_offsets.read;
	uint8_t *kernel_buffer;
	int err;

	// TODO: limit size to bar_size - offset;

	dev_info(
		&data->coleman_pci_dev->dev,
		"source bar_ptr = %p, read %zu bytes from offset %zu to user space buffer %p\n",
		data->ptr_bar[data->pcie_validator.bar_number], size, offset,
		buffer);

	// Allocate buffer for reading from device to kernel space
	kernel_buffer = kzalloc(size, GFP_KERNEL);

	if (kernel_buffer == NULL)
		return -ENOMEM;

	// Read data from device to kernel space buffer
	arfw_mem_util_iomem_read_block(
		data->ptr_bar[data->pcie_validator.bar_number], offset,
		kernel_buffer, size);

	// Copy kernel space buffer to user space buffer
	err = copy_to_user(buffer, kernel_buffer, size);

	if (err) {
		dev_err(&data->coleman_pci_dev->dev,
			"copy_to_user returns %d\n", err);
		err = -EIO;
	}

	kfree(kernel_buffer);

	return err ? err : size;
}

static ssize_t coleman_pci_write(struct file *filp, const char __user *buffer,
				 size_t size, loff_t *position)
{
	struct coleman_pci_drvdata *data = container_of(
		filp->private_data, struct coleman_pci_drvdata, misc_device);
	size_t offset;
	void *kernel_buffer;
	int err;

	offset = data->pcie_validator.bar_offsets.write;

	//TODO: limit size to bar_size - offset;

	dev_info(&data->coleman_pci_dev->dev, "write %zu bytes to offset %zu\n",
		 size, offset);

	// Allocate buffer for reading from userspace to kernel buffer
	kernel_buffer = kmalloc(size, GFP_KERNEL);

	// Copy data from userspace to kernel space buffer
	err = copy_from_user(kernel_buffer, buffer, size);

	if (err) {
		dev_err(&data->coleman_pci_dev->dev,
			"copy_from_user returns %d\n", err);
		kfree(kernel_buffer);
		return -EIO;
	}

	// Write data from kernel space buffer to the device
	arfw_mem_util_iomem_write_block(
		data->ptr_bar[data->pcie_validator.bar_number], offset,
		kernel_buffer, size);
	kfree(kernel_buffer);

	return size;
}

static int coleman_pci_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct coleman_pci_drvdata *data = container_of(
		filp->private_data, struct coleman_pci_drvdata, misc_device);
	dev_info(&data->coleman_pci_dev->dev,
		 "%s: pinned_buffer_map_select %d\n", __func__,
		 data->pcie_validator.pinned_buffer_map_select);
	struct dma_buffer *pinned_buffer = NULL;
	int status = -EINVAL;

	switch (data->pcie_validator.pinned_buffer_map_select) {
	case E_PINNED_BUFFER_DMA_SELECT:
		pinned_buffer = &data->pcie_validator.dma_buffer;
		break;
	case E_PINNED_BUFFER_APERTURE_SELECT:
		pinned_buffer = &data->pcie_validator.aperture_buffer;
		break;
	default:
		goto early_exit;
	}
	dev_info(
		&data->coleman_pci_dev->dev,
		"map: pinned_buffer->virtual_address %p\n, bus_address %llx, size %zu\n",
		pinned_buffer->virtual_address, pinned_buffer->bus_address,
		pinned_buffer->size);
	status = dma_mmap_coherent(&data->coleman_pci_dev->dev, vma,
				   pinned_buffer->virtual_address,
				   pinned_buffer->bus_address,
				   pinned_buffer->size);
early_exit:
	dev_info(&data->coleman_pci_dev->dev, "%s: status %d\n", __func__,
		 status);
	return status;
}

static const struct file_operations coleman_pci_device_fops = {
	.owner = THIS_MODULE,
	.read = coleman_pci_read,
	.write = coleman_pci_write,
	.open = coleman_pci_misc_device_open,
	.release = coleman_pci_misc_device_close,
	.unlocked_ioctl = coleman_pci_misc_device_ioctl,
	.mmap = coleman_pci_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl = coleman_pci_misc_device_ioctl,
#endif
};

/**
 * @brief Function is called, when a PCI device is registered
 *
 * @param dev   pointer to the PCI device
 * @param id    pointer to the corresponding id table's entry
 *
 * @return      0 on success
 *              negative error code on failure
 */
static int coleman_pci_probe(struct pci_dev *dev,
			     const struct pci_device_id *id)
{
	/*
   * Enable the device
   */

	struct coleman_pci_drvdata *drvdata = NULL;
	int function_number = dev->devfn & PCI_FUNCTION_MASK;
	int status;
	int bar_num;
	int i;
	int irq;
	char *device_name;
	resource_size_t bar_size;
	size_t name_len;

	dev_info(&dev->dev, "%s(pci_dev=%p, id=%p)\n", __func__, dev, id);
	dev_info(&dev->dev,
		 "%s: function_number is %d, with resource count %d\n",
		 __func__, function_number, DEVICE_COUNT_RESOURCE);

	// Reserve the PCI BAR regions for use
	status = pci_request_regions(dev, KBUILD_MODNAME);
	if (status != 0) {
		dev_err(&dev->dev,
			"pci_request_regions_exclusive failed. Error: %d\n",
			status);
		return status;
	}
	status = pcim_enable_device(dev);
	dev_err(&dev->dev, "pcim_enable_device returned: %d\n", status);
	if (status < 0) {
		dev_err(&dev->dev, "pcim_enable_device failed. Error: %d\n",
			status);
		return status;
	}

	drvdata = devm_kzalloc(&dev->dev, sizeof(struct coleman_pci_drvdata),
			       GFP_KERNEL);
	dev_info(&dev->dev, "devm_kzalloc returned %p\n", drvdata);
	if (drvdata == NULL) {
		dev_info(&dev->dev, "devm_kzalloc failed\n");
		return -ENOMEM;
	}

	drvdata->io_perform_param = IO_DMA_INIT;

	for (bar_num = 0; (bar_num < COLEMAN_BAR_COUNT); bar_num++) {
		bar_size = pci_resource_len(dev, bar_num);
		dev_info(&dev->dev, "BAR%d len is 0x%llx\n", bar_num, bar_size);
		if (bar_size > 0) {
			dev_info(&dev->dev, "BAR%d start is 0x%llx\n", bar_num,
				 pci_resource_start(dev, bar_num));
			drvdata->ptr_bar[bar_num] =
				pcim_iomap(dev, bar_num, bar_size);
			dev_info(&dev->dev, "BAR%d pointer is %p.\n", bar_num,
				 drvdata->ptr_bar[bar_num]);
			if (drvdata->ptr_bar[bar_num] == NULL) {
				dev_err(&dev->dev, "BAR%d pointer is NULL.",
					bar_num);
				return -EFAULT;
			}
		}
	}

	device_name = kzalloc(DEVICE_NAME_LEN, GFP_KERNEL);

	name_len = snprintf(device_name, DEVICE_NAME_LEN, "%s_%.2x.%.1x",
			    DEVICE_NAME, (dev->devfn) >> PCI_DEVICE_SHIFT,
			    (dev->devfn) & PCI_FUNCTION_MASK);
	if (name_len >= DEVICE_NAME_LEN) {
		dev_err(&dev->dev,
			"Device name too long (%zd) for allocated buffer (%u)\n",
			name_len, DEVICE_NAME_LEN);
		return -ENOMEM;
	}

	drvdata->misc_device.minor = MISC_DYNAMIC_MINOR;
	drvdata->misc_device.name = device_name;
	drvdata->misc_device.fops = &coleman_pci_device_fops;

	status = misc_register(&drvdata->misc_device);
	if (status != 0) {
		dev_err(&dev->dev, "Couldn't register misc driver. Error: %d\n",
			status);
		return status;
	}

	dev_info(&dev->dev, "Mounted misc device as %s\n", device_name);
	dev_set_drvdata(&dev->dev, drvdata);

	drvdata->msi_vector_count = pci_alloc_irq_vectors(
		dev, COLEMAN_MSI_MIN_VECTORS, COLEMAN_MSI_MAX_VECTORS,
		PCI_IRQ_MSI | PCI_IRQ_MSIX);

	dev_info(&dev->dev, "Configuring %d MSI vectors\n",
		 drvdata->msi_vector_count);

	dev_info(&dev->dev, "%s: Allocated %d MSI vectors for standard use\n",
		 __func__, drvdata->msi_vector_count);
	dev_info(&dev->dev,
		 "%s: Allocated MSI vector %d for signaling tests are done\n",
		 __func__, pcie_validator_test_msi);

	if (drvdata->msi_vector_count < 0) {
		dev_err(&dev->dev,
			"Couldn't configure MSI vectors. Error: %d\n",
			drvdata->msi_vector_count);
		return drvdata->msi_vector_count;
	}

	for (i = 0; i < drvdata->msi_vector_count; ++i) {
		irq = pci_irq_vector(dev, i);
		status = request_irq(irq, coleman_pci_interrupt_handler,
				     IRQF_SHARED, "coleman_pci", dev);
		if (status != 0) {
			dev_err(&dev->dev,
				"Couldn't register irq handle. MSI number %d. Error: %d\n",
				i, status);
			return status;
		}
		drvdata->allocated_irqs[i] = irq;
		dev_info(&dev->dev, "Configured MSI %d as irq %d\n", i, irq);
	}

	drvdata->coleman_pci_dev = dev;
	g_coleman_pci_dev = dev;
	pci_set_master(dev);

	device_create_file(&dev->dev, &io_dma_addr_dev_attr);
	device_create_file(&dev->dev, &io_buffer_dev_attr);
	device_create_file(&dev->dev, &dma_buffer_dev_attr);
	device_create_file(&dev->dev, &io_perform_dev_attr);

	return 0;
}

/**
 * @brief Function is called, when a PCI device is unregistered
 *
 * @param dev   pointer to the PCI device
 */
static void coleman_pci_remove(struct pci_dev *dev)
{
	int irq;
	struct coleman_pci_drvdata *drvdata = device_to_coleman(&dev->dev);
	device_remove_file(&dev->dev, &io_dma_addr_dev_attr);
	device_remove_file(&dev->dev, &dma_buffer_dev_attr);
	device_remove_file(&dev->dev, &io_buffer_dev_attr);
	device_remove_file(&dev->dev, &io_perform_dev_attr);

	/* Free all the interrupts that were allocated at _probe time*/
	for (int msi_vector_number = 0;
	     msi_vector_number < drvdata->msi_vector_count;
	     msi_vector_number++) {
		irq = pci_irq_vector(dev, msi_vector_number);
		dev_info(&dev->dev, "%s: free irq %d\n", __func__, irq);
		free_irq(irq, dev);
	}

	irq = pci_irq_vector(dev, pcie_validator_test_msi);
	dev_info(&dev->dev, "%s: free irq %d\n", __func__, irq);
	free_irq(irq, dev);

	pci_free_irq_vectors(dev);
	misc_deregister(&drvdata->misc_device);
	pci_disable_device(dev);
	pci_release_regions(dev);
	dev_info(&dev->dev, "%s: done\n", __func__);
}

/* PCI driver struct */
static struct pci_driver coleman_pci_driver = {
	.name = "Coleman PCI",
	.id_table = coleman_pci_ids,
	.probe = coleman_pci_probe,
	.remove = coleman_pci_remove,
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init coleman_pci_init(void)
{
	return pci_register_driver(&coleman_pci_driver);
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit coleman_pci_exit(void)
{
	pci_unregister_driver(&coleman_pci_driver);
}

module_init(coleman_pci_init);
module_exit(coleman_pci_exit);
