/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file arfw_pci.h
 *
 * @brief PCIe device driver constants.
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_PCI_H
#define ARFW_PCI_H

#include <arfw_config.h>

/*
 * Undefine PCI_VENDOR_ID_META at the start to override any kernel definition
 * from pci_ids.h. Each PCI driver section below will define the appropriate
 * vendor ID for its device.
 */
#undef PCI_VENDOR_ID_META

#define PCI_FUNCTION_MASK 0x7

#if defined(CONFIG_ARFIRMWARE_COLEMAN_PCI) || \
	defined(CONFIG_ARFIRMWARE_COLEMAN_PCI_MODULE)
#define PCI_DRIVER_COLEMAN AR_DRIVER_NAME "_col"
#define PCI_VENDOR_ID_META 0x7461
#define PCI_VENDOR_ID_META_EX 0x1D9B
#define PCI_DEVICE_ID_META 0x6D65
#define PCI_DEVICE_ID_F0 0x6430
#define PCI_DEVICE_ID_F1 0x6431
#define PCI_DEVICE_ID_F2 0x6432
#define PCI_DEVICE_ID_F3 0x6433
#define PCI_DEVICE_ID_F4 0x6434
#define PCI_DEVICE_ID_F5 0x6435
#define PCI_DEVICE_ID_F6 0x6436
#define PCI_DEVICE_ID_F7 0x6437
#endif // CONFIG_ARFIRMWARE_COLEMAN_PCI || CONFIG_ARFIRMWARE_COLEMAN_PCI_MODULE

#if defined(CONFIG_ARFIRMWARE_JAZZ_PCI) || \
	defined(CONFIG_ARFIRMWARE_JAZZ_PCI_MODULE)
#define PCI_DRIVER_JAZZ AR_DRIVER_NAME "_jazz"
#define PCI_VENDOR_ID_META 0x7461
#define PCI_DEVICE_ID_META 0x6D65
#endif // CONFIG_ARFIRMWARE_JAZZ_PCI || CONFIG_ARFIRMWARE_JAZZ_PCI_MODULE

#if defined(CONFIG_ARFIRMWARE_ACRO_PCI) || \
	defined(CONFIG_ARFIRMWARE_ACRO_PCI_MODULE)
#define PCI_DRIVER_ACRO AR_DRIVER_NAME "_acro"
#define PCI_VENDOR_ID_META 0x1d9b
#define PCI_DEVICE_ID_META 0xacd0
#endif // CONFIG_ARFIRMWARE_ACRO_PCI || CONFIG_ARFIRMWARE_ACRO_PCI_MODULE

#if defined(CONFIG_ARFIRMWARE_AVO_PCI) || \
	defined(CONFIG_ARFIRMWARE_AVO_PCI_MODULE)
#define PCI_DRIVER_AVO AR_DRIVER_NAME "_avo"
#define PCI_DEV_ID_A1_B0 0xabcd
#endif // CONFIG_ARFIRMWARE_AVO_PCI || CONFIG_ARFIRMWARE_AVO_PCI_MODULE

#endif // !ARFW_PCI_H
