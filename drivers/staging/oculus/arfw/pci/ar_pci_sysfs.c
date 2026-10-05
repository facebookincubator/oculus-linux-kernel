// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file ar_pci_sysfs.c
 *
 * @brief Implementation of AR PCIe sysfs nodes.
 *
 * @details PCIe layer sysfs nodes should be initialized and removed in this file. 
 *
 *******************************************************************************/

#include "ar_pci_bar.h"
#include "ar_pci_sysfs.h"
#include "arfw_log.h"

#include <linux/device.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/sysfs.h>

static ssize_t power_state_ctl_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	AR_ASSERT(driver);

	return sprintf(buf, "%s\n",
		       pm_runtime_active(dev) ?
				     "active" :
				     (pm_runtime_suspended(dev) ? "suspended" :
								  "unknown"));
}

static ssize_t power_state_ctl_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	AR_ASSERT(driver);

	if (sysfs_streq(buf, "suspend")) {
		ret = pm_runtime_put_sync_suspend(dev);
	} else if (sysfs_streq(buf, "resume")) {
		ret = pm_runtime_get_sync(dev);
	} else {
		AR_LOG_PCI_DEV_ERR(dev, AR_LOG_SYSFS_WRITE,
				   "Unsupported power ctrl command: %s", buf);
		return -EINVAL;
	}

	/**
	 * pm_runtime_put_sync_suspend and pm_runtime_get_sync return 1 if the device is already suspended
	 * or already resumed, respectively, and -errno on error.
	 */
	if (ret == 1) {
		AR_LOG_PCI_DEV_INFO(dev, AR_LOG_SYSFS_WRITE,
				    "Power state is already set to %s", buf);
	} else if (ret < 0) {
		AR_LOG_PCI_DEV_ERR(dev, AR_LOG_SYSFS_WRITE,
				   "Failed to set power state: %d", ret);
		return ret;
	}

	AR_LOG_PCI_DEV_INFO(dev, AR_LOG_SYSFS_WRITE, "Power state set to %s",
			    buf);

	return count;
}

static DEVICE_ATTR_RW(power_state_ctl);

static ssize_t link_state_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ar_pci_driver_t *driver = dev_get_drvdata(dev);

	AR_ASSERT(driver);

	return sprintf(buf, "%s\n",
		       atomic_read(&driver->link_state.up) ? "up" : "down");
}

static DEVICE_ATTR_RO(link_state);

void ar_pci_set_link_state(ar_pci_driver_t *device, bool up)
{
	const int val = up ? 1 : 0;

	AR_ASSERT(device);

	if (atomic_xchg(&device->link_state.up, val) == val)
		return;

	// sysfs_notify() may sleep and the down edge comes from the alive IRQ,
	// so notify the dirent cached when the node was created.
	if (device->link_state.kn)
		sysfs_notify_dirent(device->link_state.kn);
}

int ar_pci_add_driver_sysfs_nodes(ar_pci_driver_t *device)
{
	int err;

	err = device_create_file(&device->dev->dev, &dev_attr_power_state_ctl);
	if (err)
		return err;

	err = device_create_file(&device->dev->dev, &dev_attr_link_state);
	if (err) {
		device_remove_file(&device->dev->dev,
				   &dev_attr_power_state_ctl);
		return err;
	}

	device->link_state.kn =
		sysfs_get_dirent(device->dev->dev.kobj.sd, "link_state");

	return 0;
}

void ar_pci_remove_driver_sysfs_nodes(ar_pci_driver_t *device)
{
	if (device->link_state.kn) {
		sysfs_put(device->link_state.kn);
		device->link_state.kn = NULL;
	}
	device_remove_file(&device->dev->dev, &dev_attr_link_state);
	device_remove_file(&device->dev->dev, &dev_attr_power_state_ctl);
}
