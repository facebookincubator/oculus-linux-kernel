// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_shim.c
 *
 * @brief Shim module to decouple driver module dependencies.
 *
 * @details
 *   Different client implementations can register themselves in the shim.
 *   Client consumers like PCIe driver can look up the functions and use them.
 *   This allows all these modules to not be statically coupled with each other.
 *   Current implementation selects the last registered shim for invocation.
 *
 *   Right now shim supports two subtypes:
 *     - Character device API (0 or 1 clients)
 *     - PCIe device multifunction API (ARFW_SHIM_PCI_FUNC_CNT clients)
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/pci.h>

#include <ar_common.h>
#include <arfw_log.h>
#include <arfw_shim.h>

struct arfw_shim_cdev_entry {
	char id[ARFW_SHIM_ID_MAX_LEN];
	const struct arfw_shim_cdev *shim;
};

struct arfw_shim_pci_entry {
	const struct arfw_shim_pci *shim;
};

static struct arfw_shim_cdev_entry arfw_shim_cdev_entry;
static DEFINE_MUTEX(arfw_shim_cdev_lock);

static struct arfw_shim_pci_entry arfw_shim_pci_entries[ARFW_SHIM_PCI_FUNC_CNT];
static DEFINE_MUTEX(arfw_shim_pci_lock);

static int
arfw_shim_cdev_register_impl(struct device *hw_dev, const char *device_id,
			     const struct arfw_driver_ops *driver_ops,
			     const struct arfw_client_ops **client_ops,
			     void *base_context)
{
	int ret = -EOPNOTSUPP;

	mutex_lock(&arfw_shim_cdev_lock);
	if (arfw_shim_cdev_entry.shim) {
		AR_ASSERT(arfw_shim_cdev_entry.shim->cdev_register);
		ret = arfw_shim_cdev_entry.shim->cdev_register(
			hw_dev, device_id, driver_ops, client_ops,
			base_context);
	}
	mutex_unlock(&arfw_shim_cdev_lock);

	return ret;
}

arfw_cdev_register_t arfw_shim_cdev_register = &arfw_shim_cdev_register_impl;
EXPORT_SYMBOL(arfw_shim_cdev_register);

static int arfw_shim_cdev_unregister_impl(const char *device_id,
					  struct device **hw_device,
					  bool module_remove)
{
	int ret = -EOPNOTSUPP;

	mutex_lock(&arfw_shim_cdev_lock);
	if (arfw_shim_cdev_entry.shim) {
		AR_ASSERT(arfw_shim_cdev_entry.shim->cdev_unregister);
		ret = arfw_shim_cdev_entry.shim->cdev_unregister(
			device_id, hw_device, module_remove);
	}
	mutex_unlock(&arfw_shim_cdev_lock);

	return ret;
}

arfw_cdev_unregister_t arfw_shim_cdev_unregister =
	&arfw_shim_cdev_unregister_impl;
EXPORT_SYMBOL(arfw_shim_cdev_unregister);

int arfw_shim_cdev_add(const char *id, const struct arfw_shim_cdev *shim)
{
	int len, ret = 0;

	AR_ASSERT(id);
	AR_ASSERT(shim);
	AR_ASSERT(shim->cdev_register);
	AR_ASSERT(shim->cdev_unregister);

	len = strnlen(id, ARFW_SHIM_ID_MAX_LEN + 1);
	if (len > ARFW_SHIM_ID_MAX_LEN)
		return -E2BIG;

	mutex_lock(&arfw_shim_cdev_lock);
	if (arfw_shim_cdev_entry.shim) {
		AR_LOG_ARFW_ERR(
			AR_LOG_SHIM,
			"Cannot add a cdev shim for '%.*s', already exists",
			ARFW_SHIM_ID_MAX_LEN, id);
		ret = -EEXIST;
	} else {
		strncpy(arfw_shim_cdev_entry.id, id, ARFW_SHIM_ID_MAX_LEN);
		arfw_shim_cdev_entry.shim = shim;
		AR_LOG_ARFW_INFO(AR_LOG_SHIM, "Added a cdev shim for '%.*s'",
				 ARFW_SHIM_ID_MAX_LEN, id);
	}
	mutex_unlock(&arfw_shim_cdev_lock);

	return ret;
}
EXPORT_SYMBOL(arfw_shim_cdev_add);

int arfw_shim_cdev_remove(const char *id)
{
	int ret = 0;

	AR_ASSERT(id);

	mutex_lock(&arfw_shim_cdev_lock);
	if (!arfw_shim_cdev_entry.shim) {
		AR_LOG_ARFW_ERR(
			AR_LOG_SHIM,
			"Cannot remove a cdev shim for '%.*s', does not exist",
			ARFW_SHIM_ID_MAX_LEN, id);
		ret = -ENOENT;
	} else if (strncmp(arfw_shim_cdev_entry.id, id, ARFW_SHIM_ID_MAX_LEN) !=
		   0) {
		AR_LOG_ARFW_ERR(
			AR_LOG_SHIM,
			"Cannot remove a cdev shim for '%.*s', registered with '%.*s'",
			ARFW_SHIM_ID_MAX_LEN, id, ARFW_SHIM_ID_MAX_LEN,
			arfw_shim_cdev_entry.id);
		ret = -EINVAL;
	} else {
		arfw_shim_cdev_entry.shim = NULL;
	}
	mutex_unlock(&arfw_shim_cdev_lock);

	return ret;
}
EXPORT_SYMBOL(arfw_shim_cdev_remove);

