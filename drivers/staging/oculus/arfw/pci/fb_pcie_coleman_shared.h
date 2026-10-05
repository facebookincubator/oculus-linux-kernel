/* SPDX-License-Identifier: GPL+ */
/** *************************************************************************************************
 *
 * @file fb_pcie_coleman_shared.h
 *
 * @brief File shared between host driver and coleman FW..
 *
 * @details
 *
 * NOTE: This file in AOSP 
 *  (https://www.internalfb.com/code/aosp/kernel/drivers/staging/oculus/internal/arfirmware/pci/fb_pcie_coleman_shared.h) 
 * must be kept in sync with the fbsource-side source of truth: 
 *  fb_pcie_coleman_shared.h file (https://fburl.com/code/do5m9eux).
 *
 *
 **************************************************************************************************/

/* clang-format off */  /* This file is copied from oculus-12.0 repo, do not reformat */

#pragma once

/***************************************************************************************************
 * Includes
 **************************************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
 * Macro Definitions
 **************************************************************************************************/
// BEGIN HAPS/Production values
// Meta PCIe Vendor ID - https://pcisig.com/membership/member-companies?combine=facebook
#define META_PCI_SIG_VENDOR_ID 0x1D9B
// 64 30 spells "d0"
#define COLEMAN_PCIE_DEVICE_ID_F0 0x6430

// 64 31 spells "d1"
#define COLEMAN_PCIE_DEVICE_ID_F1 0x6431

// 64 32 spells "d2
#define COLEMAN_PCIE_DEVICE_ID_F2 0x6432

// 64 33 spells "d3"
#define COLEMAN_PCIE_DEVICE_ID_F3 0x6433

// 64 34 spells "d4"
#define COLEMAN_PCIE_DEVICE_ID_F4 0x6434

// 64 35 spells "d5"
#define COLEMAN_PCIE_DEVICE_ID_F5 0x6435

// 64 36 spells "d6"
#define COLEMAN_PCIE_DEVICE_ID_F6 0x6436

// 64 37 spells "d7"
#define COLEMAN_PCIE_DEVICE_ID_F7 0x6437
// END HAPS/Production values

// BEGIN VP values
// Taken from: https://fburl.com/code/88bxk84v
#define FERM_COLEMAN_PCI_FAKE_VENDOR_ID 0x7461
#define FERM_COLEMAN_PCIE_DEVICE_ID 0x6d65
#define FERM_COLEMAN_PCIE_BASE_CLASS_ID  0xdd
#define FERM_COLEMAN_PCIE_SUB_CLASS_ID  0xcc
// END VP values

/***************************************************************************************************
 * Type Definitions
 **************************************************************************************************/

/***************************************************************************************************
 * Constant Definitions
 **************************************************************************************************/

/***************************************************************************************************
 * Exported Variable Declarations
 **************************************************************************************************/

/***************************************************************************************************
 * Exported Function Prototypes
 **************************************************************************************************/

#ifdef __cplusplus
} // end extern "C"
#endif

/* clang-format on */ /* This file is copied from oculus-12.0 repo, re-enable format */
