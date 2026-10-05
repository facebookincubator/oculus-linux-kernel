/* SPDX-License-Identifier: GPL+ */
/** *************************************************************************************************
 *
 * @file coleman_pci_test.h
 *
 * @brief File shared between pcievalidator and arfw_coleman_pcie_test driver.
 *
 * @details This file defines the interface used to implement communication between the
 * pcievalidator and the coleman firmware side pcie test driver over the host side PCIe driver.
 *
 * NOTE: This file in fbsource (https://fburl.com/code/px85evmt) must be kept in sync with the
 * AOSP-side source of truth: coleman_pci_test.h file (https://fburl.com/code/yxgi3xuw).
 *
 *
 **************************************************************************************************/

/* clang-format off */  /* This file is copied from oculus-12.0 repo, do not reformat */

#pragma once

/***************************************************************************************************
 * Includes
 **************************************************************************************************/
#include "asm-generic/ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
 * Macro Definitions
 **************************************************************************************************/
#define PCIE_VALIDATOR_IOC_MAGIC \
	(0xFA) //!< magic number for pcivalidator ioctl requests

#define MAX_MSI_SUPPORTED (8) //!< Maximum number of MSIs supported

/**
 * @brief Request to set the offsets into BAR for read/write/ioctl operations in the driver.
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_SET_RW_OFFSETS              \
	_IOW(PCIE_VALIDATOR_IOC_MAGIC, 1, \
	     struct ioctl_set_rw_offsets)

/**
 * @brief Request to get the DMA-able buffer bus address and size allocated by the device driver.
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_GET_DMA_BUFFER              \
	_IOR(PCIE_VALIDATOR_IOC_MAGIC, 2, \
	     struct ioctl_get_pinned_buffer)

#define IOCTL_GET_DMA_BUFFER_INFO \
	_IOR(PCIE_VALIDATOR_IOC_MAGIC, 3, \
	     struct ioctl_get_pinned_buffer_info)

/**
 * @brief Request to set the current pcievalidator control request in hardware
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_SEND_VALIDATOR_CONTROL_REQUEST \
	_IOW(PCIE_VALIDATOR_IOC_MAGIC, 3,    \
	     struct ioctl_validator_control_request)
#define _IOCTL_VALIDATOR_CONTROL_REQUEST_MAX_SIZE \
	(256) //!< Maximum size of control request

/**
 * @brief Request to get the current pcievalidator control response from hardware
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_GET_VALIDATOR_CONTROL_RESPONSE \
	_IOR(PCIE_VALIDATOR_IOC_MAGIC, 4,    \
	     struct ioctl_validator_control_response)
#define _IOCTL_VALIDATOR_CONTROL_RESPONSE_MAX_SIZE \
	(256) //!< Maximum size of control response

/**
 * @brief Request to get the aperture that the firmware can access on the host side.
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_GET_APERTURE_BUFFER \
_IOR(PCIE_VALIDATOR_IOC_MAGIC, 5, \
	struct ioctl_get_pinned_buffer)

#define IOCTL_GET_APERTURE_BUFFER_INFO \
  _IOR(PCIE_VALIDATOR_IOC_MAGIC, 8, \
	struct ioctl_get_pinned_buffer_info)

/**
 * @brief Request to set the current pcievalidator doorbell request in hardware
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_SEND_VALIDATOR_DOORBELL_REQUEST \
	_IOW(PCIE_VALIDATOR_IOC_MAGIC, 5,    \
	     struct ioctl_validator_doorbell_request)
#define _IOCTL_VALIDATOR_DOORBELL_REQUEST_MAX_SIZE \
	(256) //!< Maximum size of control request

/**
 * @brief Request the number of Message Signaled Interrupts (MSIs) supported by the Host
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_GET_ALLOCATED_IRQS \
	_IOR(PCIE_VALIDATOR_IOC_MAGIC, 6,    \
	     struct ioctl_get_msi_allocation_info)

/**
 * @brief Request the number of Message Signaled Interrupts (MSIs) received by the Host
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_GET_RECEIVED_IRQS \
	_IOR(PCIE_VALIDATOR_IOC_MAGIC, 7,    \
	     struct ioctl_get_msi_received_info)

/**
 * @brief Select the next buffer to mmap()
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_SELECT_MMAP_BUFFER \
  _IOW(PCIE_VALIDATOR_IOC_MAGIC, 6, \
			struct ioctl_select_mmap_buffer)


/**
 * @brief Check if the device has recieved the test done interrupt
 *
 * @return 0 on success, negative value on failure
 */
