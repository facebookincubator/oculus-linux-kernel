// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_col_pci_main.c
 *
 * @brief PCIe driver for PCI based AR accelerator
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>

#include <arfw_config.h>
#include <arfw_log.h>
#include <arfw_shim.h>
#include <arfw_pci.h>

#include "ar_pci_int.h"
#include "ar_pci_bar.h"
#include "ar_pci_sysfs.h"
#include "arfw_device.h"
#include "control_rings.h"
#include "data_ring.h"

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
#include "msm_pcie.h"
#endif // CONFIG_ARFIRMWARE_MSM_PCIE

#define AR_PCI_SUSPEND_RESUME_TIMEOUT_MS 10000
unsigned long ctrl_ring_timeout_ms_param = 1000;

static bool enable_aperture = true;
module_param(enable_aperture, bool, 0664);

// This is the index of the handshake interrupt. -1 means it is not used.
static int msi_handshake_index = -1;
module_param(msi_handshake_index, int, 0444);

// Hack to get around the fact QEMU has the address space hardcoded.
// Func 0 sets it based on the ARP version.
// All other functions will continue probing based on its value.
static int ar_pci_max_func;
static int ar_pci_func0_err;

// Normally, support functions 0-2.
// For ARP ver >= 5.1, support functions 0-3.
#define PCI_ARP_MAJOR_VER_MF_SUPPORT 0x5
#define PCI_ARP_MINOR_VER_MF_SUPPORT 0x1
#define PCI_PRE_V5P1_MAX_FUNCTION 2
#define PCI_V5P1_MAX_FUNCTION 3
#define PCI_MAX_FUNCTION 6

// Delete all data rings protocol requires ARP ver >= 5.3
#define PCI_ARP_MAJOR_VER_DEL_ALL_SUPPORT 0x5
#define PCI_ARP_MINOR_VER_DEL_ALL_SUPPORT 0x3

static LIST_HEAD(ar_pci_registered_drivers_list);
static DEFINE_MUTEX(ar_pci_registered_drivers_lock);

// Synchronization for suspend/resume ordering
static atomic_t ar_pci_non_f0_active_count = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(ar_pci_f0_suspend_wait);
static atomic_t ar_pci_f0_is_up = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(ar_pci_f0_resume_wait);

// Core name supporting requires ARP ver >= 5.4
#define PCI_ARP_MAJOR_VER_SYS_NAME_SUPPORT 0x5
#define PCI_ARP_MINOR_VER_SYS_NAME_SUPPORT 0x4

static void ar_pci_link_suspend(ar_pci_driver_t *driver);
static int ar_pci_link_resume(ar_pci_driver_t *driver);
static int ar_pci_pm_suspend(const char *val, const struct kernel_param *kp);

static const struct pci_device_id ar_pci_id[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_META, PCI_DEVICE_ID_META) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F0) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F1) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F2) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F3) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F4) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F5) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F6) },
	{ PCI_DEVICE(PCI_VENDOR_ID_META_EX, PCI_DEVICE_ID_F7) },
	{ /* end: zero */ }
};

static const struct arfw_driver_ops ar_dev_ops = {
	.handle_send_queue = ar_dev_handle_send_queue_request,
	.handle_rcv_queue_consume = ar_dev_handle_rcv_queue_consume,
	.handle_queue_data_alloc = ar_dev_handle_queue_data_alloc,
	.handle_queue_data_free = ar_dev_handle_queue_data_free,
	.handle_queue_create = ar_dev_handle_queue_create_request,
	.handle_queue_destroy = ar_dev_handle_queue_destroy_request,
	.handle_get_device_information = ar_dev_handle_get_device_information,
	.handle_receive_payload_pend = ar_dev_handle_receive_payload_pend,
	.handle_notify_queue_ready = ar_dev_handle_notify_queue_ready,
	.handle_queue_data_mmap = ar_dev_handle_queue_data_mmap,
	.handle_dma_map_quirk = ar_dev_handle_dma_map_quirk,
	.handle_dma_unmap_quirk = ar_dev_handle_dma_unmap_quirk,
	.handle_aperture_alloc = ar_dev_handle_aperture_alloc,
	.handle_aperture_mmap = ar_dev_handle_aperture_mmap,
	.handle_aperture_free = ar_dev_handle_aperture_free,
};

static int ar_pci_register_client(ar_pci_driver_t *driver)
{
	int err;
	struct pci_dev *dev;
	arp_info_t *arp_info = &driver->arp_info;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	err = arfw_shim_cdev_register(&dev->dev, arp_info->devid, &ar_dev_ops,
				      &driver->client_ops, dev);

	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_DEV_REG,
				   "Failed to register device %s [err: %d]",
				   arp_info->devid, err);
	} else {
		AR_ASSERT(driver->client_ops);
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_DEV_REG,
				    "Registered device %s OK", arp_info->devid);
	}

	return err;
}

static int ar_pci_unregister_client(ar_pci_driver_t *driver, bool module_remove)
{
	int err;
	struct pci_dev *dev;
	arp_info_t *arp_info = &driver->arp_info;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	err = arfw_shim_cdev_unregister(arp_info->devid, NULL, module_remove);

	if (err)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_SHUTDOWN,
				   "Failed to unregister device [err: %d]",
				   err);
	else
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN,
				    "Unregistered device");

	return err;
}

static int ar_pci_aperture_create(ar_pci_driver_t *driver, const uint32_t id,
				  const size_t size)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	driver->aperture.id = id;
	driver->aperture.size = size;

	AR_ASSERT(driver->aperture.size % PAGE_SIZE == 0);

	driver->aperture.addr =
		dma_alloc_coherent(&dev->dev, driver->aperture.size,
				   &driver->aperture.dma_addr, GFP_USER);
	if (driver->aperture.addr == NULL) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to allocate aperture memory");
		return -ENOMEM;
	}

	driver->aperture.bits =
		driver->aperture.size / ARFW_APERTURE_ALLOC_CHUNK_SIZE;
	spin_lock_init(&driver->aperture.locked_allocs.lock);

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
			    "Allocated aperture memory for function %d",
			    PCI_FUNC(dev->devfn));

	return 0;
}

static void ar_pci_aperture_destroy(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	if (driver->aperture.addr == NULL)
		return;
	dma_free_coherent(&dev->dev, driver->aperture.size,
			  driver->aperture.addr, driver->aperture.dma_addr);

	// TODO(T208592194): handle aperture cleanup safely
	AR_LOG_PCI_DEV_WARN(
		&dev->dev, AR_LOG_SHUTDOWN,
		"Freeing aperture memory. Warning: it could still be used by the user, this will be fixed "
		"in T208592194. If you see random memory corruption in dma mem, perhaps it is due to "
		"aperture clients using this buffer after free. [bytes: %zu]",
		driver->aperture.size);

	// clear out aperture data
	memset(&driver->aperture, 0, sizeof(driver->aperture));
}

static int ar_pci_acquire_resources(ar_pci_driver_t *driver)
{
	int err;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	err = ar_pci_bar_map(dev);
	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to map bars [err: %d]", err);
		return err;
	}

	err = ar_pci_control_rings_create(dev);
	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to create control rings [err: %d]",
				   err);
		goto error_control_rings;
	}

	// create apertures if supported
	if (enable_aperture) {
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
				    "Aperture is enabled");
		if (PCI_FUNC(dev->devfn) == APERTURE_TEST_FUNC) {
			err = ar_pci_aperture_create(driver,
						     APERTURE_TEST_FUNC_ID,
						     APERTURE_TEST_FUNC_SIZE);
			if (err)
				AR_LOG_PCI_DEV_ERR(
					&dev->dev, AR_LOG_INIT,
					"Failed to create aperture for testing pcie function");
		} else if (PCI_FUNC(dev->devfn) == APERTURE_FDLA_FUNC) {
			err = ar_pci_aperture_create(driver,
						     APERTURE_FDLA_FUNC_ID,
						     APERTURE_FDLA_FUNC_SIZE);
			if (err)
				AR_LOG_PCI_DEV_ERR(
					&dev->dev, AR_LOG_INIT,
					"Failed to create aperture for FDLA pcie function");
		}
	} else {
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
				    "Aperture is disabled");
	}

	return 0;

