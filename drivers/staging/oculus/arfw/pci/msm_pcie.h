/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file msm_pcie.h
 *
 * @brief Declares special operations for Qualcom MSM PCIe
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef MSM_PCIE_H
#define MSM_PCIE_H

#include <linux/msm_pcie.h>

#include <arfw_config.h>

#include "ar_pci_int.h"

typedef void (*msm_pcie_notify_callback_t)(ar_pci_driver_t *,
					   enum msm_pcie_event);

int msm_pcie_pm_link_down(ar_pci_driver_t *driver);
int msm_pcie_pm_link_resume(ar_pci_driver_t *driver);
int msm_pcie_pm_link_suspend(ar_pci_driver_t *driver);
int msm_pcie_subscribe_events(ar_pci_driver_t *driver,
			      msm_pcie_notify_callback_t cb, uint32_t events);
int msm_pcie_unsubscribe_events(ar_pci_driver_t *driver);
int msm_pcie_set_aspm_state(ar_pci_driver_t *driver, bool enable);
int msm_pcie_get_aspm_state(ar_pci_driver_t *driver, bool *state);

#endif // !MSM_PCIE_H
