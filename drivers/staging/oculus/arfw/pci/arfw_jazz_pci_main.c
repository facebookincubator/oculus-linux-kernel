// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_jazz_pci.c
 *
 * @brief Driver for PCI based Jazz AI accelerator
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

#include <arfw_log.h>
#include <arfw_shim.h>
#include <arfw_pci.h>

#include "ar_pci_int.h"
#include "ar_pci_bar.h"
#include "ar_pci_sysfs.h"
#include "arfw_device.h"
#include "control_rings.h"
#include "data_ring.h"

static bool enable_aperture = false;
module_param(enable_aperture, bool, 0664);

// This is the index of the handshake interrupt. -1 means it is not used.
static int msi_handshake_index = -1;
module_param(msi_handshake_index, int, 0444);

// Hack to get around the fact QEMU has the address space hardcoded.
// Func 0 sets it based on the ARP version.
// All other functions will continue probing based on its value.
static int ar_pci_max_func;
static int ar_pci_func0_err;

// On Jazz we only have 1 function enabled which is on SMCU
// Ignore the rest, let's not support pre-MF configs for Jazz.
#define PCI_MAX_FUNCTION 0

static LIST_HEAD(ar_pci_registered_drivers_list);
static DEFINE_MUTEX(ar_pci_registered_drivers_lock);

static void ar_pci_link_suspend(ar_pci_driver_t *driver);
static int ar_pci_link_resume(ar_pci_driver_t *driver);

static const struct pci_device_id ar_pci_id[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_META, PCI_DEVICE_ID_META) },
	{ /* end: zero */ }
};

#define ARFW_DEVID_LEN 16
static const char *ar_dev_id = "AR-JAZZ-PCI";

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
	char devid[ARFW_DEVID_LEN];

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	snprintf(devid, ARFW_DEVID_LEN, "%s-%d", ar_dev_id,
		 dev->devfn & PCI_FUNCTION_MASK);
	err = arfw_shim_cdev_register(&dev->dev, devid, &ar_dev_ops,
				      &driver->client_ops, dev);

	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_DEV_REG,
				   "Failed to register device %s [err: %d]",
				   ar_dev_id, err);
	} else {
		AR_ASSERT(driver->client_ops);
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_DEV_REG,
				    "Registered device %s OK", ar_dev_id);
	}

	return err;
}

static int ar_pci_unregister_client(ar_pci_driver_t *driver, bool module_remove)
{
	int err;
	struct pci_dev *dev;
	char devid[ARFW_DEVID_LEN];

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	snprintf(devid, ARFW_DEVID_LEN, "%s-%d", ar_dev_id,
		 dev->devfn & PCI_FUNCTION_MASK);
	err = arfw_shim_cdev_unregister(devid, NULL, module_remove);

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

	ar_pci_release_client_resource(driver, &driver->client_send_queues,
				       reason);
	ar_pci_release_client_resource(driver, &driver->client_rcv_queues,
				       reason);
}

static void ar_pci_unblock_client_resources(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN,
			    "Unblocking down the client resources");

	ar_pci_unblock_client_resource(&driver->client_send_queues);
	ar_pci_unblock_client_resource(&driver->client_rcv_queues);
}

static bool emulate_perst;
module_param(emulate_perst, bool, 0664);

/* For the emulated targets, like diamond_emu/amethyst_emu, there is no GPIO or other
 * side channel to send a PERST signal to CP. Because of this it is
 * required to full reboot VP emulator, after every insmod. This is the
 * PERST emulation support for the targets which require it.
 * QEMU<->VP does not support getting BAR size from VP.
 * So hardcode the BAR sizes here.
 */
#define PCIE_FUNC_0_MMIO_MEMORY_BAR_SIZE 0x1000
#define PCI_FUNC_1 1
#define PCIE_FUNC_1_MMIO_MEMORY_BAR_SIZE 0x800
#define PCIE_EMULATE_PERST_DELAY_MS 200

/* VP emulation is slow, so for now the timeout would be set to 4
 * seconds. After CI runs it would be more clear if it should be
 * increased or decreased in the future.
 */
#define PCIE_EMULATE_PERST_RETRY_ITER 20

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

	if (func == PCI_FUNC_1) {
		// Func1 has a smaller BAR size
		bar_size = PCIE_FUNC_1_MMIO_MEMORY_BAR_SIZE;
	} else {
		// default BAR size
		bar_size = PCIE_FUNC_0_MMIO_MEMORY_BAR_SIZE;
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
	 * (PCIE_EMULATE_PERST_DELAY_MS * PCIE_EMULATE_PERST_RETRY_ITER)
	 * milliseconds.
	 */
	for (i = 0; i < PCIE_EMULATE_PERST_RETRY_ITER; i++) {
		mdelay(PCIE_EMULATE_PERST_DELAY_MS);
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

static const struct kernel_param_ops ready_param_ops = {
	.get = param_get_bool,
};

static bool ready_param;
module_param_cb(ready, &ready_param_ops, &ready_param, 0664);

#define TIMEOUT_MIN_DUTY_CYCLE_MULTIPLE 4
unsigned long ctrl_ring_timeout_ms_param = 1000;

static DEFINE_MUTEX(duty_cycle_freq_hz_lock);
static int duty_cycle_freq_hz_param = 0; // 0 means disabled

// static set of supported duty cycle frequencies
#define DUTY_CYCLE_OFF_FREQ_HZ 0
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
	int ret, new_duty_cycle_freq_hz;

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
		ar_pci_max_func = PCI_MAX_FUNCTION;
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT,
				    "Max func handshake supported is %d",
				    ar_pci_max_func);
	}

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

	ret = ar_pci_add_driver_sysfs_nodes(driver);
	if (ret)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to add sysfs nodes [err: %d]", ret);