error_control_rings:
	if (ar_pci_bar_unmap(dev))
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to unmap bars");

	return err;
}

static void ar_pci_release_resources(ar_pci_driver_t *driver)
{
	int err;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	// if aperture present, free it
	if (driver->aperture.addr)
		ar_pci_aperture_destroy(driver);

	ar_pci_control_rings_destroy(dev);
	ar_pci_bar_release(dev);

	err = ar_pci_bar_unmap(dev);
	if (err)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_SHUTDOWN,
				   "Failed to unmap bars [err: %d]", err);
}

static void ar_pci_unblock_client_resource(ar_pci_client_list_t *queues)
{
	AR_ASSERT(queues);
	mutex_lock(&queues->lock);
	queues->released = false;
	mutex_unlock(&queues->lock);
}

static void ar_pci_release_client_resource(ar_pci_driver_t *driver,
					   ar_pci_client_list_t *queues,
					   enum ar_queue_shutdown_reason reason)
{
	ar_client_queue_t *queue = NULL, *temp = NULL;

	AR_ASSERT(driver);
	AR_ASSERT(driver->client_ops);
	AR_ASSERT(driver->client_ops->handle_client_queue_shutdown);
	AR_ASSERT(driver->client_ops->handle_client_queue_destroy);

	// Acquire a lock to be sure that arfw_queue is alive and won't
	// be released in the middle of the operation. Set ->released only
	// after the lock is held so the client release path cannot observe
	// released == true and take its group-shutdown fast-path while we
	// are about to iterate the queue list (which would leave stale
	// entries pointing at NULL queues, panicking the shutdown handler).
	driver->client_ops->handle_client_lock();
	mutex_lock(&queues->lock);
	queues->released = true;
	mutex_unlock(&queues->lock);

	list_for_each_entry_safe(queue, temp, &queues->list_node, node) {
		driver->client_ops->handle_client_queue_shutdown(
			queue->arfw_queue, reason);
		driver->client_ops->handle_client_queue_destroy(
			queue->arfw_queue);
	}
	driver->client_ops->handle_client_unlock();
}

static int ar_pci_count_queue_nodes(ar_pci_client_list_t *queues)
{
	int cnt = 0;
	ar_client_queue_t *queue;
	list_for_each_entry(queue, &queues->list_node, node)
		cnt++;
	return cnt;
}

static void
ar_pci_release_all_client_resources(ar_pci_driver_t *driver,
				    enum ar_queue_shutdown_reason reason)
{
	struct pci_dev *dev;
	ar_client_queue_t *queue;
	ar_pci_client_list_t *send_queues;
	ar_pci_client_list_t *rcv_queues;
	ar_future_t *result;
	arfw_client_queue_t *arfw_queues;
	int err;
	int queue_cnt, idx;

	AR_ASSERT(driver);
	AR_ASSERT(driver->client_ops);
	AR_ASSERT(driver->client_ops->handle_client_queue_shutdown);
	AR_ASSERT(driver->client_ops->handle_client_queue_destroy);

	dev = driver->dev;
	AR_ASSERT(dev);

	AR_LOG_PCI_DEV_INFO(
		&dev->dev, AR_LOG_SHUTDOWN,
		"Shutting down the all client resources, signaling upstream");

	send_queues = &driver->client_send_queues;
	rcv_queues = &driver->client_rcv_queues;

	// Acquire a lock to be sure that arfw_queue is alive and won't
	// be released in the middle of the operation. Set ->released only
	// after the lock is held so the client release path cannot observe
	// released == true and take its group-shutdown fast-path while we
	// are about to iterate the queue list (which would leave stale
	// entries pointing at NULL queues, panicking the shutdown handler).
	driver->client_ops->handle_client_lock();
	mutex_lock(&send_queues->lock);
	send_queues->released = true;
	mutex_unlock(&send_queues->lock);
	mutex_lock(&rcv_queues->lock);
	rcv_queues->released = true;
	mutex_unlock(&rcv_queues->lock);

	queue_cnt = ar_pci_count_queue_nodes(send_queues) +
		    ar_pci_count_queue_nodes(rcv_queues);

	if (queue_cnt == 0) {
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN,
				    "Nothing to release, no queues");
		driver->client_ops->handle_client_unlock();
		return;
	}

	arfw_queues = kzalloc(queue_cnt * sizeof(*arfw_queues), GFP_KERNEL);
	if (!arfw_queues) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_SHUTDOWN,
				   "Failed to allocate memory for arfw_queues");
		driver->client_ops->handle_client_unlock();
		return;
	}

	idx = 0;
	// Copy the queues to a local array, since the PCI layer clears the list
	// and it is empty after `ar_pci_destroy_all_data_rings.
	list_for_each_entry(queue, &send_queues->list_node, node)
		arfw_queues[idx++] = queue->arfw_queue;
	list_for_each_entry(queue, &rcv_queues->list_node, node)
		arfw_queues[idx++] = queue->arfw_queue;

	for (idx = 0; idx < queue_cnt; idx++)
		driver->client_ops->handle_client_queue_shutdown(
			arfw_queues[idx], reason);
	// Send a single group request to delete all queues at once instead of
	// individual PCIe commands, reducing PCIe traffic.
	result = ar_pci_destroy_all_data_rings(driver->dev,
					       ctrl_ring_timeout_ms_param);

	if (IS_ERR(result)) {
		err = PTR_ERR(result);
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_SHUTDOWN,
			"FW failed to destroy all data rings (err: %d).\n"
			"Destroying memory allocated for data rings anyway.\n"
			"If FW uses deallocated resources, this may cause a kernel panic.",
			err);
	} else {
		ar_future_wait(result);
	}

	for (idx = 0; idx < queue_cnt; idx++)
		driver->client_ops->handle_client_queue_destroy(
			arfw_queues[idx]);

	driver->client_ops->handle_client_unlock();
	kfree(arfw_queues);
}

static void ar_pci_unblock_client_resources(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_RESUME,
			    "Unblocking the client resources");

	ar_pci_unblock_client_resource(&driver->client_send_queues);
	ar_pci_unblock_client_resource(&driver->client_rcv_queues);
}

static void
ar_pci_release_client_resources(ar_pci_driver_t *driver,
				enum ar_queue_shutdown_reason reason)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	AR_LOG_PCI_DEV_INFO(
		&dev->dev, AR_LOG_SHUTDOWN,
		"Shutting down the client resources, signaling upstream");

	if (driver->group_shutdown_supported) {
		ar_pci_release_all_client_resources(driver, reason);
	} else {
		ar_pci_release_client_resource(
			driver, &driver->client_send_queues, reason);
		ar_pci_release_client_resource(
			driver, &driver->client_rcv_queues, reason);
	}
}

static bool coleman_emulate_perst;
module_param(coleman_emulate_perst, bool, 0664);

/* For the emulated targets, like diamond_emu, there is no GPIO or other
 * side channel to send a PERST signal to Coleman. Because of this it is
 * required to full reboot VP emulator, after every insmod. This is the
 * PERST emulation support for the targets which require it.
 * QEMU<->VP does not support getting BAR size from VP.
 * So hardcode the BAR sizes here.
 */
#define COL_PCIE_FUNC_0_MMIO_MEMORY_BAR_SIZE 0x1000
#define COL_PCI_FUNC_1 1
#define COL_PCIE_FUNC_1_MMIO_MEMORY_BAR_SIZE 0x800
#define COL_PCIE_EMULATE_PERST_DELAY_MS 200

