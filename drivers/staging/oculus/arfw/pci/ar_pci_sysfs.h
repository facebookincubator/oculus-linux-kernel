/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file ar_pci_sysfs.h
 *
 * @brief Internal header for sysfs helpers.
 *
 * @details APIs for initializing and managing sysfs files for the ar pcie driver. 
 *
 *******************************************************************************/

#ifndef AR_PCI_SYSFS_H
#define AR_PCI_SYSFS_H

#include "ar_pci_int.h"

int ar_pci_add_driver_sysfs_nodes(ar_pci_driver_t *driver);
void ar_pci_remove_driver_sysfs_nodes(ar_pci_driver_t *driver);

/**
 * @brief Publish a link-state transition on the link_state sysfs node.
 *
 * @details Wakes pollers so a client can block until the link is usable
 * instead of retrying open() on the char device. Safe from hard IRQ.
 * No-op if the state is unchanged.
 */
void ar_pci_set_link_state(ar_pci_driver_t *driver, bool up);

#endif // AR_PCI_SYSFS_H
