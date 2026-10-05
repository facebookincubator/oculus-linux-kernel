// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file msm_pcie.c
 *
 * @brief Defines special operations for Qualcom MSM PCIe
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/workqueue.h>

#include <arfw_config.h>
#include <ar_common.h>
#include <arfw_log.h>

#include "ar_pci_int.h"
#include "msm_pcie.h"

#define PM_OPTIONS_DEFAULT 0

// ASPM operations may be slow, assuming slower that a PERST
// op that may take 100 ms. Use a safe x4 cool off time.
#define ASPM_OP_COOL_OFF_MS 400

// "Imported" from pci-msm.c.
// Note they do export the function using this enum
enum msm_pcie_debugfs_option {
	MSM_PCIE_OUTPUT_PCIE_INFO,
	MSM_PCIE_DISABLE_LINK,
	MSM_PCIE_ENABLE_LINK,
	MSM_PCIE_DISABLE_ENABLE_LINK,
	MSM_PCIE_DUMP_SHADOW_REGISTER,
	MSM_PCIE_DISABLE_L0S,
	MSM_PCIE_ENABLE_L0S,
	MSM_PCIE_DISABLE_L1,
	MSM_PCIE_ENABLE_L1,
	MSM_PCIE_DISABLE_L1SS,
	MSM_PCIE_ENABLE_L1SS,
	MSM_PCIE_ENUMERATION,
	MSM_PCIE_READ_PCIE_REGISTER,
	MSM_PCIE_WRITE_PCIE_REGISTER,
	MSM_PCIE_DUMP_PCIE_REGISTER_SPACE,
	MSM_PCIE_ALLOCATE_DDR_MAP_LBAR,
	MSM_PCIE_FREE_DDR_UNMAP_LBAR,
	MSM_PCIE_OUTPUT_DDR_LBAR_ADDRESS,
	MSM_PCIE_CONFIGURE_LOOPBACK,
	MSM_PCIE_SETUP_LOOPBACK_IATU,
	MSM_PCIE_READ_DDR,
	MSM_PCIE_READ_LBAR,
	MSM_PCIE_WRITE_DDR,
	MSM_PCIE_WRITE_LBAR,
	MSM_PCIE_DISABLE_AER,
	MSM_PCIE_ENABLE_AER,
	MSM_PCIE_GPIO_STATUS,
	MSM_PCIE_ASSERT_PERST,
	MSM_PCIE_DEASSERT_PERST,
	MSM_PCIE_KEEP_RESOURCES_ON,
	MSM_PCIE_FORCE_GEN1,
	MSM_PCIE_FORCE_GEN2,
	MSM_PCIE_FORCE_GEN3,
	MSM_PCIE_MAX_DEBUGFS_OPTION
};

static unsigned long link_resume_attempt_delay_ms;
module_param(link_resume_attempt_delay_ms, ulong, 0664);

static unsigned long link_resume_attempt_max_count = 3;
module_param(link_resume_attempt_max_count, ulong, 0664);

static unsigned long link_suspend_attempt_delay_ms = 1000;
module_param(link_suspend_attempt_delay_ms, ulong, 0664);

static unsigned long link_suspend_attempt_max_count = 3;
module_param(link_suspend_attempt_max_count, ulong, 0664);

int msm_pcie_pm_link_down(ar_pci_driver_t *driver)
{
	int err;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;

	err = msm_pcie_pm_control(MSM_PCIE_HANDLE_LINKDOWN, dev->bus->number,
				  dev, NULL, PM_OPTIONS_DEFAULT);
	if (err)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CTRL,
			"Failed to trigger a link down event, bus=%d [err: %d]",
			dev->bus->number, err);

	return err;
}