/* VP emulation is slow, so for now the timeout would be set to 4
 * seconds. After CI runs it would be more clear if it should be
 * increased or decreased in the future.
 */
#define COL_PCIE_EMULATE_PERST_RETRY_ITER 20

static int ar_pci_emulate_perst(struct pci_dev *dev)
{
	ar_pci_driver_t *driver;
	size_t reset_offset;
	int func;
	size_t bar_size;
	uint32_t value;
	int i;

	AR_ASSERT(dev);
	func = PCI_FUNC(dev->devfn);

	if (func == COL_PCI_FUNC_1) {
		// Func1 has a smaller BAR size
		bar_size = COL_PCIE_FUNC_1_MMIO_MEMORY_BAR_SIZE;
	} else {
		// default BAR size
		bar_size = COL_PCIE_FUNC_0_MMIO_MEMORY_BAR_SIZE;
	}
	/* It was decided to make the PERST emulation by putting non-zero value to the
	 * last 4 bytes of the MMIO memory region.
	 */
	driver = pci_get_drvdata(dev);
	reset_offset = bar_size - 4;
	arfw_mem_util_iomem_write_32(driver->mmio_memory_bar, reset_offset,
				     0x1);

	// Look up the doorbell offset from the MMIO memory region.
	// Because the driver has not completed the handshake, we will need to grab
	// the bar memory region to find the doorbell offset.
	// We will assume that the doorbell offset has been properly set by firmware.
	ar_pci_bar_init_doorbell(dev);
	ar_pci_bar_ring_doorbell(dev);

	/* Put a delay to give time for the FW to reinit everything.
	 * Since the VP emulation is very slow, set the timeout to
	 * (COL_PCIE_EMULATE_PERST_DELAY_MS * COL_PCIE_EMULATE_PERST_RETRY_ITER)
	 * milliseconds.
	 */
	for (i = 0; i < COL_PCIE_EMULATE_PERST_RETRY_ITER; i++) {
		mdelay(COL_PCIE_EMULATE_PERST_DELAY_MS);
		/* Check that FW reinit everything. During initialization FW set the
		 * BAR space to 0.
		 */
		value = arfw_mem_util_iomem_read_32(driver->mmio_memory_bar,
						    reset_offset);
		if (!value)
			break;
	}

	return value ? -ETIMEDOUT : 0;
}

static const struct kernel_param_ops coleman_ready_param_ops = {
	.get = param_get_bool,
};

static bool coleman_ready_param;
module_param_cb(coleman_ready, &coleman_ready_param_ops, &coleman_ready_param,
		0664);

#define TIMEOUT_MIN_DUTY_CYCLE_MULTIPLE 4

static DEFINE_MUTEX(duty_cycle_freq_hz_lock);
static int duty_cycle_freq_hz_param = 0; // 0 means disabled

// static set of supported duty cycle frequencies
#define DUTY_CYCLE_OFF_FREQ_HZ 0
#define DUTY_CYCLE_LOWEST_FREQ_HZ 1
#define DUTY_CYCLE_LOWERER_FREQ_HZ 5
#define DUTY_CYCLE_LOWER_FREQ_HZ 15
#define DUTY_CYCLE_LOW_FREQ_HZ 45
#define DUTY_CYCLE_HIGH_FREQ_HZ 90

static int duty_cycle_freq_param_get(char *val, const struct kernel_param *kp)
{
	return sprintf(val, "%d\n", duty_cycle_freq_hz_param);
}

static int duty_cycle_freq_param_set(const char *val,
				     const struct kernel_param *kp)
{
	ar_pci_driver_t *driver = NULL;
	int ret, new_duty_cycle_freq_hz, min_timeout_ms;

	ret = kstrtoint(val, 10, &new_duty_cycle_freq_hz);
	if (ret) {
		AR_LOG_PCI_ERR(
			AR_LOG_SYSFS_WRITE,
			"Failed to parse duty cycle frequency [val: %s, err: %d]",
			val, ret);
		return ret;
	}

	switch (new_duty_cycle_freq_hz) {
	case DUTY_CYCLE_OFF_FREQ_HZ:
	case DUTY_CYCLE_LOWEST_FREQ_HZ:
	case DUTY_CYCLE_LOWERER_FREQ_HZ:
	case DUTY_CYCLE_LOWER_FREQ_HZ:
	case DUTY_CYCLE_LOW_FREQ_HZ:
	case DUTY_CYCLE_HIGH_FREQ_HZ:
		break;
	default:
		AR_LOG_PCI_ERR(AR_LOG_SYSFS_WRITE,
			       "Invalid duty cycle frequency [val: %d]",
			       new_duty_cycle_freq_hz);
		return -EINVAL;
	}

	if (new_duty_cycle_freq_hz == duty_cycle_freq_hz_param) {
		AR_LOG_PCI_INFO(AR_LOG_SYSFS_WRITE,
				"Duty cycle frequency is already set to %d",
				new_duty_cycle_freq_hz);
		return 0;
	}

	// we won't fail to set duty cycle, so update timeouts now
	if (new_duty_cycle_freq_hz > 0) {
		min_timeout_ms = MSEC_PER_SEC *
				 TIMEOUT_MIN_DUTY_CYCLE_MULTIPLE /
				 new_duty_cycle_freq_hz;
		min_timeout_ms = max(min_timeout_ms, 1);
		if (ctrl_ring_timeout_ms_param < min_timeout_ms)
			ctrl_ring_timeout_ms_param = min_timeout_ms;
	}

	// ALWAYS fetch duty cycle freq lock BEFORE registered drivers lock
	mutex_lock(&duty_cycle_freq_hz_lock);
	mutex_lock(&ar_pci_registered_drivers_lock);

	duty_cycle_freq_hz_param = new_duty_cycle_freq_hz;

	// Iterate over all registered drivers and update their duty cycle freq
	list_for_each_entry(driver, &ar_pci_registered_drivers_list, list) {
		// if link is not up, skip
		if (!driver->link_is_up)
			continue;

		/**
		 * It is possible that the driver is in the middle of a handshake
		 * and we are trying to update the duty cycle freq. The
		 * duty_cycle_freq_hz_lock protects us accidentally updating the
		 * duty cycle freq back to the old value.
		 */
		ret = ar_pci_bar_enable_duty_cycle(driver->dev,
						   duty_cycle_freq_hz_param,
						   ctrl_ring_timeout_ms_param);
		if (ret)
			AR_LOG_PCI_DEV_ERR(
				&driver->dev->dev, AR_LOG_SYSFS_WRITE,
				"Failed to enable the duty cycle [err: %d]",
				ret);
	}

	mutex_unlock(&ar_pci_registered_drivers_lock);
	mutex_unlock(&duty_cycle_freq_hz_lock);

	return 0;
}

static const struct kernel_param_ops duty_cycle_freq_param_ops = {
	.get = duty_cycle_freq_param_get,
	.set = duty_cycle_freq_param_set,
};

module_param_cb(duty_cycle_freq_hz, &duty_cycle_freq_param_ops,
		&duty_cycle_freq_hz_param, 0664);

static int ctrl_ring_timeout_ms_param_set(const char *val,
					  const struct kernel_param *kp)
{
	int ret, timeout_ms, min_timeout_ms;

	ret = kstrtoint(val, 10, &timeout_ms);
	if (ret) {
		AR_LOG_PCI_ERR(AR_LOG_SYSFS_WRITE,
			       "Failed to parse timeout [val: %s, err: %d]",
			       val, ret);
		return ret;
	}

	if (timeout_ms < 0) {
		AR_LOG_PCI_ERR(AR_LOG_SYSFS_WRITE, "Invalid timeout [val: %d]",
			       timeout_ms);
		return -EINVAL;
	}

	// no need to round up to duty cycle freq if it is disabled
	if (duty_cycle_freq_hz_param == 0) {
		ctrl_ring_timeout_ms_param = timeout_ms;
		return 0;
	}

	// calculate the minimum timeout
	min_timeout_ms = MSEC_PER_SEC * TIMEOUT_MIN_DUTY_CYCLE_MULTIPLE /
			 duty_cycle_freq_hz_param;
	min_timeout_ms = max(min_timeout_ms, 1);

	if (timeout_ms < min_timeout_ms) {
		AR_LOG_PCI_ERR(AR_LOG_SYSFS_WRITE,
			       "Timeout too small [val: %d, min: %d]",
			       timeout_ms, min_timeout_ms);
		return -EINVAL;
	}

	ctrl_ring_timeout_ms_param = timeout_ms;

	return 0;
}