#define IOCTL_CHECK_TEST_DONE_FLAG \
	_IOR(PCIE_VALIDATOR_IOC_MAGIC, 8,    \
	     struct ioctl_check_test_done_flag)

/***************************************************************************************************
 * Type Definitions
 **************************************************************************************************/

/// Userland to kernel structure for setting offsets into the pcievalidator interface.
struct ioctl_set_rw_offsets {
	uint32_t bar_number; //!< BAR number to use for pcievalidator interface
	uint32_t read_offset; //!< Offset into BAR for read operation (test interface)
	uint32_t write_offset; //!< Offset into BAR for write operation (test interface)
	struct {
		uint32_t request; //!< Offset into BAR for request structure
		uint32_t response; //!< Offset into BAR for response structure
	} control; //!< Offsets into BAR for control interface
};

/// Userland to kernel structure for getting bus address and size for DMA-able data.
struct ioctl_get_pinned_buffer {
	uint64_t bus_address; //!< Base PCIe BUS address of DMA-able buffer for use by device
	size_t size; //!< Size of DMA-able buffer
};

struct ioctl_get_pinned_buffer_info {
	uint32_t buffer_index; //!< Index to use when selecting the buffer to mmap()
	uint64_t bus_address; //!< Base PCIe BUS address of DMA-able buffer for use by device
	size_t size; //!< Size of DMA-able buffer
};

/// Userland to kernel structure for sending a control request to pcievalidator.
struct ioctl_validator_control_request {
	uint32_t size; //!< Amount of data to write from data array to device
	uint8_t data[_IOCTL_VALIDATOR_CONTROL_REQUEST_MAX_SIZE]; //!< Data to write to device's
		//!< control request interface
};

/// Userland to kernel structure for getting a control response from pcievalidator.
struct ioctl_validator_control_response {
	uint32_t size; //!< Amount of data read from device's control response interface
	uint8_t data[_IOCTL_VALIDATOR_CONTROL_RESPONSE_MAX_SIZE]; //!< Data read from device's
		//!< control response interface
};

/// Userland to kernel structure for sending a doorbell request.
struct ioctl_validator_doorbell_request {
	uint32_t doorbell_reg_offset; //!< Offset of doorbell register to write to
};

/// Userland to kernel structure for getting MSI info from Host.
struct ioctl_get_msi_allocation_info {
	uint16_t allocated_irqs[MAX_MSI_SUPPORTED];  //!< IRQ numbers for allocated MSIs
};

/// Userland to kernel structure for getting MSI info from Host.
struct ioctl_get_msi_received_info {
	uint16_t received_irq_numbers[MAX_MSI_SUPPORTED];	//!< IRQ numbers of MSIs received
	uint16_t received_irq_count[MAX_MSI_SUPPORTED];  //!< Number of IRQs received per MSI
};

/// Userland to kernel structure for selecting the next buffer to mmap.
struct ioctl_select_mmap_buffer {
  uint32_t buffer_index; //!< Buffer to select for next mmap() call
};

/// Userland to kernel structure for checking the pcievalidator test flag.
struct ioctl_check_test_done_flag {
  uint32_t done; //!< Flag to indicate the test status (0: not done, 1: done)
};

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
