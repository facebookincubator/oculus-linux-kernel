/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file arfw_config.h
 *
 * @brief replacement for kernel configuration file (change these ad-hoc).
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_CONFIG_H
#define ARFW_CONFIG_H

/**
 * Enable debug functionality.
 * Uncomment the following lines to enable.
 */
#define CONFIG_ARFIRMWARE_PCI_DEBUG 1

/**
 * Enable special PCIe root complex functionality wrappers.
 * On the Qualcom MSM system to suspend/resume the bus link when needed.
 * This allows firmware to reset the link properly and re-initialize itself
 * on PERST interrupt so that de-probing works.
 * This works because we only compile one module here: https://fburl.com/code/sl03d5lf.
 * This will not work if we compile multiple like AVO/COL for DEV0/NFF0.
 * Uncomment the following lines to enable.
 */
#if defined(CONFIG_ARFIRMWARE_ACRO_PCI) || \
	defined(CONFIG_ARFIRMWARE_ACRO_PCI_MODULE)
#define CONFIG_ARFIRMWARE_MSM_PCIE 1
#endif // CONFIG_ARFIRMWARE_ACRO_PCI || CONFIG_ARFIRMWARE_ACRO_PCI_MODULE || CONFIG_ARFIRMWARE_COLEMAN_PCI || CONFIG_ARFIRMWARE_COLEMAN_PCI_MODULE

/**
 * Some products msm pcie driver implmentations require force suspend to work properly.
 */
#if defined(CONFIG_ARFIRMWARE_COLEMAN_PCI) || \
	defined(CONFIG_ARFIRMWARE_COLEMAN_PCI_MODULE)
#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
#define CONFIG_ARFIRMWARE_MSM_PCIE_FORCE_SUSPEND
#endif // CONFIG_ARFIRMWARE_MSM_PCIE
#endif // CONFIG_ARFIRMWARE_COLEMAN_PCI || CONFIG_ARFIRMWARE_COLEMAN_PCI_MODULE

/**
 * Enable power state management switches.
 * Will allow to suspend/resume the probed devices without the power management
 * subsystem. The functionality will directly operate on the kernel module.
 * Can be used to reset the protocol and re-init link without device reboot.
 * Uncomment the following lines to enable.
 */
// #define CONFIG_ARFIRMWARE_PCI_PM_OPS 1

/**
 * Enable PCI GPIO doorbell mechanism.
 * This will replace interrupt bar region doorbell mechanism with a dedicate GPIO.
 * This is useful for CPs that do not support an interrupt bar region.
 * This works because we only compile one module here: https://fburl.com/code/sl03d5lf.
 * This will not work if we compile multiple like AVO/COL for DEV0/NFF0.
 * Uncomment the following lines to enable.
 */
#if defined(CONFIG_ARFIRMWARE_ACRO_PCI) || \
	defined(CONFIG_ARFIRMWARE_ACRO_PCI_MODULE)
#define CONFIG_ARFIRMWARE_PCI_GPIO_DOORBELL 1
#endif // CONFIG_ARFIRMWARE_ACRO_PCI || CONFIG_ARFIRMWARE_ACRO_PCI_MODULE

#endif // ARFW_CONFIG_H