static const struct kernel_param_ops ctrl_ring_timeout_ms_param_ops = {
	.get = param_get_int,
	.set = ctrl_ring_timeout_ms_param_set,
};

module_param_cb(ctrl_ring_timeout_ms, &ctrl_ring_timeout_ms_param_ops,
		&ctrl_ring_timeout_ms_param, 0664);

static int ar_pci_func_handshake(struct pci_dev *dev, bool error_is_fatal)
{
	int ret, func, err_shim;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	/**
	 * This lock is for two purposes:
	 * 1. We're touching power-related operations, so we should grab this lock.
	 * 2. Ensure we are the only one in the handshake, since it can occur from interrupt or from probe
	 */
	AR_ASSERT(mutex_is_locked(&driver->power.lock));

	func = PCI_FUNC(dev->devfn);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_INIT,
			   "Handshake attempt for function %d, fatal: %d", func,
			   error_is_fatal);

	/**
	 * This could happen in two scenarios:
	 * 1. While the interrupt is in transit, we saw the good handshake magic first and did the
	 * happy path first.
	 * 2. FW sent us more than one handshake interrupt.
	 * Scenario 2 really shouldn't happen (or so I'm told). Either way, we shouldn't do
	 * anything crazy, just log and skip.
	 */
	if (driver->link_is_up) {
		AR_LOG_PCI_DEV_INFO(&driver->dev->dev, AR_LOG_INIT,
				    "Handshake attempted after link is up");
		return 0;
	}

	ret = ar_pci_bar_handshake(dev);
	if (ret) {
		if (ret != -EAGAIN)
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
					   "Bar handshake failed [err: %d]",
					   ret);
		goto error_handshake;
	}

	// Reset so we start over on each probe
	if (!func) {
		ar_pci_func0_err = 0;

		if (driver->arp_ver.major < PCI_ARP_MAJOR_VER_MF_SUPPORT ||
		    (driver->arp_ver.major == PCI_ARP_MAJOR_VER_MF_SUPPORT &&
		     driver->arp_ver.minor < PCI_ARP_MINOR_VER_MF_SUPPORT))
			ar_pci_max_func = PCI_PRE_V5P1_MAX_FUNCTION;
		else if (driver->arp_ver.major ==
				 PCI_ARP_MAJOR_VER_MF_SUPPORT &&
			 driver->arp_ver.minor == PCI_ARP_MINOR_VER_MF_SUPPORT)
			ar_pci_max_func = PCI_V5P1_MAX_FUNCTION;
		else // all other cases (including future versions)
			ar_pci_max_func = PCI_MAX_FUNCTION;

		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
				    "Max func handshake supported is %d",
				    ar_pci_max_func);
	}

	// sys_name based cdevs support
	if ((driver->arp_ver.major > PCI_ARP_MAJOR_VER_SYS_NAME_SUPPORT) ||
	    ((driver->arp_ver.major == PCI_ARP_MAJOR_VER_SYS_NAME_SUPPORT) &&
	     (driver->arp_ver.minor >= PCI_ARP_MINOR_VER_SYS_NAME_SUPPORT)))
		driver->sys_name_supported = true;
	else
		driver->sys_name_supported = false;

	driver->link_is_up = true;

	if (ar_pci_get_arp_info(dev, ctrl_ring_timeout_ms_param) == NULL) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to get ARP info");
		ret = -EIO;
		goto error_info;
	}

	mutex_lock(&duty_cycle_freq_hz_lock);
	if (duty_cycle_freq_hz_param > 0) {
		ret = ar_pci_bar_enable_duty_cycle(dev,
						   duty_cycle_freq_hz_param,
						   ctrl_ring_timeout_ms_param);
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Failed to enable the duty cycle [err: %d]",
				ret);
		} else {
			AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
					    "Duty cycle enabled [freq: %d hz]",
					    duty_cycle_freq_hz_param);
		}
	}
	mutex_unlock(&duty_cycle_freq_hz_lock);

	ret = ar_pci_register_client(driver);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to register device [err: %d]", ret);
		goto error_register_client;
	}

	if (driver->aperture.addr) {
		if (func != APERTURE_TEST_FUNC && func != APERTURE_FDLA_FUNC) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Aperture allocated for unsupported function. [func: %d]",
				func);
			ar_pci_aperture_destroy(driver);
		} else {
			// Otherwise PCI function is [APERTURE_TEST_FUNC | APERTURE_FDLA_FUNC]
			ret = ar_pci_map_aperture_cmd(
				driver->dev, ctrl_ring_timeout_ms_param);
			AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
					    "Map aperture to fw, size %zu",
					    driver->aperture.size);
			if (ret)
				AR_LOG_PCI_DEV_ERR(
					&dev->dev, AR_LOG_INIT,
					"Failed to map aperture with FW (non-fatal) [err: %d]",
					ret);
		}
	}

#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
	// Initialize debug resources
	ret = ar_pci_debug_init(dev);
	if (ret)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to initialize debug interface [err: %d]", ret);
#endif // CONFIG ARFIRMWARE_PCI_DEBUG

	if (func == 0)
		coleman_ready_param = true;

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT, "Init OK");

	// Attempt to dispatch multifunction probe.
	// This is in case we need to share this function with another driver.
	// Getting -ENOTSUPP is expected if the function is not registered with another driver.
	// In cases when the shim probe fails, we still want to continue with the driver init.
	err_shim = arfw_shim_pci_probe(dev, driver->dev_id);
	if (err_shim && err_shim != -EOPNOTSUPP)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to dispatch multifunction probe [err: %d]",
			err_shim);

	return 0;

error_register_client:
error_info:
error_handshake:

	if (!error_is_fatal) {
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
				    "Handshake deferred");
		return 0;
	}

	/**
	 * Link should be suspended by F0 on handshake failure (to clean up, since F0 resumed the
	 * link in start protocol.) Non-zero functions haven't started comms yet so we don't need
	 * to fake PeRST. (We leave it up for deferred handshake.)
	 */
	ar_pci_link_suspend(driver);
	ar_pci_bar_disable_interrupt(dev);
	ar_pci_release_resources(driver);
	pci_disable_device(dev);
	if (func == 0)
		ar_pci_func0_err = ret;

	// mark link down since we're down
	driver->link_is_up = false;

	return ret;
}

static int ar_pci_func_handshake_handler(struct pci_dev *dev)
{
	// interrupt callback handshake is always fatal if it fails (we only get 1)
	return ar_pci_func_handshake(dev, /* error_is_fatal */ true);
}

static void ar_pci_suspend_wake_f0(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;
	int func;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	func = PCI_FUNC(dev->devfn);

	AR_ASSERT(func != 0);

	// Non-F0: signal we're ready and wait for F0 to suspend
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_SUSPEND,
		"Non-F0 function reached link suspend, waiting for F0");
	if (atomic_dec_and_test(&ar_pci_non_f0_active_count)) {
		AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_SUSPEND,
				   "Last non-F0 function ready, waking F0");
		wake_up(&ar_pci_f0_suspend_wait);
	}
}

