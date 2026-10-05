/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file data_ring.h
 *
 * @brief Ring buffer header for PCI data
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_PCI_DATA_RING_H
#define AR_PCI_DATA_RING_H

#include <arfw_ops.h>
#include <ar_fw_message.h>

#include "ar_pci_int.h"
#include "ar_future.h"

#define CLIENT_RING_HEADER_SIZE 64
#define CLIENT_RING_HEADER_PADDING_SIZE \
	(CLIENT_RING_HEADER_SIZE - sizeof(ar_firmware_message_header_t))

struct ar_pci_bar_data_ring {
	bool in_use;
	uint16_t cached_ring_index;

	// T208533565: remove deprecated fields. These fields have sense only for
	// BAR space. Once queue indexes moved to RAM it could be removed.
	uint16_t *cached_arp_index;
	uint16_t *cached_ap_index;

	uint32_t rd_index_reg;
	uint32_t wr_index_reg;

	// The ring ID used at create/destroy time
	pcie_ring_id_t create_ring_id;
};
typedef struct ar_pci_bar_data_ring ar_pci_bar_data_ring_t;

/**
 * This struct is used as a promise for command replies.
 * When a reply comes in with the corresponding seq_num,
 * the reply_complete will signal a completion.
 *
 * The reply pointer contains an allocated copy of the message.
 * It must be freed by the holder of the promise.
 */
typedef struct ar_pci_cmd_reply_promise {
	ar_promise_t base;
	struct ar_client_queue *queue;
	uint16_t seq_num;
	struct pci_dev *dev;
	void (*complete)(struct ar_pci_cmd_reply_promise *, pcie_ctrl_resp_t *);
} ar_pci_cmd_reply_promise_t;

/**
 * Convenience structure for looking at client ring entries.
 *
 * The ar_ipc_header portion is not transferred to/from the firmware. It is only
 * used to communicate payload information between the driver and the client.
 *
 * Firmware will produce/consume beginning at the pcie_common_header
 *
 */
typedef struct {
	/// Ar Firmware IPC header, not referenced by the firmware
	ar_firmware_message_header_t ar_ipc_header;

	/// Padding to the cache line size
	uint8_t padding[CLIENT_RING_HEADER_PADDING_SIZE];

	/// ring entry, with src or dst payload types
	union {
		pcie_common_header_t common_header;
		pcie_ipc_common_header_t ipc_common_header;
		pcie_ap_dst_data_item_t ap_dst_data;
		pcie_ap_src_data_item_t ap_src_data;
	};
} ar_pci_client_ring_entry_t;

// Assert for backwards compatibility and to ensure that offsets
// were not changed to deviate from each other across message types.
static_assert(offsetof(ar_pci_client_ring_entry_t,
		       ap_src_data.pyld_inline.data) ==
	      offsetof(ar_pci_client_ring_entry_t,
		       ap_dst_data.pyld_inline.data));
static_assert(offsetof(ar_pci_client_ring_entry_t,
		       ap_src_data.data_msg.inline_data) ==
	      offsetof(ar_pci_client_ring_entry_t,
		       ap_dst_data.data_msg.inline_data));
static_assert(offsetof(ar_pci_client_ring_entry_t,
		       ap_src_data.data_msg.inline_data) ==
	      offsetof(ar_pci_client_ring_entry_t,
		       ap_src_data.pyld_inline.data));

/**
 * Allocate all available entries in a queue for receiving and update the read
 * index for the queue.
 *
 * This is the greedy approach to pre-reserve all the slots in the queue to
 * update the read index shared with firmware. The alternative would be to
 * update the read index on a write from firmware. However, if FW manages to
 * fill the queue, we could stall.
 *
 * The result is that on interrupt, we need to use the write index to find the
 * corresponding slot in the queue instead of being able to call reserve buffer.
 *
 * @param[in] arfw_queue The representation of the client queue
 * @param[in] queue The driver representation of the client queue
 */
void ar_client_rcv_ring_update_read_ptr(arfw_client_queue_t arfw_queue,
					ar_client_queue_t *queue);

/**
 * Called when an interrupt arrives to service all active receive rings on
 * the list.
 *
 * @param[in] queue_list The list of receive rings to service
 */
void ar_client_rcv_ring_list_service(ar_pci_client_list_t *queue_list);

/**
 * Called when an interrupt arrives to service all active send rings on the
 * list.
 *
 * @param[in] queue_list The list of send rings to service
 */
void ar_client_send_ring_list_service(ar_pci_client_list_t *queue_list);

