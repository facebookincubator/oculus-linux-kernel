/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file arfw_shim.h
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
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_SHIM_H
#define ARFW_SHIM_H

/**
 * Character device shim.
 * Each shim uses a string to associate with as a tag.
 * The string is used for shim removal later and the length of that
 * identity string is limited by this constant.
 */
#define ARFW_SHIM_ID_MAX_LEN 48

/**
 * Character device shim.
 * Forward declarations of some device utilities this shim
 * module will be dealing with.
 */
struct device;
struct arfw_driver_ops;
struct arfw_client_ops;

/**
 * Character device shim.
 * API function types the shim will be calling.
 */
typedef int (*arfw_cdev_register_t)(struct device *, const char *,
				    const struct arfw_driver_ops *,
				    const struct arfw_client_ops **, void *);
typedef int (*arfw_cdev_unregister_t)(const char *, struct device **, bool);

/**
 * Character device shim.
 * Dynamically selected shim implementation invocations.
 * PCIe driver should be calling these to dynamically select clients.
 * Returns -EOPNOTSUPP when no shim can be selected.
 */
extern arfw_cdev_register_t arfw_shim_cdev_register;
extern arfw_cdev_unregister_t arfw_shim_cdev_unregister;

/**
 * Character device shim.
 * Holder for dynamic device functions to register in
 * the shim module. Function pointers SHOULD NOT be NULL.
 */
struct arfw_shim_cdev {
	arfw_cdev_register_t cdev_register;
	arfw_cdev_unregister_t cdev_unregister;
};

/**
 * Character device shim.
 * Client implementations call this to register themselves in the shim.
 * This should be done on client module init.
 *
 * @param[in] id    String to associate this implementation with
 * @param[in] shim  Structure with all dynamic function implementations
 *
 * @retval 0       OK
 * @retval -E2BIG  When id max length is exceeded, see ARFW_SHIM_ID_MAX_LEN
 * @retval -EEXIST When id is already associated with some implementation
 */
int arfw_shim_cdev_add(const char *id, const struct arfw_shim_cdev *shim);

/**
 * Character device shim.
 * Client implementations call this to unregister themselves from the shim.
 * This should be done on client module exit.
 *
 * @param[in] id String to look up an implementation with
 *
 * @retval 0       OK
 * @retval -ENOENT When shim is not present at all
 * @retval -EINVAL When shim id does not match the provided id
 */
int arfw_shim_cdev_remove(const char *id);

/**
 * PCIe device multifunction shim.
 * The maximum number of functions this shim can support.
 */
#define ARFW_SHIM_PCI_FUNC_CNT 8

/**
 * PCIe device multifunction shim.
 * Forward declarations of some device utilities this shim
 * module will be dealing with.
 */
struct pci_dev;
struct pci_device_id;

/**
 * PCIe device multifunction shim.
 * API function types the shim will be calling.
 */
typedef int (*arfw_pci_probe_t)(struct pci_dev *, const struct pci_device_id *);
typedef int (*arfw_pci_remove_t)(struct pci_dev *);
typedef int (*arfw_pci_linkup_t)(struct pci_dev *);
typedef int (*arfw_pci_linkdown_t)(struct pci_dev *);

/**
 * PCIe device multifunction shim.
 * Dynamically selected shim implementation invocations.
 * PCIe driver should be calling these to dynamically select clients.
 * Returns -EOPNOTSUPP when no shim can be selected.
 */
extern arfw_pci_probe_t arfw_shim_pci_probe;
extern arfw_pci_remove_t arfw_shim_pci_remove;
extern arfw_pci_linkup_t arfw_shim_pci_linkup;
extern arfw_pci_linkdown_t arfw_shim_pci_linkdown;

/**
 * PCIe device multifunction shim.
 * Holder for dynamic device functions to register in
 * the shim module. Function pointers SHOULD NOT be NULL.
 */
struct arfw_shim_pci {
	arfw_pci_probe_t pci_probe;
	arfw_pci_remove_t pci_remove;
	arfw_pci_linkup_t pci_linkup;
	arfw_pci_linkdown_t pci_linkdown;
};

/**
 * PCIe device multifunction shim.
 * Client implementations call this to register themselves in the shim.
 * This should be done on client module init.
 * Multiple shims can be registered at the same time for different functions.
 *
 * @param[in] devfn PCIe device function number
 * @param[in] shim  Structure with all dynamic function implementations
 *
 * @retval 0       OK
 * @retval -EEXIST When devfn is already associated with some implementation
 */
int arfw_shim_pci_add_func(unsigned int devfn,
			   const struct arfw_shim_pci *shim);

/**
 * PCIe device multifunction shim.
 * Client implementations call this to unregister themselves from the shim.
 * This should be done on client module exit.
 *
 * @param[in] devfn PCIe device function number
 *
 * @retval 0       OK
 * @retval -ENOENT When shim is not present with provided devfn
 */
int arfw_shim_pci_remove_func(unsigned int devfn);

#endif // !ARFW_SHIM_H