static void ar_pci_suspend_wait_for_fns(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;
	int func;
	long timeout_jiffies;
	long ret;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	func = PCI_FUNC(dev->devfn);

	AR_ASSERT(func == 0);

	// wait for all other functions to suspend with timeout
	AR_LOG_PCI_DEV_DBG(
		&dev->dev, AR_LOG_SUSPEND,
		"F0 waiting for other functions to reach link suspend");
	timeout_jiffies = msecs_to_jiffies(AR_PCI_SUSPEND_RESUME_TIMEOUT_MS);
	ret = wait_event_timeout(ar_pci_f0_suspend_wait,
				 atomic_read(&ar_pci_non_f0_active_count) == 0,
				 timeout_jiffies);
	if (ret == 0) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_SUSPEND,
			"Timeout waiting for non-F0 functions to suspend. "
			"Functions may be suspending out of order, this could cause SMMU fault.");
	}

	// Clear the F0 is up flag and wake all waiting non-F0 functions
	atomic_set(&ar_pci_f0_is_up, 0);
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_SUSPEND,
			   "F0 link suspended, waking other functions");
	wake_up_all(&ar_pci_f0_resume_wait);
}

static void ar_pci_resume_wake_fns(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;
	int func;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	func = PCI_FUNC(dev->devfn);

	// Only one who can wake is f0
	AR_ASSERT(func == 0);

	atomic_set(&ar_pci_f0_is_up, 1);
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_RESUME,
			   "F0 resumed, waking other functions");
	wake_up_all(&ar_pci_f0_resume_wait);
}

static void ar_pci_resume_wait_for_f0(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;
	int func;
	long timeout_jiffies;
	long ret;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	func = PCI_FUNC(dev->devfn);

	// deadlock - can't wait on F0 for F0
	AR_ASSERT(func != 0);

	// Non-F0: wait for F0 start first before proceeding with timeout
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_RESUME,
			   "Non-F0 function waiting for F0 to come up");
	timeout_jiffies = msecs_to_jiffies(AR_PCI_SUSPEND_RESUME_TIMEOUT_MS);
	ret = wait_event_timeout(ar_pci_f0_resume_wait,
				 atomic_read(&ar_pci_f0_is_up) != 0,
				 timeout_jiffies);
	if (ret == 0) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_RESUME,
			"Timeout waiting for F0 to resume. "
			"Functions may be resuming out of order, this could cause AP crash.");
	}
	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_RESUME,
			   "F0 is up, proceeding with resume");

	// Increment non-F0 active count
	atomic_inc(&ar_pci_non_f0_active_count);
}

static int ar_pci_start_protocol(ar_pci_driver_t *driver)
{
	int ret, func;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	AR_ASSERT(mutex_is_locked(&driver->power.lock));
	dev = driver->dev;
	AR_ASSERT(dev);
	func = PCI_FUNC(dev->devfn);

	if (func && ar_pci_func0_err) {
		AR_LOG_PCI_DEV_INFO(
			&dev->dev, AR_LOG_INIT,
			"Warning: failed to start func=0 with err=%d, skipping [func: %d]",
			ar_pci_func0_err, func);
		return ar_pci_func0_err;
	} else if (func > ar_pci_max_func) {
		AR_LOG_PCI_DEV_INFO(
			&dev->dev, AR_LOG_INIT,
			"Warning: max func handshake supported is %d, skipping [func: %d]",
			ar_pci_max_func, func);
		return 0;
	}

	// we can't initialize non-zero fns until after f0 has completed init.
	if (func != 0)
		ar_pci_resume_wait_for_f0(driver);

	ret = ar_pci_link_resume(driver);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to resume the link [err: %d]", ret);
		goto error_exit;
	}

	// First, move device to D0 power state before accessing config space
	ret = pci_set_power_state(driver->dev, PCI_D0);
	if (ret < 0) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to put device in D0 [err: %d]", ret);
		goto error_exit;
	}

	/*
	 * Before we enable the device, restore the saved config state. We need to do this, since
	 * we manually save the state on stop. See the comments in the stop protocol function.
	 *
	 * We call it after we set power state to D0, because the device should be powered before
	 * we attempt to access the config space. (Otherwise, for example, if the device is in
	 * D3cold, this won't work). This needs to happen after link is up, since otherwise we can't
	 * correctly access the config space. This also needs to happen before pci_assign_resource,
	 * since pci_assign_resource will access the BARs and those live in the config space.
	 */
	pci_restore_state(dev);

	atomic_set(&driver->next_seq_num, 0);
	atomic_set(&driver->data_irq_count, 0);
	atomic_set(&driver->handshake_irq_count, 0);

	// Unblock client ring queues as we're setting them up again
	ar_pci_unblock_client_resources(driver);

	// Override set max segment size, previously set by generic pci probe code
	dma_set_max_seg_size(&dev->dev, UINT_MAX);

	// Set the DMA mask before mapping BARs
	ret = dma_set_mask(&dev->dev, DMA_BIT_MASK(32));
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to set DMA mask [err: %d]", ret);
		goto error_assign_resource;
	}

	/*
	 * Ideally, pci_restore_state should let us not do this, but we need to update the BARs on
	 * resume (follow up tracked in T248739349). Internally, pci_assign_resource will re-map the
	 * BARs using pci_update_resource, and since the bar size didn't change, it also won't bother
	 * moving the VA of the mapping. We should NOT call pci_remove_resource, this is managed by
	 * the QC pcie bus driver. (Actually, the bus driver already assigns the resource during probe,
	 * but it is OK to re-assign for the reasons above, and we need to re-assign on resume or else
	 * AP crashes.)
	 */
	ret = pci_assign_resource(dev, driver->mmio_interrupt_bar_region);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to assign interrupt bar resource [err: %d]",
			ret);
		goto error_assign_resource;
	}

	// Warning: see above pci_assign_resource call before touching.
	ret = pci_assign_resource(dev, driver->mmio_memory_bar_region);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to assign memory bar resource [err: %d]", ret);
		goto error_assign_resource;
	}

	ret = pci_enable_device(dev);
	if (ret != 0) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to enable device [err: %d]", ret);
		goto error_acquire_resources;
	}

	// The PCI bus is awake now, the hardware should be minimally poke-able.
	driver->power.state = AR_PCI_DEV_POWER_STATE_ON;

	// Map and acquire resources after enabling the pci dev
	ret = ar_pci_acquire_resources(driver);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to acquire resources [err: %d]",
				   ret);
		goto error_acquire_resources;
	}

	// All resources are assigned and device is enabled. Enable DMA before handshake.
	pci_set_master(dev);

	// set up boot interrupt handler (don't need to do for func 0)
	if (func != 0)
		ar_pci_bar_set_handshake_irq_handler(
			dev, ar_pci_func_handshake_handler);

	ret = ar_pci_bar_enable_interrupt(dev);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to enable interrupt [err: %d]", ret);
		goto error_interrupt;
	}

	// func 0 handshake failure is non-deferrable, and fatal on failure
	ret = ar_pci_func_handshake(
		dev, /* error_is_fatal */ PCI_FUNC(dev->devfn) == 0);

	// handshake function cleans up everything on failure, just return
	if (ret)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "ar_pci_func_handshake failed [err: %d]",
				   ret);
	else if (func ==
		 0) // ret == 0 && func == 0: we should wake waiting functions
		ar_pci_resume_wake_fns(driver);

	if ((driver->arp_ver.major > PCI_ARP_MAJOR_VER_DEL_ALL_SUPPORT) ||
	    ((driver->arp_ver.major >= PCI_ARP_MAJOR_VER_DEL_ALL_SUPPORT) &&
	     (driver->arp_ver.minor >= PCI_ARP_MINOR_VER_DEL_ALL_SUPPORT)))
		driver->group_shutdown_supported = true;
	else
		driver->group_shutdown_supported = false;

	return ret;

error_interrupt:
	ar_pci_release_resources(driver);
error_acquire_resources:
	pci_disable_device(dev);
error_assign_resource:
	ar_pci_link_suspend(driver);
error_exit:
	if (func == 0)
		ar_pci_func0_err = ret;

	return ret;
}

