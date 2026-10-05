// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_acro_pci_main.c
 *
 * @brief PCIe driver for PCI based AR accelerator
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
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

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
#include "msm_pcie.h"
#endif // CONFIG_ARFIRMWARE_MSM_PCIE

// These are used to track all the drivers for pm ops emulation.
// Since the ops will be executed outside of pci dev scope.
static LIST_HEAD(ar_pci_registered_drivers_list);
static DEFINE_MUTEX(ar_pci_registered_drivers_lock);

static void ar_pci_link_suspend(ar_pci_driver_t *driver, bool pm_link);
static int ar_pci_link_resume(ar_pci_driver_t *driver, bool pm_link);

static const struct pci_device_id ar_pci_id[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_META, PCI_DEVICE_ID_META) },
	{ /* end: zero */ }
};

static const char *ar_dev_id = "AR-ACRO-PCI";

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
};

unsigned long ctrl_ring_timeout_ms_param = 1000;
static const struct kernel_param_ops ctrl_ring_timeout_ms_param_ops = {
	.get = param_get_int,
	.set = param_set_int,
};
module_param_cb(ctrl_ring_timeout_ms, &ctrl_ring_timeout_ms_param_ops,
		&ctrl_ring_timeout_ms_param, 0664);

/**
 * Handles Acro alive signal being de-asserted which signals that
 * the link is down and we need to notify the RC to handle it. This
 * is done in order to support recovery even in L1/L1ss states.
 */
static irqreturn_t ar_pci_alive_irq_handler(int irq, void *data)
{
	ar_pci_driver_t *driver = (ar_pci_driver_t *)data;
	struct pci_dev *dev = driver->dev;

	(void)irq;

	AR_LOG_PCI_DEV_INFO(
		&dev->dev, AR_LOG_CTRL,
		"Acro alive signal de-asserted, triggering link down");
	ar_pci_set_link_state(driver, false);
	msm_pcie_pm_link_down(driver);

	return IRQ_HANDLED;
}

static int ar_pci_register_client(ar_pci_driver_t *driver)
{
	int err;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	err = arfw_shim_cdev_register(&dev->dev, ar_dev_id, &ar_dev_ops,
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

static void ar_pci_unregister_client(ar_pci_driver_t *driver,
				     bool module_remove)
{
	int err;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	err = arfw_shim_cdev_unregister(ar_dev_id, NULL, module_remove);

	if (err)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_SHUTDOWN,
				   "Failed to unregister device [err: %d]",
				   err);
	else
		AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN,
				    "Unregistered device");
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

static int ar_pci_start_protocol(ar_pci_driver_t *driver, bool pm_link)
{
	int ret;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	// If we assert wake on Avogadro we need to
	// de-assert it even if we failed to resume the link.
	// The link resumption will do the required sleeps and attempts
	// so we do not race with Avogadro here - can be a sync call.
	// Even though Avogadro will raise GPIO to signal when it's ready to linkup.
	ret = ar_pci_link_resume(driver, pm_link);

	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to resume the link [err: %d]", ret);
		return ret;
	}

	atomic_set(&driver->next_seq_num, 0);
	atomic_set(&driver->data_irq_count, 0);
	driver->msi_int_num = 0;

	// Unblock client ring queues as we're setting them up again
	ar_pci_unblock_client_resources(driver);

	// Override set max segment size, previously set by generic pci probe code
	dma_set_max_seg_size(&dev->dev, UINT_MAX);

	// Set the DMA mask before mapping BARs
	ret = pci_set_dma_mask(dev, DMA_BIT_MASK(32));
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to set DMA mask [err: %d]", ret);
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
		goto error_assign_resource;
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

	// Perform handsake and stash the bar memory area
	ret = ar_pci_bar_handshake(dev);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Handshake failed [err: %d]", ret);
		goto error_handshake;
	}

	driver->link_is_up = true;

	ret = ar_pci_bar_enable_interrupt(dev);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to enable interrupt [err: %d]", ret);
		goto error_interrupt;
	}

	if (ar_pci_get_arp_info(dev, ctrl_ring_timeout_ms_param) == NULL) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to get ARP info");
		ret = -EIO;
		goto error_info;
	}

	ret = ar_pci_register_client(driver);
	if (ret) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to register device [err: %d]", ret);
		goto error_register_client;
	}

#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
	// Initialize debug resources
	ret = ar_pci_debug_init(dev);
	if (ret)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to initialize debug interface [err: %d]", ret);
#endif // CONFIG ARFIRMWARE_PCI_DEBUG

	// Published only here: the char device is registered and the protocol is
	// up, so this is the first point at which a client open() can succeed.
	ar_pci_set_link_state(driver, true);

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT, "Init OK");
	return 0;

