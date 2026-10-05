/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file ar_pci_bar.h
 *
 * @brief PCI BAR interface for AR accelorator
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_PCI_BAR_H
#define AR_PCI_BAR_H

#include <linux/pci.h>
#include <linux/hrtimer.h>

#include "ar_pci_int.h"
#include "arfw_mem_util.h"

// Util to find the address of a status block, current version
#define AR_BAR_STATUS_BLOCK_ADDR(driver, field)                             \
	(driver->deprecated_bar_layout->bar_memory_area.status_block_addr + \
	 offsetof(pcie_bar_status_block_area_t, field))

// Util to find the address of a status block, v2 and previous
#define AR_BAR_STATUS_BLOCK_V2_ADDR(driver, field)                          \
	(driver->deprecated_bar_layout->bar_memory_area.status_block_addr + \
	 offsetof(pcie_bar_status_block_area_v2_t, field))

// Location and value of the doorbell interrupt
#define MMIO_INTERRUPT_OFFSET 0x2000
#define MMIO_INTERRUPT_VALUE 1

/**
 * Map the bar spaces expected by the driver
 *
 * @param[in] dev the pci device object, with the driver context already
 * set
 *
 * @retval 0 on success
 */
int ar_pci_bar_map(struct pci_dev *dev);

/**
 * Unmap the bar spaces mapped by the driver
 *
 * @param[in] dev the pci device object, with the driver context already
 * set
 *
 * @retval 0 on success
 */
int ar_pci_bar_unmap(struct pci_dev *dev);

/**
 * Validate the handshake is successful
 *
 * @param[in] dev the pci device object, with the driver context already
 * set. This also caches the startup contents of the bar memory area
 *
 * @retval 0 The handshake values are correct
 */
int ar_pci_bar_handshake(struct pci_dev *dev);

/**
 * Release all the resources allocated during handshake
 *
 * @param[in] dev the pci device object, with the driver context already
 * set.
 *
 * @retval None
 */
void ar_pci_bar_release(struct pci_dev *dev);

/**
 * Initialize device driver with the doorbell offset.
 *
 * @param[in] dev the pci device object, with the driver context already
 * set.
 *
 * @retval None
 */
void ar_pci_bar_init_doorbell(struct pci_dev *dev);

/**
 * Ring the MMIO doorbell on Avo
 *
 * @param[in] dev the pci device object, with the driver context already
 * set.
 *
 * @retval 0 on success
 */
int ar_pci_bar_ring_doorbell(struct pci_dev *dev);

/**
 * Enable the interrupt
 *
 * @param[in] dev the pci device object, with the driver context already
 * set.
 *
 * @retval 0 on success
 */
int ar_pci_bar_enable_interrupt(struct pci_dev *dev);

/**
 * Register a boot interrupt handler.
 * @param[in] dev the pci device object, with the driver context already
 * set.
 * @param[in] handshake_irq_handler the handler to register
 */
void ar_pci_bar_set_handshake_irq_handler(
	struct pci_dev *dev,
	ar_pci_handshake_irq_handler_t handshake_irq_handler);

/**
 * Disable the interrupt
 *
 * @param[in] dev the pci device object, with the driver context already
 * set.
 */
void ar_pci_bar_disable_interrupt(struct pci_dev *dev);

/**
 * Submit an ap source control packet to the control ring
 *
 * @param[in] dev the pci device object, with the driver context already
 * set.
 * @param[in] packet The payload to deliver
 * @param[in] ring_doorbell True if AVO should be notified
 *
 * @retval 0 on success
 */
int ar_pci_bar_submit_control_packet(struct pci_dev *dev,
				     pcie_ctrl_req_t *packet,
				     bool ring_doorbell);

/**
 * Submit an ap source buffer to the control ring
 *
 * @param[in] dev the pci device object, with the driver context already
 * set.
 * @param[in] packet The payload to deliver
 * @param[in] ring_doorbell True if AVO should be notified
 *
 * @retval 0 on success
 */
int ar_pci_bar_submit_ap_src_buffer(struct pci_dev *dev,
				    pcie_ap_src_buf_item_t *packet,
				    bool ring_doorbell);

/// Generate the next sequence number for the control ring usage
uint16_t ar_pci_bar_next_cmd_sequence_number(struct pci_dev *dev);

/**
 * Get the ARP device information.
 * This call will block on the first call until it has fetched the info from
 * the ARP. Following calls will return immediately.
 *
 * @param[in] dev the pci device object, with the driver context
 *
 * @retval arp_info_t with the ARP's information
 * @retval NULL on error fetching the ARP info
 */
arp_info_t *ar_pci_get_arp_info(struct pci_dev *dev, uint32_t timeout_ms);

/**
 * Notify the ARP of the aperture region. This call blocks until the ARP replies.
 *
 * @param[in] dev the pci device object, with the driver context
 *
 * @retval 0 on success
 */
int ar_pci_map_aperture_cmd(struct pci_dev *dev, uint32_t timeout_ms);

/**
 * Notify the ARP of the duty cycle. This call blocks until the ARP replies.
 *
 * @param[in] dev the pci device object, with the driver context
 * @param[in] freq_hz the duty cycle rate in Hz
 *
 * @retval 0 on success
 */
int ar_pci_bar_enable_duty_cycle(struct pci_dev *dev, int freq_hz,
				 uint32_t timeout_ms);

/**
 * Disable the duty cycle. This call blocks until the ARP replies.
 *
 * @param[in] dev the pci device object, with the driver context
 *
 * @retval 0 on success
 */
int ar_pci_bar_disable_duty_cycle(struct pci_dev *dev, uint32_t timeout_ms);

/**
 * Returns true if the doorbell is not called in the caller context
 */
bool ar_pci_bar_doorbell_deferred(void);

enum hrtimer_restart ar_pci_doorbell_timer_callback(struct hrtimer *timer);

#endif // !AR_PCI_BAR_H