static void ar_pci_stop_protocol(ar_pci_driver_t *driver,
				 enum ar_queue_shutdown_reason reason)
{
	struct pci_dev *dev;
	int err, func;

	AR_ASSERT(driver);
	AR_ASSERT(mutex_is_locked(&driver->power.lock));
	dev = driver->dev;
	AR_ASSERT(dev);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_SHUTDOWN,
			   "Stop protocol [reason %d, current state %d]",
			   reason, driver->power.state);

	if (driver->power.state == AR_PCI_DEV_POWER_STATE_OFF)
		return;

	func = PCI_FUNC(dev->devfn);

	// Attempt to dispatch multifunction remove.
	// This is in case we need to share this function with another driver.
	// Getting -ENOTSUPP is expected if the function is not registered with another driver.
	err = arfw_shim_pci_remove(dev);
	if (err && err != -EOPNOTSUPP)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_SHUTDOWN,
			"Failed to dispatch multifunction remove [err: %d]",
			err);

	/**
	 * If link is down, this means we are stopping the protocol before the handshake interrupt ever came in.
	 * In this case, we skip cleaning up the parts that were never initialized.
	 */
	if (driver->link_is_up) {
		if (PCI_FUNC(dev->devfn) == 0)
			coleman_ready_param = false;

		// TODO(T223140695): Remove when we have the PERST functionalty
		if (atomic_read_acquire(&driver->duty_cycle.enabled)) {
			err = ar_pci_bar_disable_duty_cycle(
				dev, ctrl_ring_timeout_ms_param);
			if (err)
				AR_LOG_PCI_DEV_ERR(
					&dev->dev, AR_LOG_SHUTDOWN,
					"Failed to disable the duty cycle [err: %d]",
					err);
			else
				AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN,
						    "Duty cycle disabled");
		}

		err = ar_pci_unregister_client(
			driver, reason == AR_QUEUE_SHUTDOWN_DRIVER_STATE);
		if (err != -ENODEV) {
			/* Because of deferred handshake there is a possibility that:
			 *   - handshake is failed on the get arp info
			 *   - device is not registered
			 *   - link_is_up set to true
			 * If this is the case there is nothing to clean up or remove.
			 */
#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
			ar_pci_debug_remove(dev);
#endif // CONFIG_ARFIRMWARE_PCI_DEBUG
			ar_pci_release_client_resources(driver, reason);
		}

		if (coleman_emulate_perst) {
			// The main purpose of the emulated perst is to "reset" MCU so it
			// will be able to rehandshake and also didn't make DMA/MSI after
			// we free the memory. So we make this fake perst, before we free
			// the memory.
			// The PERST is needed in two cases:
			// 1. QEMU/VP doesn't have any PERST support, so we are indeed
			// emulating it.
			// 2. Coleman HW: we should not do it, since there is real PERST,
			// but we could not prevent FW of doing DMA/MSI. So we always
			// will have a race after we clean the memory and perform a real
			// PERST on RC. In the bad scenario it leads to SMMU crash, since
			// FW tries to DMA the IOMMU memory, which was already freed. The
			// proper solution would be to call "pci_clear_master", but Coleman
			// is not respecting it. There should be support in Coleman2. Once
			// we have we will not need it for the real hardware case. So here
			// we use fake PERST to prevent FW of doing DMA/MSI interaction
			// before we fully clean up the memory and resources.
			// TODO: T247519877 to address with "pci_clear_master"
			err = ar_pci_emulate_perst(dev);
			if (err)
				AR_LOG_PCI_DEV_ERR(
					&dev->dev, AR_LOG_SHUTDOWN,
					"FW didn't restore its state after PERST emulation [err: %d]",
					err);
			else
				AR_LOG_PCI_DEV_INFO(
					&dev->dev, AR_LOG_SHUTDOWN,
					"FW restored its state after PERST emulation - signaled");
		}
	}

	// before we touch pci suspend power ops, we need to make sure the ordering is correct.
	// f0 should suspend last, so we need to wait for all other functions to suspend before
	// we continue here.
	if (func == 0)
		ar_pci_suspend_wait_for_fns(driver);

	/**
	 * The suspend flow moves pci fn device into D3hot to save power. Before doing it, the config
	 * registers should be saved. In fact it is not recommended to do it in the driver suspend code.
	 * Here is what doc is saying (PCI Power Management):
	 *
	 *   3.1.2. suspend()
	 *
	 *   The suspend() callback is only executed during system suspend, after prepare() callbacks
	 *   have been executed for all devices in the system.
	 *
	 *   This callback is expected to quiesce the device and prepare it to be put into a low-power
	 *   state by the PCI subsystem. It is not required (in fact it even is not recommended) that a
	 *   PCI driver’s suspend() callback save the standard configuration registers of the device,
	 *   prepare it for waking up the system, or put it into a low-power state. All of these operations
	 *   can very well be taken care of by the PCI subsystem, without the driver’s participation.
	 *
	 *   However, in some rare case it is convenient to carry out these operations in a PCI driver.
	 *   Then, pci_save_state(), pci_prepare_to_sleep(), and pci_set_power_state() should be used to
	 *   save the device’s standard configuration registers, to prepare it for system wakeup (if
	 *   necessary), and to put it into a low-power state, respectively.
	 *
	 * The problem is that if we are not doing it the FW is not resumed properly after handshake
	 * and could get an DMA failure.
	 */
	err = pci_save_state(driver->dev);
	if (err)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_SHUTDOWN,
				   "Failed to save pci state [err: %d]", err);

	/**
	 * Just to be consistent with the comment above. Since we are handling the registers state
	 * manually, let's do the same for power state.
	 * Move pci fn device into D3hot to disable it. This should be done after any ctrl
	 * ring messages required for cleanup.
	 */
	err = pci_set_power_state(driver->dev, PCI_D3hot);
	if (err < 0)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_SHUTDOWN,
				   "Failed to put device in D3hot [err: %d]",
				   err);

	/**
	 * We should link down in stop protocol. FW (function 0) expects us to assert PeRST to signal
	 * to stop dma, in place of the bus master bit. This is why we link down after we stop link work
	 * but *before* we de-allocate resources. It's possible for incoming interrupts or dma to race,
	 * since without perst down FW may attempt DMA / interrupts, which will smmu fault or crash the
	 * AP if the regions are unmapped or the interrupts are freed.
	 *
	 * Non-zero functions utilize the fake perst.
	 */
	ar_pci_link_suspend(driver);
	ar_pci_bar_disable_interrupt(dev);
	ar_pci_release_resources(driver);

	pci_disable_device(dev);
	driver->power.state = AR_PCI_DEV_POWER_STATE_OFF;
	driver->link_is_up = false;

	if (func != 0)
		ar_pci_suspend_wake_f0(driver);
}

static void ar_pci_link_suspend(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;
	int func;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	func = PCI_FUNC(dev->devfn);

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
	if (func == 0)
		msm_pcie_pm_link_suspend(driver);
#else
	(void)func;
#endif // CONFIG_ARFIRMWARE_MSM_PCIE
}

static int ar_pci_link_resume(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;
	int func, err = 0;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	func = PCI_FUNC(dev->devfn);

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
	if (func == 0) {
		err = msm_pcie_pm_link_resume(driver);
		if (err)
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Failed to resume the link [err: %d]", err);

		return err;
	}
#else
	(void)func;
#endif // CONFIG_ARFIRMWARE_MSM_PCIE

	return err;
}