#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
	// Initialize debug resources
	ret = ar_pci_debug_init(dev);
	if (ret)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to initialize debug interface [err: %d]", ret);
#endif // CONFIG ARFIRMWARE_PCI_DEBUG

	if (func == 0)
		ready_param = true;

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

	ar_pci_bar_disable_interrupt(dev);
	ar_pci_release_resources(driver);
	pci_disable_device(dev);
	ar_pci_link_suspend(driver);
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

static int ar_pci_start_protocol(ar_pci_driver_t *driver)
{
	int ret, func;
	struct pci_dev *dev;
	bool bypass_fake_perst;

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

	ret = ar_pci_link_resume(driver);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to resume the link [err: %d]", ret);
		goto error_exit;
	}

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

	ret = pci_assign_resource(dev, driver->mmio_interrupt_bar_region);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to assign interrupt bar resource [err: %d]",
			ret);
		goto error_assign_resource;
	}

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

	// When we have deferred handshake enabled we cannot do fake PERST on
	// all non-zero functions, the MCUs are not booted yet. Instead we will get
	// an MSI and finish probing then and MCUs will be initialized correctly.
	// We still do fake PERST on deprobe to signal MCUs to re-init.
	bypass_fake_perst = driver->handshake_irq_index >= 0 && func > 0;

	if (emulate_perst && !bypass_fake_perst) {
		ret = ar_pci_emulate_perst(dev);
		if (ret) {
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_INIT,
				"FW didn't restore its state after PERST emulation [err: %d]",
				ret);
			goto error_perst;
		}
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

	return ret;

error_interrupt:
error_perst:
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
	int err;

	AR_ASSERT(driver);
	AR_ASSERT(mutex_is_locked(&driver->power.lock));
	dev = driver->dev;
	AR_ASSERT(dev);

	/**
	 * If link is down, this means we are stopping the protocol before the handshake interrupt ever came in.
	 * In this case, we skip cleaning up the parts that were never initialized.
	 */
	if (driver->link_is_up) {
		if (PCI_FUNC(dev->devfn) == 0)
			ready_param = false;

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

			ar_pci_remove_driver_sysfs_nodes(driver);
#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
			ar_pci_debug_remove(dev);
#endif // CONFIG_ARFIRMWARE_PCI_DEBUG
			ar_pci_release_client_resources(driver, reason);
		}
	}
	if (driver->link_is_up && emulate_perst) {
		err = ar_pci_emulate_perst(dev);
		if (err)
			AR_LOG_PCI_DEV_ERR(
				&dev->dev, AR_LOG_SHUTDOWN,
				"FW didn't restore its state after PERST emulation [err: %d]",
				err);
	}
	if (reason != AR_QUEUE_SHUTDOWN_LINK_STATE)
		ar_pci_link_suspend(driver);
	ar_pci_bar_disable_interrupt(dev);
	ar_pci_release_resources(driver);

	pci_disable_device(dev);
	driver->power.state = AR_PCI_DEV_POWER_STATE_OFF;
}

static void ar_pci_link_suspend(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);
}

static int ar_pci_link_resume(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	return 0;
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
	int err;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	// When hrtimer_cancel() returns, the caller can be sure that the timer
	// is no longer active and that its expiration function is not running
	// anywhere in the system.
	// Note we might still have some race here anyway.
	hrtimer_cancel(&driver->doorbell_timer);

	// Disable handshake interrupts before grabbing the mutex to avoid deadlocks.
	// In case we get a deferred handshake interrupt only when we tear things down.
	atomic_set(&driver->handshake_irq_count, 1);

	mutex_lock(&driver->power.lock);
	if (driver->power.state < AR_PCI_DEV_POWER_STATE_ON) {
		// Unbinding while the link is down is fine, just skip everything...
		goto unlock_power;
	}

	// Attempt to dispatch multifunction remove.
	// This is in case we need to share this function with another driver.
	// Getting -ENOTSUPP is expected if the function is not registered with another driver.
	err = arfw_shim_pci_remove(dev);
	if (err && err != -EOPNOTSUPP)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_SHUTDOWN,
			"Failed to dispatch multifunction remove [err: %d]",
			err);

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN, "Shutdown started");
	ar_pci_stop_protocol(driver, AR_QUEUE_SHUTDOWN_DRIVER_STATE);
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN, "Shutdown finished");

