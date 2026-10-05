// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file debug_int.h
 *
 * @brief AR PCI internal debug header
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_PCI_DEBUG_INT_H
#define AR_PCI_DEBUG_INT_H

/**
 * Add or Remove sysfs debug nodes for pci control ring
 */
int ar_pci_control_ring_sysfs_add(struct pci_dev *dev);
void ar_pci_control_ring_sysfs_remove(struct pci_dev *dev);

/**
 * Add or Remove sysfs debug nodes for pci data ring
 */
int ar_pci_data_ring_sysfs_add(struct pci_dev *dev);
void ar_pci_data_ring_sysfs_remove(struct pci_dev *dev);

#endif // !AR_PCI_DEBUG_INT_H