error_register_client:
error_info:
error_interrupt:
	ar_pci_bar_disable_interrupt(dev);
error_handshake:
	ar_pci_release_resources(driver);
error_acquire_resources:
	pci_disable_device(dev);
error_assign_resource:
	ar_pci_link_suspend(driver, pm_link);
	return ret;
}

static void ar_pci_stop_protocol(ar_pci_driver_t *driver,
				 enum ar_queue_shutdown_reason reason)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
	ar_pci_debug_remove(dev);
#endif // CONFIG_ARFIRMWARE_PCI_DEBUG

	ar_pci_set_link_state(driver, false);
	ar_pci_unregister_client(driver,
				 reason == AR_QUEUE_SHUTDOWN_DRIVER_STATE);
	ar_pci_release_client_resources(driver, reason);
	ar_pci_link_suspend(driver, /* pm_link = */ true);
	ar_pci_bar_disable_interrupt(dev);
	ar_pci_release_resources(driver);

	pci_disable_device(dev);
	driver->power.state = AR_PCI_DEV_POWER_STATE_OFF;
}

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
static bool linkup_protocol_start = true;
module_param(linkup_protocol_start, bool, 0664);

static void ar_pci_handle_linkup(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	if (!linkup_protocol_start) {
		AR_LOG_PCI_DEV_INFO(
			&dev->dev, AR_LOG_CTRL,
			"Protocol start is disabled for linkup events, skipping...");
		return;
	}

	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	mutex_lock(&driver->power.lock);
	if (driver->link_is_up)
		goto unlock_power;

	if (ar_pci_start_protocol(driver, /* pm_link = */ true))
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_CTRL,
				   "Failed to start the protocol");

unlock_power:
	mutex_unlock(&driver->power.lock);
}

static bool linkdown_protocol_stop = true;
module_param(linkdown_protocol_stop, bool, 0664);

static void ar_pci_handle_linkdown(ar_pci_driver_t *driver)
{
	struct pci_dev *dev;
	pcie_protocol_version_t version;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);
	version = driver->arp_ver;

	if (!linkdown_protocol_stop) {
		AR_LOG_PCI_DEV_INFO(
			&dev->dev, AR_LOG_CTRL,
			"Protocol stop is disabled for linkdown events, skipping...");
		return;
	}

	mutex_lock(&driver->power.lock);
	if (!driver->link_is_up ||
	    driver->power.state < AR_PCI_DEV_POWER_STATE_ON) {
		// Handle linkdown when the link is down already is a bit weird, but fine, skip...
		goto unlock_power;
	}

	// Mark that we do not have a link anymore.
	// This allows us to bypass the firmware notifications and
	// speeds up the link tear down. We assume that firmware died and
	// will clean up on it's own.
	driver->link_is_up = false;
	ar_pci_stop_protocol(driver, AR_QUEUE_SHUTDOWN_LINK_STATE);

unlock_power:
	mutex_unlock(&driver->power.lock);
}

static void ar_pci_handle_link_event(ar_pci_driver_t *driver,
				     enum msm_pcie_event event)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	AR_LOG_PCI_DEV_DBG(&dev->dev, AR_LOG_CTRL, "Handle link event type=%d",
			   event);
	switch (event) {
	case MSM_PCIE_EVENT_LINKDOWN:
		ar_pci_handle_linkdown(driver);
		break;
	case MSM_PCIE_EVENT_LINKUP:
		/* fallthrough */
	case MSM_PCIE_EVENT_WAKEUP:
		ar_pci_handle_linkup(driver);
		break;
	default:
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_CTRL,
				   "Unexpected link event type=%d", event);
	}
}
#endif // CONFIG_ARFIRMWARE_MSM_PCIE

static void ar_pci_link_suspend(ar_pci_driver_t *driver, bool pm_link)
{
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT, "Suspending the bus link");
	if (pm_link)
		msm_pcie_pm_link_suspend(driver);
#endif // CONFIG_ARFIRMWARE_MSM_PCIE
}

static int ar_pci_link_resume(ar_pci_driver_t *driver, bool pm_link)
{
	int err = 0;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_INIT, "Resuming the bus link");
	if (pm_link)
		err = msm_pcie_pm_link_resume(driver);
#endif // CONFIG_ARFIRMWARE_MSM_PCIE

	return err;
}

static int ar_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int err = 0;
	int alive_irq;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);

	driver = devm_kzalloc(&dev->dev, sizeof(ar_pci_driver_t), GFP_KERNEL);
	if (driver == NULL)
		return -ENOMEM;

	mutex_init(&driver->data_ring_lock);
	driver->dev = dev;

	// Acro doesn't support deferred handshake
	driver->handshake_irq_index = -1;
	// Acro doesn't support interrupts via the BAR
	driver->mmio_interrupt_bar_region = -1;
	driver->mmio_memory_bar_region = 0;

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
	driver->link_event = NULL;