/**
 * Submit a payload to a client data ring
 *
 * @param[in] client_queue The driver representation of the queue
 * @param[in] req The IO request to submit
 * @param[out] consumed True if the ring entry should be mark consumed
 *
 * @retval true When the data is submitted
 */
bool ar_pci_send_ring_data(ar_client_queue_t *client_queue,
			   const ar_firmware_io_request_t *req, bool *consumed);

/**
 * Allocate a send ring from the driver context
 *
 * @param[in] dev the pci device, with driver context
 * @param[out] ring_entry Pointer to the ring entry
 * @param[out] ring_index Index into the driver array
 *
 * @retval 0 Success
 * @retval ENOMEM No slots were available
 */
int ar_pci_allocate_send_data_ring_slot(struct pci_dev *dev,
					ar_pci_bar_data_ring_t **ring_entry,
					uint32_t *ring_index);

/**
 * Allocate a receive ring from the driver context
 *
 * @param[in] dev the pci device, with driver context
 * @param[out] ring_entry Pointer to the ring entry
 * @param[out] ring_index Index into the driver array
 *
 * @retval 0 Success
 * @retval ENOMEM No slots were available
 */
int ar_pci_allocate_rcv_data_ring_slot(struct pci_dev *dev,
				       ar_pci_bar_data_ring_t **ring_entry,
				       uint32_t *ring_index);

/**
 * Mark a ring entry as available
 *
 * @param[in] driver The driver context
 * @param[in] ring_entry The previously allocated slot
 */
void ar_pci_release_data_ring_slot(ar_pci_driver_t *driver,
				   ar_pci_bar_data_ring_t *ring_entry);

/**
 * Create a client data ring with firmware
 *
 * @param[in] dev the pci device, with driver context
 * @param[in] queue_params The parameters for the queue
 * @param[in] arfw_queue The ArFirmware class client context
 * @param[in] queue_data Queue data memory region
 * @param[in] timeout_ms The max time allowed for future to complete in ms
 *
 * @retval ar_future_t<0> Success
 * @retval ar_future_t<errno> Failure
 * @retval NULL Could not create/setup a future or a promise
 */
ar_future_t *ar_pci_create_data_ring(
	struct pci_dev *dev,
	const struct arfw_client_queue_create_params *queue_params,
	arfw_client_queue_t arfw_queue,
	struct arfw_queue_mem_region *queue_data, unsigned long timeout_ms);

/**
 * Destroy a client data ring with firmware
 *
 * @param[in] dev the pci device, with driver context
 * @param[in] client_queue The ArFirmware class client context
 * @param[in] timeout_ms The max time allowed for future to complete in ms
 *
 * @retval ar_future_t<0> Success
 * @retval ar_future_t<errno> Failure
 * @retval NULL Could not create/setup a future or a promise
 */
ar_future_t *ar_pci_destroy_data_ring(struct pci_dev *dev,
				      ar_client_queue_t *client_queue,
				      unsigned long timeout_ms);

/**
 * Destroy all client data rings with firmware
 *
 * @param[in] dev The device with driver context set
 * @param[in] timeout_ms The max time allowed for future to complete in ms
 *
 * @retval ar_future_t<0> Success
 * @retval ar_future_t<errno> Failure
 * @retval NULL Could not create/setup a future or a promise
 */
ar_future_t *ar_pci_destroy_all_data_rings(struct pci_dev *dev,
					   unsigned long timeout_ms);

/**
 * Pend a receive buffer from the client to the firmware
 *
 * @param[in] dev the pci device, with driver context
 * @param[in] client_queue The driver representation of the client queue
 * @param[in] buffer The data pipeline buffer with associated ARFW information
 *
 * @retval ar_future_t<0> Success
 * @retval ar_future_t<errno> Failure
 */
int ar_pci_submit_receive_buffer(struct pci_dev *dev,
				 ar_client_queue_t *client_queue,
				 const struct arfw_payload_pend_req *buffer);

/**
 * Get the PCI ops to support old firmware API which uses different message
 * types to send message.
 *
 * @retval pointer to the arfw_pci_proto_ops structure
 */
const struct arfw_pci_proto_ops *ar_pci_get_ar_pci_multiple_ops(void);

/**
 * Get the PCI ops to support firmware API which uses unified message
 * type to send all types of messages.
 *
 * @retval pointer to the arfw_pci_proto_ops structure
 */
const struct arfw_pci_proto_ops *ar_pci_get_ar_pci_unified_ops(void);

#endif // !AR_PCI_DATA_RING_H