int msm_pcie_pm_link_resume(ar_pci_driver_t *driver)
{
	int err = 0;
	const unsigned long max_attempts = link_resume_attempt_max_count;
	unsigned long attempts = max_attempts;
	unsigned long delay_ms = link_resume_attempt_delay_ms;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;

	while (attempts--) {
		err = msm_pcie_pm_control(MSM_PCIE_RESUME, dev->bus->number,
					  dev, NULL, PM_OPTIONS_DEFAULT);

		if (err == 0) {
			AR_LOG_PCI_DEV_INFO(
				&dev->dev, AR_LOG_INIT,
				"Resumed the pcie bus link, bus=%d attempts=%lu",
				dev->bus->number, (max_attempts - attempts));
			break;
		}

		// Only adding sleeps between attempts, not for the last one.
		if (attempts && delay_ms)
			msleep(delay_ms);
	}

	if (err)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to resume the pcie bus link, bus=%d attempts=%lu delay_ms=%lu [err: %d]",
			dev->bus->number, max_attempts, delay_ms, err);

	return err;
}

int msm_pcie_pm_link_suspend(ar_pci_driver_t *driver)
{
	int err = 0;
	const unsigned long max_attempts = link_suspend_attempt_max_count;
	unsigned long attempts = max_attempts;
	unsigned long delay_ms = link_suspend_attempt_delay_ms;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	while (attempts--) {
#ifdef CONFIG_ARFIRMWARE_MSM_PCIE_FORCE_SUSPEND
		err = msm_pcie_pm_control(MSM_PCIE_SUSPEND, dev->bus->number,
					  dev, NULL,
					  MSM_PCIE_CONFIG_FORCE_SUSP);
#else
		err = msm_pcie_pm_control(MSM_PCIE_SUSPEND, dev->bus->number,
					  dev, NULL, PM_OPTIONS_DEFAULT);
#endif

		if (err == 0) {
			driver->link_is_up = false;
			AR_LOG_PCI_DEV_INFO(
				&dev->dev, AR_LOG_SHUTDOWN,
				"Suspended the pcie bus link, bus=%d attempts=%lu",
				dev->bus->number, (max_attempts - attempts));
			break;
		}

		// Only adding sleeps between attempts, not for the last one.
		if (attempts && delay_ms)
			msleep(delay_ms);
	}

	if (err)
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_INIT,
			"Failed to suspend the pcie bus link, bus=%d attempts=%lu delay_ms=%lu [err: %d]",
			dev->bus->number, max_attempts, delay_ms, err);

	return err;
}

struct msm_pcie_offload_event {
	struct work_struct work;
	ar_pci_driver_t *driver;
	msm_pcie_notify_callback_t cb;
	enum msm_pcie_event type;
};

static void msm_pcie_offload_handle(struct work_struct *work)
{
	struct msm_pcie_offload_event *event;

	AR_ASSERT(work);
	event = container_of(work, struct msm_pcie_offload_event, work);

	AR_ASSERT(event);
	AR_ASSERT(event->cb);
	AR_ASSERT(event->driver);
	event->cb(event->driver, event->type);

	devm_kfree(&event->driver->dev->dev, event);
}

static void msm_pcie_offload_callback(struct msm_pcie_notify *notify)
{
	struct pci_dev *dev;
	ar_pci_driver_t *driver;
	msm_pcie_notify_callback_t cb;
	struct msm_pcie_offload_event *event;

	if (!notify) {
		AR_LOG_PCI_ERR(AR_LOG_CTRL,
			       "Invalid input to pcie notification callback");
		return;
	}

	dev = (struct pci_dev *)notify->user;
	if (!dev) {
		AR_LOG_PCI_ERR(AR_LOG_CTRL,
			       "Invalid user in pcie notification callback");
		return;
	}

	driver = pci_get_drvdata(dev);
	AR_ASSERT(driver);

	cb = (msm_pcie_notify_callback_t)notify->data;
	if (!cb) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CTRL,
			"Invalid data in pcie notification callback");
		return;
	}

	event = devm_kzalloc(&dev->dev, sizeof(struct msm_pcie_offload_event),
			     GFP_ATOMIC);
	if (!event) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CTRL,
			"Failed to allocate offload event, callback not executed");
		return;
	}

	event->cb = cb;
	event->driver = driver;
	event->type = notify->event;

	INIT_WORK(&event->work, msm_pcie_offload_handle);
	schedule_work(&event->work);
}