#endif // CONFIG_ARFIRMWARE_MSM_PCIE

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

#ifdef CONFIG_ARFIRMWARE_PCI_GPIO_DOORBELL
	driver->acro_doorbell_gpio =
		devm_gpiod_get(&dev->dev, "acro-doorbell", GPIOD_OUT_LOW);
	if (IS_ERR(driver->acro_doorbell_gpio)) {
		err = PTR_ERR(driver->acro_doorbell_gpio);
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to acquire the acro_doorbell GPIO [err: %d]",
			err);
		goto error_doorbell;
	}
#endif // CONFIG_ARFIRMWARE_PCI_GPIO_DOORBELL

	alive_irq = of_irq_get_byname(dev->dev.of_node, "acro-alive");
	if (alive_irq < 0) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to get the alive IRQ [err: %d]",
				   alive_irq);
		goto error_doorbell;
	}

	err = devm_request_irq(&dev->dev, alive_irq, &ar_pci_alive_irq_handler,
			       IRQF_TRIGGER_FALLING | IRQF_SHARED,
			       "arfw_acro_alive_irq", driver);
	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to request the alive IRQ [err: %d]",
				   err);
		goto error_doorbell;
	}

	err = enable_irq_wake(alive_irq);
	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to enable wake for the alive IRQ");
		goto error_doorbell;
	}

	pci_set_drvdata(dev, driver);

	spin_lock_init(&driver->doorbell_lock);
	hrtimer_init(&driver->doorbell_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	driver->doorbell_timer.function = ar_pci_doorbell_timer_callback;

	/**
	 * We will start the protocol on linkup event instead of probe to make
	 * things uniform across state transitions.
	 */
	// Created here rather than in start_protocol so the node, and any poller
	// blocked on it, survives the link cycling.
	err = ar_pci_add_driver_sysfs_nodes(driver);
	if (err)
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_INIT,
				   "Failed to add sysfs nodes [err: %d]", err);

	err = msm_pcie_subscribe_events(driver, ar_pci_handle_link_event,
					MSM_PCIE_EVENT_LINKDOWN |
						MSM_PCIE_EVENT_LINKUP |
						MSM_PCIE_EVENT_WAKEUP);

	if (err == 0)
		return 0;

	ar_pci_remove_driver_sysfs_nodes(driver);

error_doorbell:
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

	// When hrtimer_cancel() returns, the caller can be sure that the timer
	// is no longer active and that its expiration function is not running
	// anywhere in the system.
	// Note we might still have some race here anyway.
	hrtimer_cancel(&driver->doorbell_timer);

	mutex_lock(&driver->power.lock);

	// Unbinding while the link is down is fine, just skip everything...
	if (driver->power.state < AR_PCI_DEV_POWER_STATE_ON)
		goto unlock_power;

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN, "Shutdown started");
	ar_pci_stop_protocol(driver, AR_QUEUE_SHUTDOWN_DRIVER_STATE);
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_SHUTDOWN, "Shutdown finished");

unlock_power:
	mutex_unlock(&driver->power.lock);
	ar_pci_remove_driver_sysfs_nodes(driver);
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

	mutex_lock(&driver->power.lock);

	// Suspending while the link is down is fine, just skip everything...
	if (driver->power.state < AR_PCI_DEV_POWER_STATE_ON)
		goto unlock_power;

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

	// Resume with a link already up, not sure how, but skip...
	if (driver->power.state >= AR_PCI_DEV_POWER_STATE_ON)
		goto unlock_power;

	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_RESUME, "Resume started");
	err = ar_pci_start_protocol(driver, /* pm_link = */ true);
	if (err)
		goto unlock_power;
	AR_LOG_PCI_DEV_INFO(&dev->dev, AR_LOG_RESUME, "Resume finished");

unlock_power:
	mutex_unlock(&driver->power.lock);

	return err;
}

static void ar_pci_shutdown(struct pci_dev *dev)
{
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
	.name = PCI_DRIVER_ACRO,
	.id_table = ar_pci_id,
	.probe = ar_pci_probe,
	.remove = ar_pci_remove,
	.suspend = ar_pci_suspend,
	.resume = ar_pci_resume,
	.shutdown = ar_pci_shutdown,
	.err_handler = &ar_pci_error_handler,
};

static int ar_pci_pm_suspend(const char *val, const struct kernel_param *kp)
{
	int err;
	ar_pci_driver_t *driver;
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
	ar_pci_driver_t *driver;

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

/* Stub - CP_WAKE_REQ not supported on Acro platform */
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