static int arfw_shim_pci_probe_impl(struct pci_dev *dev,
				    const struct pci_device_id *id)
{
	int ret = -EOPNOTSUPP;
	const struct arfw_shim_pci *shim;

	AR_ASSERT(dev);

	mutex_lock(&arfw_shim_pci_lock);
	shim = arfw_shim_pci_entries[PCI_FUNC(dev->devfn)].shim;
	if (shim && shim->pci_probe)
		ret = shim->pci_probe(dev, id);
	mutex_unlock(&arfw_shim_pci_lock);

	return ret;
}
arfw_pci_probe_t arfw_shim_pci_probe = &arfw_shim_pci_probe_impl;
EXPORT_SYMBOL(arfw_shim_pci_probe);

static int arfw_shim_pci_remove_impl(struct pci_dev *dev)
{
	int ret = -EOPNOTSUPP;
	const struct arfw_shim_pci *shim;

	AR_ASSERT(dev);

	mutex_lock(&arfw_shim_pci_lock);
	shim = arfw_shim_pci_entries[PCI_FUNC(dev->devfn)].shim;
	if (shim && shim->pci_remove)
		ret = shim->pci_remove(dev);
	mutex_unlock(&arfw_shim_pci_lock);

	return ret;
}
arfw_pci_remove_t arfw_shim_pci_remove = &arfw_shim_pci_remove_impl;
EXPORT_SYMBOL(arfw_shim_pci_remove);

static int arfw_shim_pci_linkup_impl(struct pci_dev *dev)
{
	int ret = -EOPNOTSUPP;
	const struct arfw_shim_pci *shim;

	AR_ASSERT(dev);

	mutex_lock(&arfw_shim_pci_lock);
	shim = arfw_shim_pci_entries[PCI_FUNC(dev->devfn)].shim;
	if (shim && shim->pci_linkup)
		ret = shim->pci_linkup(dev);
	mutex_unlock(&arfw_shim_pci_lock);

	return ret;
}
arfw_pci_linkup_t arfw_shim_pci_linkup = &arfw_shim_pci_linkup_impl;
EXPORT_SYMBOL(arfw_shim_pci_linkup);

static int arfw_shim_pci_linkdown_impl(struct pci_dev *dev)
{
	int ret = -EOPNOTSUPP;
	const struct arfw_shim_pci *shim;

	AR_ASSERT(dev);

	mutex_lock(&arfw_shim_pci_lock);
	shim = arfw_shim_pci_entries[PCI_FUNC(dev->devfn)].shim;
	if (shim && shim->pci_linkdown)
		ret = shim->pci_linkdown(dev);
	mutex_unlock(&arfw_shim_pci_lock);

	return ret;
}
arfw_pci_linkdown_t arfw_shim_pci_linkdown = &arfw_shim_pci_linkdown_impl;
EXPORT_SYMBOL(arfw_shim_pci_linkdown);

int arfw_shim_pci_add_func(const unsigned int devfn,
			   const struct arfw_shim_pci *shim)
{
	int ret = 0;

	AR_ASSERT(devfn < ARFW_SHIM_PCI_FUNC_CNT);
	AR_ASSERT(shim);

	mutex_lock(&arfw_shim_pci_lock);
	if (arfw_shim_pci_entries[devfn].shim) {
		AR_LOG_ARFW_ERR(
			AR_LOG_SHIM,
			"Cannot add a pci shim for devfn 0x%x, already exists",
			devfn);
		ret = -EEXIST;
	} else {
		arfw_shim_pci_entries[devfn].shim = shim;
		AR_LOG_ARFW_INFO(AR_LOG_SHIM, "Added a pci shim for devfn 0x%x",
				 devfn);
	}
	mutex_unlock(&arfw_shim_pci_lock);

	return ret;
}
EXPORT_SYMBOL(arfw_shim_pci_add_func);

int arfw_shim_pci_remove_func(const unsigned int devfn)
{
	int ret = 0;

	AR_ASSERT(devfn < ARFW_SHIM_PCI_FUNC_CNT);

	mutex_lock(&arfw_shim_pci_lock);
	if (!arfw_shim_pci_entries[devfn].shim) {
		AR_LOG_ARFW_ERR(
			AR_LOG_SHIM,
			"Cannot remove a pci shim for devfn 0x%x, does not exist",
			devfn);
		ret = -ENOENT;
	} else {
		arfw_shim_pci_entries[devfn].shim = NULL;
	}
	mutex_unlock(&arfw_shim_pci_lock);

	return ret;
}
EXPORT_SYMBOL(arfw_shim_pci_remove_func);

static int __init arfw_shim_init(void)
{
	AR_LOG_ARFW_INFO(AR_LOG_SHIM, "Init arfw shim module");
	return 0;
}

static void __exit arfw_shim_exit(void)
{
	// Since the memory is allocated statically, we don't need to free it.
	// It will be freed when the module is unloaded.
	AR_LOG_ARFW_INFO(AR_LOG_SHIM, "Exit arfw shim module");
}

#if IS_MODULE(CONFIG_ARFIRMWARE_COMMON)
module_init(arfw_shim_init);
#else // !IS_MODULE(CONFIG_ARFIRMWARE_COMMON)
arch_initcall(arfw_shim_init);
#endif // IS_MODULE(CONFIG_ARFIRMWARE_COMMON)
module_exit(arfw_shim_exit);

MODULE_LICENSE("GPL");
