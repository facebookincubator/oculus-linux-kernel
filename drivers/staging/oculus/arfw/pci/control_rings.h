/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file control_rings.h
 *
 * @brief Control ring header for AR accelorator
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_PCI_CONTROL_RINGS_H
#define AR_PCI_CONTROL_RINGS_H

#include "linux/dma-mapping.h"

/// Depth of the ap src control ring, in entries
#define AR_PCI_AP_SRC_RING_DEPTH PCIE_AP_SRC_CTRL_RING_EL

/// Depth of the ap dest control ring, in entries
#define AR_PCI_AP_DST_RING_DEPTH PCIE_AP_DST_CTRL_RING_EL

/// Depth of the ap src buffer ring, in entries
#define AR_PCI_AP_SRC_BUFFER_RING_DEPTH PCIE_AP_SRC_BUF_RING_EL

/// Representation of a single control ring
struct ar_pci_control_ring_context {
	/// Representation of the memory allocated to share with the driver
	dma_addr_t dma_handle;
	void *va;
	size_t va_size;

	size_t element_size;
	uint16_t element_count;

	bool receive_ring;

	/**
	 * Representation of the last value read from the ARP status block.
	 * For a receive ring, this would be the write index from ARP to note new data
	 * to consume in the ring. For a send ring, this would be read index to note a
	 * payload has been consumed.
	 */
	uint16_t cached_ring_index;

	/// "register" addresses for ring indexes, cached at startup.
	uint32_t rd_index_reg;
	uint32_t wr_index_reg;

	// Internal ringbuffer values
	uint32_t local_rd_index;
	uint32_t local_wr_index;

	/**
	 * The driver queues reuse the xr circular queue.
	 * While this incurs a small bit of extra overhead, the clarity it provides
	 * to code when dealing with DMA driven updates over PCI should be helpful.
	 *
	 * The produce_queue is used by the driver to allocate slots for DMA target
	 * from PCIe and to update the queue status after DMA complete. The
	 * consume_queue view is used after MSI to update queue state and access DMA
	 * sourced data.
	 */
	// xr_circular_queue_t produce_queue;
	// xr_circular_queue_t consume_queue;

	/// Allocation for the circular queue book keeping data
	void *cq_consume;

	/**
	 * Lock used to protect agaist parallel updates of the ring index
	 * Consider moving the index update operation to a serial queue
	 */
	struct mutex lock;
};
typedef struct ar_pci_control_ring_context ar_pci_control_ring_context_t;

/**
 * Create the driver control rings to exchange DMA messages with firmware
 *
 * @param[in] dev the pci device, with the driver context already set
 *
 * @retval 0 on success.
 */
int ar_pci_control_rings_create(struct pci_dev *dev);

/**
 * Destroy the driver controll rings
 *
 * @param[in] pci_dev, with the driver context already set
 */
void ar_pci_control_rings_destroy(struct pci_dev *dev);

/**
 * Submit data to an AP source control ring
 *
 * @param[in] ring The control ring to submit to
 * @param[out] index The resulting index where the data was written
 * @param[in] data Pointer to the data to be copied into the ring
 * @param[in] data_size The size, in bytes, of data
 *
 * @retval 0 The data was updated
 * @retval ENOMEM The ring was full, Awaiting reads from firmware
 */
int ap_src_ring_produce(ar_pci_control_ring_context_t *ring, uint32_t *index,
			void *data, size_t data_size);

/**
 * Update an AP source ring to indicate data was consumed by hardware. This may
 * need to be called repeatedly if firmware updates indexes more than a single
 * hop.
 *
 * @param[in] ring The control ring where the submit buffer was consumed
 * @param[in] index The ring index to mark as consumed
 *
 * @retval 0 on success
 */
int ap_src_ring_mark_consumed(ar_pci_control_ring_context_t *ring,
			      uint32_t index);

/**
 * Indicate that a slot is ready to receive data on the ap dst ring from
 * firmware via DMA. This can be used to update the read index that will be
 * written to BAR space.
 *
 * @param[in] ring The control ring
 * @param[out] index The resulting index in the ring that is now available for
 * data.
 *
 * @retval ENOMEM The ring is full allocated
 * @retval 0 A slot was reserved, see the resulting index
 */
int ap_dst_ring_reserve_for_produce(ar_pci_control_ring_context_t *ring,
				    uint32_t *index);

/**
 * Update an ap dst control ring book keeping to indicate data is present. This
 * is done when an MSI is received and BAR space write indexes have been
 * updated. Multiple calls may be required if the write index was updated more
 * than a single slot.
 *
 * @param[in] ring The control ring
 * @param[in] index The index of the slot to mark produced
 *
 * @retval 0 on success
 */
int ap_dst_ring_mark_produced(ar_pci_control_ring_context_t *ring,
			      uint32_t index);

/**
 * Consume data from an ap dst ring, if available. Data is copied out instead of
 * referenced. This helps ensure that the cache line is never written and
 * therefor safe to flush+invalidate after hardware has finished DMA.
 *
 * @param[in] ring The control ring
 * @param[out] index The index of the slot where data was consumed
 * @param[out] data A pointer to a buffer suitable to receive the data
 * @param[in] data_size The size, in bytes, of data
 *
 * @retval ENOMEM No data was available
 * @retval 0 Data was copied
 */
int ap_dst_ring_consume(ar_pci_control_ring_context_t *ring, uint32_t *index,
			void *data, size_t data_size);

void *ar_pci_control_ring_get_base_ptr(ar_pci_control_ring_context_t *ring);

#endif // !AR_PCI_CONTROL_RINGS_H
