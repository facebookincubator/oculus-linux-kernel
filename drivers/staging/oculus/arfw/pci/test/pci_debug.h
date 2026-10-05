// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file pci_debug.h
 *
 * @brief AR PCI debug header
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_PCI_DEBUG_H
#define AR_PCI_DEBUG_H

#include <linux/pci.h>

typedef struct {
	size_t offset;
	size_t bar_size;
	void *bar_base;
} pci_debug_data_t;

/**
 * Initialize debug features
 *
 * @param[in] dev the pci device
 *
 * @retval 0 on success
 */
int ar_pci_debug_init(struct pci_dev *dev);

/**
 * Remove debug features
 *
 * @param[in] dev the pci device
 */
void ar_pci_debug_remove(struct pci_dev *dev);

/**
 * Dump client ring bar information
 */
void ar_pci_debug_bar_dump_client_rings(struct pci_dev *dev);

#endif // !AR_PCI_DEBUG_H