static int ar_pci_probe(struct pci_dev *dev, const struct pci_device_id *dev_id)
{
	int err;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	AR_ASSERT(dev_id);

	driver = devm_kzalloc(&dev->dev, sizeof(ar_pci_driver_t), GFP_KERNEL);
	if (driver == NULL)
		return -ENOMEM;

	mutex_init(&driver->data_ring_lock);
	driver->dev = dev;
	driver->dev_id = dev_id;

	driver->handshake_irq_index = msi_handshake_index;
	driver->mmio_interrupt_bar_region = 0;
	driver->mmio_memory_bar_region = 1;

	mutex_lock(&ar_pci_registered_drivers_lock);
	list_add(&driver->list, &ar_pci_registered_drivers_list);
	mutex_unlock(&ar_pci_registered_drivers_lock);

	INIT_LIST_HEAD(&driver->cmd_reply_list);
	spin_lock_init(&driver->cmd_reply_lock);
	INIT_LIST_HEAD(&driver->client_send_queues.list_node);
	mutex_init(&driver->client_send_queues.lock);
	INIT_LIST_HEAD(&driver->client_rcv_queues.list_node);
	mutex_init(&driver->client_rcv_queues.lock);
	mutex_init(&driver->power.lock);
	mutex_init(&driver->duty_cycle.lock);

	driver->power.state = AR_PCI_DEV_POWER_STATE_OFF;

	pci_set_drvdata(dev, driver);

	spin_lock_init(&driver->doorbell_lock);
	hrtimer_init(&driver->doorbell_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	driver->doorbell_timer.function = ar_pci_doorbell_timer_callback;

	mutex_lock(&driver->power.lock);

	err = ar_pci_start_protocol(driver);

	mutex_unlock(&driver->power.lock);

	if (err == 0) {
		/**
		* To support runtime PM ops, we need to call pm_runtime_put on a successful probe.
		* We should be careful that we don't get a pm_runtime_put before this point, otherwise
		* we might miss a suspend. (Such as exposing sysfs nodes for runtime pm)
		*/
		pm_runtime_put_noidle(&dev->dev);

		err = ar_pci_add_driver_sysfs_nodes(driver);
		if (err)
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"Failed to add sysfs nodes [err: %d]", err);

		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
				    "Succeeded to probe the driver");
		return 0;
	}

	AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
			   "Failed to probe the driver, err=%d", err);

	mutex_lock(&ar_pci_registered_drivers_lock);
	list_del(&driver->list);
	mutex_unlock(&ar_pci_registered_drivers_lock);

	devm_kfree(&dev->dev, driver);

	return err;
}

static void ar_pci_remove(struct pci_dev *dev)
{
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_SHUTDOWN, "Removing pcie device");

	// When hrtimer_cancel() returns, the caller can be sure that the timer
	// is no longer active and that its expiration function is not running
	// anywhere in the system.
	// Note we might still have some race here anyway.
	hrtimer_cancel(&driver->doorbell_timer);

	// Disable handshake interrupts before grabbing the mutex to avoid deadlocks.
	// In case we get a deferred handshake interrupt only when we tear things down.
	atomic_set(&driver->handshake_irq_count, 1);

	// ar_pci_stop_protocol does not remove the sysfs nodes, so we need to do it here.
	ar_pci_remove_driver_sysfs_nodes(driver);

	// Unbinding while the link is down is fine, just skip everything...
	mutex_lock(&driver->power.lock);

	if (driver->power.state < AR_PCI_DEV_POWER_STATE_ON) {
		goto unlock_power;
	}

	// the stop protocol is blocking, so we are assuming the rmmod will call f0 last.
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN, "Shutdown started");
	ar_pci_stop_protocol(driver, AR_QUEUE_SHUTDOWN_DRIVER_STATE);
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN, "Shutdown finished");

unlock_power:
	mutex_unlock(&driver->power.lock);

	/**
	 * During probe, we called pm_runtime_put_noidle, so we want a matching pm_runtime_get_noresume during
	 * remove. We use the noresume variant since there's no need to wake up the driver / invoke
	 * resume callbacks if we are removing the driver.
	 */
	pm_runtime_get_noresume(&dev->dev);

	mutex_lock(&ar_pci_registered_drivers_lock);
	list_del(&driver->list);
	mutex_unlock(&ar_pci_registered_drivers_lock);

	devm_kfree(&dev->dev, driver);
}

static int ar_pci_suspend(struct device *dev)
{
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(to_pci_dev(dev));
	AR_ASSERT(driver);

	// Disable handshake interrupts before grabbing the mutex to avoid deadlocks.
	// In case we get a deferred handshake interrupt only when we tear things down.
	atomic_set(&driver->handshake_irq_count, 1);

	mutex_lock(&driver->power.lock);
	if (driver->power.state < AR_PCI_DEV_POWER_STATE_ON) {
		// Suspending while the link is down is fine, just skip everything...
		goto unlock_power;
	}

	AR_LOG_PCI_DEV_INFO(dev, AR_LOG_SUSPEND, "Suspend started");
	ar_pci_stop_protocol(driver, AR_QUEUE_SHUTDOWN_POWER_STATE);
	AR_LOG_PCI_DEV_INFO(dev, AR_LOG_SUSPEND, "Suspend finished");

unlock_power:
	mutex_unlock(&driver->power.lock);

	return 0;
}

static int ar_pci_resume(struct device *dev)
{
	int err = 0;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(to_pci_dev(dev));
	AR_ASSERT(driver);

	mutex_lock(&driver->power.lock);
	if (driver->power.state >= AR_PCI_DEV_POWER_STATE_ON) {
		AR_LOG_PCI_DEV_INFO(dev, AR_LOG_RESUME, "Already up");
		goto unlock_power;
	}

	AR_LOG_PCI_DEV_INFO(dev, AR_LOG_RESUME, "Resume started");
	err = ar_pci_start_protocol(driver);
	if (err)
		goto unlock_power;
	AR_LOG_PCI_DEV_INFO(dev, AR_LOG_RESUME, "Resume finished");

unlock_power:
	mutex_unlock(&driver->power.lock);

	return err;
}

struct ar_pci_resume_work {
	struct delayed_work work;
	ar_pci_driver_t *driver;
};

static void ar_pci_deferred_resume_work(struct work_struct *work)
{
	struct ar_pci_resume_work *resume_work =
		container_of(work, struct ar_pci_resume_work, work.work);
	ar_pci_driver_t *driver = resume_work->driver;

	AR_ASSERT(driver);

	AR_LOG_PCI_DEV_INFO(&driver->dev->dev, AR_LOG_RESUME,
			    "Deferred resume work started");

	ar_pci_resume(&driver->dev->dev);

	AR_LOG_PCI_DEV_INFO(&driver->dev->dev, AR_LOG_RESUME,
			    "Deferred resume work completed");

	kfree(resume_work);
}

static int ar_pci_deferred_resume(struct device *dev)
{
	ar_pci_driver_t *driver;
	struct ar_pci_resume_work *resume_work;
	const unsigned long delay_ms = 5 * 1000;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(to_pci_dev(dev));
	AR_ASSERT(driver);

	resume_work = kzalloc(sizeof(*resume_work), GFP_KERNEL);
	if (!resume_work) {
		AR_LOG_PCI_DEV_ERR(dev, AR_LOG_RESUME,
				   "Failed to allocate resume work structure");
		return -ENOMEM;
	}

	resume_work->driver = driver;
	INIT_DELAYED_WORK(&resume_work->work, ar_pci_deferred_resume_work);

	if (!schedule_delayed_work(&resume_work->work,
				   msecs_to_jiffies(delay_ms))) {
		AR_LOG_PCI_DEV_ERR(dev, AR_LOG_RESUME,
				   "Failed to schedule deferred resume work");
		kfree(resume_work);
		return -EINVAL;
	}

	AR_LOG_PCI_DEV_INFO(dev, AR_LOG_RESUME,
			    "Scheduled deferred resume with delay %lu ms",
			    delay_ms);

	return 0;
}

static pci_ers_result_t ar_pci_error_detected(struct pci_dev *dev,
					      pci_channel_state_t state)
{
	// For now let's just keep track of fatals/nonfatals/correctables we see.
	// Treat everything like it's bad news tho.
	AR_LOG_PCI_ERR(AR_LOG_CTRL, "Error detected: %d, but no handler",
		       state);
	return PCI_ERS_RESULT_DISCONNECT;
}