int msm_pcie_subscribe_events(ar_pci_driver_t *driver,
			      msm_pcie_notify_callback_t cb, uint32_t events)
{
	int err;
	struct pci_dev *dev;
	struct msm_pcie_register_event *event;

	AR_ASSERT(driver);
	AR_ASSERT(cb);
	dev = driver->dev;

	event = devm_kzalloc(&dev->dev, sizeof(struct msm_pcie_register_event),
			     GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	event->user = dev;
	event->callback = msm_pcie_offload_callback;
	event->events = events;
	event->mode = MSM_PCIE_TRIGGER_CALLBACK;
	event->notify.data = cb;

	err = msm_pcie_register_event(event);
	if (err) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CTRL,
			"Failed to subscribe to pcie events=%d, bus=%d [err: %d]",
			events, dev->bus->number, err);
		devm_kfree(&dev->dev, event);
		return err;
	}

	driver->link_event = event;

	return 0;
}

int msm_pcie_unsubscribe_events(ar_pci_driver_t *driver)
{
	int err;
	struct pci_dev *dev;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	if (!driver->link_event)
		return -EINVAL;

	err = msm_pcie_deregister_event(driver->link_event);
	if (err) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_CTRL,
			"Failed to unsubscribe from pcie link bus=%d [err: %d]",
			dev->bus->number, err);
	}

	devm_kfree(&dev->dev, driver->link_event);
	driver->link_event = NULL;

	return err;
}

int msm_pcie_set_aspm_state(ar_pci_driver_t *driver, bool enable)
{
	struct pci_dev *dev;
	int err;

	AR_ASSERT(driver);
	dev = driver->dev;
	AR_ASSERT(dev);

	err = mutex_lock_interruptible(&driver->aspm_lock);
	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_POWER,
				   "Failed to get the aspm lock [err: %d]",
				   err);
		return err;
	}

	if (enable) {
		// yes, the "debug_info" function changes state
		err = msm_pcie_debug_info(dev, MSM_PCIE_ENABLE_L1, 0, 0, 0, 0);
		if (err) {
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_POWER,
					   "Failed to enable L1 [err: %d]",
					   err);
			goto out;
		}
		err = msm_pcie_debug_info(dev, MSM_PCIE_ENABLE_L1SS, 0, 0, 0,
					  0);
		if (err) {
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_POWER,
					   "Failed to enable L1SS [err: %d]",
					   err);
			goto out;
		}
	} else { // disable
		err = msm_pcie_debug_info(dev, MSM_PCIE_DISABLE_L1, 0, 0, 0, 0);
		if (err) {
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_POWER,
					   "Failed to enable L1 [err: %d]",
					   err);
			goto out;
		}
		err = msm_pcie_debug_info(dev, MSM_PCIE_DISABLE_L1SS, 0, 0, 0,
					  0);
		if (err) {
			AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_POWER,
					   "Failed to enable L1SS [err: %d]",
					   err);
			goto out;
		}
	}

	// AVO needs this time to propagate the hw changes.
	msleep(ASPM_OP_COOL_OFF_MS);

out:
	mutex_unlock(&driver->aspm_lock);
	return err;
}

int msm_pcie_get_aspm_state(ar_pci_driver_t *driver, bool *state)
{
	struct pci_dev *dev;
	uint32_t lnkctrl_val;
	int err;

	AR_ASSERT(state);
	AR_ASSERT(driver);
	dev = driver->dev;

	err = pci_read_config_dword(dev, dev->pcie_cap + PCI_EXP_LNKCTL,
				    &lnkctrl_val);
	if (err) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_POWER,
			"Failed to read the linkctrl register [err: %d]", err);
		goto out;
	}

	// Let's assume that if L1 it is enabled, L1SS will be enabled too
	*state = !!(lnkctrl_val & (PCI_EXP_LNKCTL_ASPM_L1));
out:
	return err;
}