unlock_power:
	mutex_unlock(&driver->power.lock);

	mutex_lock(&ar_pci_registered_drivers_lock);
	list_del(&driver->list);
	mutex_unlock(&ar_pci_registered_drivers_lock);

	devm_kfree(&dev->dev, driver);
}

static int ar_pci_suspend(struct pci_dev *dev, pm_message_t state)
{
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	// Disable handshake interrupts before grabbing the mutex to avoid deadlocks.
	// In case we get a deferred handshake interrupt only when we tear things down.
	atomic_set(&driver->handshake_irq_count, 1);

	mutex_lock(&driver->power.lock);
	if (driver->power.state < AR_PCI_DEV_POWER_STATE_ON) {
		// Suspending while the link is down is fine, just skip everything...
		goto unlock_power;
	}

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SUSPEND, "Suspend started");
	ar_pci_stop_protocol(driver, AR_QUEUE_SHUTDOWN_POWER_STATE);
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SUSPEND, "Suspend finished");

unlock_power:
	mutex_unlock(&driver->power.lock);

	return 0;
}

static int ar_pci_resume(struct pci_dev *dev)
{
	int err = 0;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	mutex_lock(&driver->power.lock);
	if (driver->power.state >= AR_PCI_DEV_POWER_STATE_ON) {
		// Resume with a link already up, not sure how, but skip...
		goto unlock_power;
	}

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_RESUME, "Resume started");
	err = ar_pci_start_protocol(driver);
	if (err)
		goto unlock_power;
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_RESUME, "Resume finished");

unlock_power:
	mutex_unlock(&driver->power.lock);

	return err;
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

static struct pci_driver ar_pci_driver = {
	.name = PCI_DRIVER_JAZZ,
	.id_table = ar_pci_id,
	.probe = ar_pci_probe,
	.remove = ar_pci_remove,
	.suspend = ar_pci_suspend,
	.resume = ar_pci_resume,
	.shutdown = ar_pci_remove,
	.err_handler = &ar_pci_error_handler,
};

static int ar_pci_pm_suspend(const char *val, const struct kernel_param *kp)
{
	int err;
	ar_pci_driver_t *driver = NULL;
	pm_message_t pm_msg;

	(void)val;
	(void)kp;

	AR_LOG_PCI_DBG(AR_LOG_SUSPEND,
		       "Suspend for all registered devices started");
	err = mutex_lock_interruptible(&ar_pci_registered_drivers_lock);
	if (err) {
		AR_LOG_PCI_ERR(AR_LOG_SUSPEND, "Suspend failed, err: %d", err);
		return err;
	}

	pm_msg.event = PM_EVENT_SUSPEND;
	list_for_each_entry(driver, &ar_pci_registered_drivers_list, list) {
		AR_ASSERT(driver->dev);
		err = ar_pci_suspend(driver->dev, pm_msg);
		if (err) {
			AR_LOG_PCI_ERR(AR_LOG_SUSPEND,
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
	ar_pci_driver_t *driver = NULL;

	(void)val;
	(void)kp;

	AR_LOG_PCI_DBG(AR_LOG_RESUME,
		       "Resume for all registered devices started");
	err = mutex_lock_interruptible(&ar_pci_registered_drivers_lock);
	if (err) {
		AR_LOG_PCI_DBG(AR_LOG_RESUME, "Resume failed, err: %d", err);
		return err;
	}

	list_for_each_entry(driver, &ar_pci_registered_drivers_list, list) {
		AR_ASSERT(driver->dev);
		err = ar_pci_resume(driver->dev);
		if (err) {
			AR_LOG_PCI_DBG(AR_LOG_RESUME, "Resume failed, err: %d",
				       err);
			break;
		}
	}

	mutex_unlock(&ar_pci_registered_drivers_lock);
	AR_LOG_PCI_DBG(AR_LOG_RESUME,
		       "Resume for all registered devices finished");

	return err;
}

/* Stub - CP_WAKE_REQ not supported on Jazz platform */
int ar_pci_wake_functions(uint8_t func_bit_mask)
{
	return 0;
}

static int ar_pci_pm_reset(const char *val, const struct kernel_param *kp)
{
	int err;

	err = ar_pci_pm_suspend(val, kp);
	if (err)
		return err;

	return ar_pci_pm_resume(val, kp);
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

// Use a dummy param since the ops are write-only and are cbs.
// The value is discarded so we can use the same one for all of them.
static int ar_pci_pm_param;

module_param_cb(pm_suspend, &ar_pci_pm_suspend_ops, &ar_pci_pm_param, 0664);
module_param_cb(pm_resume, &ar_pci_pm_resume_ops, &ar_pci_pm_param, 0664);
module_param_cb(pm_reset, &ar_pci_pm_reset_ops, &ar_pci_pm_param, 0664);

MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, ar_pci_id);
module_pci_driver(ar_pci_driver);