static struct pci_error_handlers ar_pci_error_handler = {
	.error_detected = ar_pci_error_detected,
};
static const struct dev_pm_ops ar_pci_pm_ops = {
	.suspend = ar_pci_suspend,
	.runtime_suspend = ar_pci_suspend,
	.resume = ar_pci_deferred_resume,
	.runtime_resume = ar_pci_resume,
};

static struct pci_driver ar_pci_driver = {
	.name = PCI_DRIVER_COLEMAN,
	.id_table = ar_pci_id,
	.probe = ar_pci_probe,
	.remove = ar_pci_remove,
	.shutdown = ar_pci_remove,
	.err_handler = &ar_pci_error_handler,
	.driver = {
		.pm = &ar_pci_pm_ops,
	},
};

static int ar_pci_pm_suspend(const char *val, const struct kernel_param *kp)
{
	int err;
	ar_pci_driver_t *driver = NULL;

	(void)val;
	(void)kp;

	AR_LOG_PCI_DBG(AR_LOG_SUSPEND,
		       "Suspend for all registered devices started");
	err = mutex_lock_interruptible(&ar_pci_registered_drivers_lock);
	if (err) {
		AR_LOG_PCI_ERR(AR_LOG_SUSPEND,
			       "Locking drivers for suspend failed, err: %d",
			       err);
		return err;
	}

	list_for_each_entry(driver, &ar_pci_registered_drivers_list, list) {
		AR_ASSERT(driver->dev);

		if (!pm_runtime_active(&driver->dev->dev))
			continue;

		err = pm_runtime_put_sync(&driver->dev->dev);
		if (err < 0) {
			AR_LOG_PCI_DEV_ERR(&driver->dev->dev, AR_LOG_SUSPEND,
					   "Suspend failed, err: %d", err);
			break;
		}
	}

	mutex_unlock(&ar_pci_registered_drivers_lock);
	AR_LOG_PCI_DBG(AR_LOG_SUSPEND,
		       "Suspend for all registered devices finished");

	return err;
}

static int ar_pci_pm_resume(const char *val, const struct kernel_param *kp)
{
	int err;
	bool link_is_up = false;
	ar_pci_driver_t *driver = NULL;

	(void)val;
	(void)kp;

	AR_LOG_PCI_DBG(AR_LOG_RESUME,
		       "Resume for all registered devices started");
	err = mutex_lock_interruptible(&ar_pci_registered_drivers_lock);
	if (err) {
		AR_LOG_PCI_ERR(AR_LOG_RESUME,
			       "Locking drivers for resume failed, err: %d",
			       err);
		return err;
	}

	/**
	 * For some reason, the link needs to come up before the runtime PM triggers resume.
	 * If the link is brought up after the PM core triggers PCI resume, the link will appear
	 * to come up properly but DMA will stop working shortly after.
	 * This is a hack to get around that. QC case 08256515 contains more information for
	 * our joint debugging of this issue.
	 *
	 * This hook does not alleviate system suspend / resume.
	 *
	 * The list is iterated in reverse order to have f0 resume first.
	 */
	list_for_each_entry_reverse(driver, &ar_pci_registered_drivers_list,
				    list) {
		AR_ASSERT(driver->dev);

		if (!link_is_up) {
			AR_ASSERT(PCI_FUNC(driver->dev->devfn) == 0);
			ar_pci_link_resume(driver);
			link_is_up = true;
		}

		if (pm_runtime_active(&driver->dev->dev))
			continue;

		err = pm_runtime_get_sync(&driver->dev->dev);
		if (err < 0) {
			// if device has already been requested to resume, skip and move on.
			if (err == -EINPROGRESS) {
				AR_LOG_PCI_DEV_INFO(
					&driver->dev->dev, AR_LOG_RESUME,
					"Resume already in progress, continuing");
				continue;
			}

			AR_LOG_PCI_DEV_ERR(&driver->dev->dev, AR_LOG_RESUME,
					   "Resume failed, err: %d", err);
		}
	}

	mutex_unlock(&ar_pci_registered_drivers_lock);
	AR_LOG_PCI_DBG(AR_LOG_RESUME,
		       "Resume for all registered devices finished");

	return err;
}

/**
 * ar_pci_wake_functions() - Wake specific PCIe functions from D3hot state
 * @func_bit_mask: Bitmask of functions to wake (bit N = function N)
 *
 * Called when firmware sends CP_WAKE_REQ message to request AP to wake
 * specific functions from D3hot to D0 state.
 *
 * Return: 0 on success, negative error code on failure
 */
int ar_pci_wake_functions(uint8_t func_bit_mask)
{
	int err, ret = 0;
	ar_pci_driver_t *driver;

	AR_LOG_PCI_DBG(AR_LOG_RESUME,
		       "Wake requested for functions [mask: 0x%02x]",
		       func_bit_mask);

	err = mutex_lock_interruptible(&ar_pci_registered_drivers_lock);
	if (err)
		return err;

	list_for_each_entry(driver, &ar_pci_registered_drivers_list, list) {
		int func = PCI_FUNC(driver->dev->devfn);

		/* Skip if function not in mask or already active */
		if (!(func_bit_mask & (1 << func)))
			continue;
		if (pm_runtime_active(&driver->dev->dev))
			continue;

		err = pm_runtime_get_sync(&driver->dev->dev);
		if (err < 0 && err != -EINPROGRESS) {
			AR_LOG_PCI_DEV_ERR(
				&driver->dev->dev, AR_LOG_RESUME,
				"Wake failed for function %d [err: %d]", func,
				err);
			ret = err;
		}
	}

	mutex_unlock(&ar_pci_registered_drivers_lock);
	return ret;
}

static int ar_pci_pm_reset(const char *val, const struct kernel_param *kp)
{
	int err;

	err = ar_pci_pm_suspend(val, kp);
	if (err)
		return err;

	return ar_pci_pm_resume(val, kp);
}

static int ar_pci_pm_active(char *buf, const struct kernel_param *kp)
{
	int err, count = 0;
	ar_pci_driver_t *driver = NULL;

	(void)kp;

	err = mutex_lock_interruptible(&ar_pci_registered_drivers_lock);
	if (err) {
		AR_LOG_PCI_DBG(AR_LOG_SYSFS_READ,
			       "Unable to get devices lock, err: %d", err);
		return err;
	}

	list_for_each_entry(driver, &ar_pci_registered_drivers_list, list) {
		if (driver->power.state == AR_PCI_DEV_POWER_STATE_ON)
			count++;
	}

	mutex_unlock(&ar_pci_registered_drivers_lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", count);
}

static const struct kernel_param_ops ar_pci_pm_suspend_ops = {
	.set = ar_pci_pm_suspend,
};

static const struct kernel_param_ops ar_pci_pm_resume_ops = {
	.set = ar_pci_pm_resume,
};

static const struct kernel_param_ops ar_pci_pm_reset_ops = {
	.set = ar_pci_pm_reset,
};

static const struct kernel_param_ops ar_pci_pm_active_ops = {
	.get = ar_pci_pm_active,
};

// Use a dummy param since the ops are write-only and are cbs.
// The value is discarded so we can use the same one for all of them.
static int ar_pci_pm_param;

module_param_cb(pm_suspend, &ar_pci_pm_suspend_ops, &ar_pci_pm_param, 0664);
module_param_cb(pm_resume, &ar_pci_pm_resume_ops, &ar_pci_pm_param, 0664);
module_param_cb(pm_reset, &ar_pci_pm_reset_ops, &ar_pci_pm_param, 0664);
module_param_cb(pm_active, &ar_pci_pm_active_ops, &ar_pci_pm_param, 0664);

MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, ar_pci_id);
module_pci_driver(ar_pci_driver);
