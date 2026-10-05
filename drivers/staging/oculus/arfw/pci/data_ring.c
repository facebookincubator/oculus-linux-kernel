// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file data_ring.c
 *
 * @brief Ring buffer implementation for PCI data
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/
#include "data_ring.h"

#include <linux/err.h>

#include <ar_common.h>
#include <arfw_log.h>

#include "ar_pci_bar.h"

static void ar_client_ring_add_active(ar_pci_client_list_t *queue_list,
				      ar_client_queue_t *queue)
{
	AR_ASSERT(queue);
	AR_ASSERT(queue_list);

	mutex_lock(&queue_list->lock);
	queue->lock = &queue_list->lock;
	list_add_tail(&queue->node, &queue_list->list_node);
	mutex_unlock(&queue_list->lock);
}

static void ar_client_ring_remove_active(ar_client_queue_t *queue)
{
	AR_ASSERT(queue);

	mutex_lock(queue->lock);
	list_del(&queue->node);
	mutex_unlock(queue->lock);
}

static void ar_client_ring_remove_replies(ar_pci_driver_t *driver,
					  ar_client_queue_t *queue)
{
	unsigned long flags;
	ar_pci_cmd_reply_promise_t *cmd_promise;
	ar_promise_t *promise = NULL, *temp = NULL;

	AR_ASSERT(driver);
	AR_ASSERT(queue);

	spin_lock_irqsave(&driver->cmd_reply_lock, flags);
	list_for_each_entry_safe(promise, temp, &driver->cmd_reply_list, list) {
		cmd_promise =
			container_of(promise, ar_pci_cmd_reply_promise_t, base);
		if (queue == cmd_promise->queue) {
			list_del(&cmd_promise->base.list);
			kfree(cmd_promise);
		}
	}
	spin_unlock_irqrestore(&driver->cmd_reply_lock, flags);
}

static uint32_t dma_buffer_len(const struct arfw_dma_sg_elem *dma_sg_list,
			       uint32_t sg_list_len)
{
	int i;
	uint32_t len = 0;

	for (i = 0; i < sg_list_len; i++)
		len += dma_sg_list[i].len;

	return len;
}

/*
 * Create a pcie_sg_el_t buffer list from a client buffer
 * Note: the client buffer must be allocated in DMA memory
 */
static void construct_pcie_sg_list(pcie_sg_el_t *buf_list,
				   const struct arfw_dma_sg_elem *sg_list,
				   size_t sg_list_len)
{
	int i;

	// the buffers may overlap, but dma_sg_list addr < buf_list addr.
	for (i = sg_list_len - 1; i >= 0; i--) {
		buf_list[i].len = sg_list[i].len;
		buf_list[i].buf_addr = sg_list[i].addr;
	}
}

// ar_pci_multiple_* is a backwards compatibility API to support the old
// firmware, old message payloads and types.
static bool
ar_pci_proto_multiple_set_payload(ar_firmware_message_header_t *ar_ipc_header,
				  void *ring_entry)
{
	bool ret = true;
	ar_pci_client_ring_entry_t *entry = ring_entry;
	uint32_t buf_num;

	if (ar_ipc_header->data_location == ARFW_BUFFER_LOC_NONE ||
	    ar_ipc_header->data_location == ARFW_BUFFER_LOC_IN_LINE) {
		pcie_ap_src_data_inline_item_t *pyld_inline =
			&entry->ap_src_data.pyld_inline;

		// in-line data fields
		pyld_inline->total_len = entry->ar_ipc_header.data_size;

		pyld_inline->common_header.flag = AP_SRC_IPC_PYLD_INLINE;
	} else if (entry->ar_ipc_header.data_location ==
		   ARFW_BUFFER_LOC_EXTERNAL) {
		pcie_ap_src_data_multi_buffer_list_item_t *payload_list =
			&entry->ap_src_data.pyld_multi_buf;
		buf_num = ar_ipc_header->data_size /
			  sizeof(payload_list->buf_list[0]);
		if (buf_num == 1) {
			// FW backward compatibility: if only one buffer should
			// be sent, then it should be a special AP_SRC_IPC_PYLD_SG_INLINE
			// type.
			pcie_ap_src_data_pyld_list_item_t *payload_single =
				&entry->ap_src_data.pyld_list;
			// The buf_list field in the pcie_ap_src_data_pyld_list_item_t structure shifts
			// for 4 bytes in comparison with the pcie_ap_src_data_multi_buffer_list_item_t
			// structure.
			// Since it is only one element in the array, it is okay to make inplace
			// transformation starting with the very last field. In this case the memory
			// will not be overwritten.
			payload_single->buf_list[0].len =
				payload_list->buf_list[0].size;
			payload_single->buf_list[0].buf_addr =
				payload_list->buf_list[0].addr;
			payload_single->total_ipc_pyld_len =
				payload_single->buf_list[0].len;
			payload_single->num_pyld_bufs = 1;

			payload_single->common_header.flag =
				AP_SRC_IPC_PYLD_SG_INLINE;
		} else {
			payload_list->num_bufs = buf_num;
			// Inplace conversion is transparent see the static_assert checks in this file.

			payload_list->common_header.flag = AP_SRC_IPC_PYLD_LIST;
		}
	} else {
		ret = false;
	}

	return ret;
}

static void ar_pci_proto_multiple_service_ring_entry(void *ring_entry)
{
	ar_pci_client_ring_entry_t *entry = ring_entry;
	pcie_ap_src_data_multi_buffer_list_item_t *payload_list;
	pcie_ap_src_data_pyld_list_item_t *payload_single;

	if (entry->ar_ipc_header.data_location != ARFW_BUFFER_LOC_EXTERNAL)
		return;

	payload_list = &entry->ap_src_data.pyld_multi_buf;
	if (payload_list->common_header.flag != AP_SRC_IPC_PYLD_SG_INLINE)
		return;

	// No special translation is required to handle aperture buffers.
	payload_single = &entry->ap_src_data.pyld_list;
	// Only single buffer case is supported.
	AR_ASSERT(payload_single->num_pyld_bufs == 1);
	payload_list->buf_list[0].addr = payload_single->buf_list[0].buf_addr;
	payload_list->buf_list[0].size = payload_single->buf_list[0].len;
}

static const struct arfw_pci_proto_ops arfw_pci_proto_ops_multiple = {
	.arfw_pci_proto_set_payload = ar_pci_proto_multiple_set_payload,
	.arfw_pci_proto_service_ring_entry =
		ar_pci_proto_multiple_service_ring_entry,
};

const struct arfw_pci_proto_ops *ar_pci_get_ar_pci_multiple_ops(void)
{
	return &arfw_pci_proto_ops_multiple;
}

// ar_pci_unified_* is API which uses unified message type (AP_SRC_IPC_DATA_MSG)
// for all the messages.
static bool
ar_pci_proto_unified_set_payload(ar_firmware_message_header_t *ar_ipc_header,
				 void *ring_entry)
{
	bool ret;
	ar_pci_client_ring_entry_t *entry = ring_entry;

	ret = true;
	if (ar_ipc_header->data_location == ARFW_BUFFER_LOC_NONE ||
	    ar_ipc_header->data_location == ARFW_BUFFER_LOC_IN_LINE) {
		pcie_data_msg_item_t *data_msg = &entry->ap_src_data.data_msg;

		data_msg->inline_msg_len = ar_ipc_header->inline_msg_len;
		data_msg->num_bufs = 0;
		data_msg->common_header.flag = AP_SRC_IPC_DATA_MSG;
	} else if (entry->ar_ipc_header.data_location ==
		   ARFW_BUFFER_LOC_EXTERNAL) {
		pcie_data_msg_item_t *data_msg = &entry->ap_src_data.data_msg;

		data_msg->inline_msg_len = ar_ipc_header->inline_msg_len;
		data_msg->num_bufs = (ar_ipc_header->data_size -
				      ALIGN(ar_ipc_header->inline_msg_len,
					    sizeof(uint64_t))) /
				     sizeof(pcie_ap_multi_buf_element_t);
		// Inplace conversion is transparent see the static_assert checks in this file.

		data_msg->common_header.flag = AP_SRC_IPC_DATA_MSG;
	} else if (entry->ar_ipc_header.data_location ==
		   ARFW_BUFFER_LOC_APERTURE) {
		pcie_data_msg_item_t *data_msg = &entry->ap_src_data.data_msg;

		data_msg->inline_msg_len = ar_ipc_header->inline_msg_len;
		data_msg->num_bufs = (ar_ipc_header->data_size -
				      ALIGN(ar_ipc_header->inline_msg_len,
					    sizeof(uint64_t))) /
				     sizeof(pcie_ap_multi_buf_element_t);

		data_msg->common_header.flag = AP_SRC_IPC_DATA_MSG |
					       AP_SRC_IPC_APERTURE_LIST;

		// Inplace aperture buffer conversion is transparent, see the
		// static_assert checks in this file.
	} else {
		ret = false;
	}

	return ret;
}

static void ar_pci_proto_unified_service_ring_entry(void *ring_entry)
{
	// Nothing to do for the unified message types.
	(void)ring_entry;
}

static const struct arfw_pci_proto_ops arfw_pci_proto_ops_unified = {
	.arfw_pci_proto_set_payload = ar_pci_proto_unified_set_payload,
	.arfw_pci_proto_service_ring_entry =
		ar_pci_proto_unified_service_ring_entry,
};

const struct arfw_pci_proto_ops *ar_pci_get_ar_pci_unified_ops(void)
{
	return &arfw_pci_proto_ops_unified;
}

/*
 * TODO: deprecated will be removed as part of T209702058.
 * Send queue.
 * To perform inplace conversion for the multi buffer case, we are making several
 * assumptions which are checked on during the compile time:
 *   - The pcie_ap_src_data_inline_item_t and pcie_ap_src_data_multi_buffer_list_item_t should be
 *     identical. And the data and buf_list offset should be the same, so the addresses will be
 *     the same.
 * If these assumptions are true, then we could pass data as is without any conversion.
 */
static_assert(sizeof(pcie_ap_src_data_inline_item_t) ==
	      sizeof(pcie_ap_src_data_multi_buffer_list_item_t));
static_assert(offsetof(pcie_ap_src_data_inline_item_t, common_header) ==
	      offsetof(pcie_ap_src_data_multi_buffer_list_item_t,
		       common_header));
static_assert(offsetof(pcie_ap_src_data_inline_item_t, ipc_header) ==
	      offsetof(pcie_ap_src_data_multi_buffer_list_item_t, ipc_header));
static_assert(offsetof(pcie_ap_src_data_inline_item_t, data) ==
	      offsetof(pcie_ap_src_data_multi_buffer_list_item_t, buf_list));
static_assert(offsetof(pcie_ap_src_data_multi_buffer_list_item_t, buf_list) !=
	      offsetof(pcie_ap_src_data_aperture_msg_item_t, buf_list));

/*
 * All data is sent by using AP_SRC_IPC_DATA_MSG. The message could contain both
 * inline messages and SG list of buffers to send.
 *
 * To perform inplace conversion for the multi buffer case, we are making several
 * assumptions which are checked on during the compile time:
 *   - The pcie_ap_multi_buf_element_t and arfw_dma_sg_elem structures should be the
 *     same.
 *   - The inline_data offset in the pcie_data_msg_item_t structure should be equal to
 *     16 (sizeof(pcie_common_header_t) + sizeof(pcie_ipc_header_t) + sizeof(internal_fields)).
 *     In this case there is no shift for data and both AP and FW are using the same data
 *     offset.
 * If these assumptions are true, then we could pass data as is without any addition
 * memory move.
 */
static_assert(sizeof(pcie_ap_multi_buf_element_t) ==
	      sizeof(struct arfw_dma_sg_elem));
static_assert(offsetof(pcie_ap_multi_buf_element_t, addr) ==
	      offsetof(struct arfw_dma_sg_elem, addr));
static_assert(offsetof(pcie_ap_multi_buf_element_t, size) ==
	      offsetof(struct arfw_dma_sg_elem, len));
static_assert(offsetof(pcie_ap_multi_buf_element_t, mem_id) ==
	      offsetof(struct arfw_dma_sg_elem, mem_id));
static_assert(offsetof(pcie_data_msg_item_t, inline_data) == 16);

void ar_client_rcv_ring_update_read_ptr(arfw_client_queue_t arfw_queue,
					ar_client_queue_t *queue)
{
	int ret = 0;
	uint32_t index = UINT_MAX;
	struct arfw_io_request buffer;

	while (ret == 0) {
		ret = queue->driver->client_ops
			      ->handle_client_queue_reserve_request(arfw_queue,
								    &buffer);
		if (ret == 0)
			index = buffer.index;
	}

	if (index != UINT_MAX) {
		index = (index + 1) % queue->queue_params.depth;
		AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_READ,
				     "Update rd_idx [new fw rd_idx: %u]",
				     index);

		*queue->data_ring->cached_ap_index = index;
		if (!ar_pci_bar_doorbell_deferred())
			arfw_mem_util_iomem_write_16(
				queue->driver->ap_update_block_addr,
				queue->data_ring->rd_index_reg,
				(uint16_t)index);

		// notify FW on read ptr update if needed
		if (!atomic_read_acquire(&queue->driver->duty_cycle.enabled))
			ar_pci_bar_ring_doorbell(queue->driver->dev);
	} else {
		AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_READ, "No slots to reserve");
	}
}

bool ar_pci_send_ring_data(ar_client_queue_t *client_queue,
			   const ar_firmware_io_request_t *req, bool *consumed)
{
	int ret = 0;
	uint16_t index = 0;

	struct pci_dev *dev;
	const struct arfw_pci_proto_ops *proto_ops;
	ar_pci_client_ring_entry_t *ring_entry = req->data;
	ar_firmware_message_header_t *ar_ipc_header =
		&ring_entry->ar_ipc_header;
	// copy in ARFW IPC header fields
	pcie_ipc_header_t *pci_ipc_header =
		&ring_entry->ipc_common_header.ipc_header;

	AR_ASSERT(client_queue);
	AR_ASSERT(client_queue->driver);
	dev = client_queue->driver->dev;

	pci_ipc_header->ap_ep_id = client_queue->queue_params.hlos_endpoint_id;
	pci_ipc_header->arp_ep_id = client_queue->queue_params.fw_endpoint_id;
	pci_ipc_header->msg_id = ar_ipc_header->msg_id;
	pci_ipc_header->tracking_num = ar_ipc_header->tracking_id;
	pci_ipc_header->sequence_num = ar_ipc_header->sequence_id;

	// set the pcie sequence number for the queue
	ring_entry->common_header.seq_num = client_queue->seq_num++;

	// Message type is Ap source buffer
	ring_entry->common_header.msg_type = AP_SRC_DATA_MSG;

	proto_ops = client_queue->driver->proto_ops;
	AR_ASSERT(proto_ops);
	AR_ASSERT(proto_ops->arfw_pci_proto_set_payload);
	if (!proto_ops->arfw_pci_proto_set_payload(ar_ipc_header, ring_entry)) {
		AR_LOG_PCI_QUEUE_ERR(client_queue, AR_LOG_WRITE,
				     "Unsupported data location [location: %d]",
				     ar_ipc_header->data_location);
		goto error_tear_down;
	}

	index = (uint16_t)req->index;
	client_queue->send_queue_write_index = index;

	AR_LOG_PCI_QUEUE_DBG(
		client_queue, AR_LOG_WRITE,
		"Send Ring entry [index %u, msg_id: 0x%x, ar seq: %u, tracking: %d, seq: %u]",
		index, pci_ipc_header->msg_id, pci_ipc_header->sequence_num,
		pci_ipc_header->tracking_num,
		ring_entry->common_header.seq_num);

	index++;
	index = index % client_queue->queue_params.depth;

	*client_queue->data_ring->cached_ap_index = index;
	// Update the write index for fw
	if (!ar_pci_bar_doorbell_deferred())
		arfw_mem_util_iomem_write_16(
			client_queue->driver->ap_update_block_addr,
			client_queue->data_ring->wr_index_reg, index);

	// notify the fw if needed
	if (!atomic_read_acquire(&client_queue->driver->duty_cycle.enabled) ||
	    ar_ipc_header->immediate)
		ar_pci_bar_ring_doorbell(dev);

	// The ring entry cannot be re-used until firmware has completed the DMA
	*consumed = false;

	return true;

error_tear_down:
	/*
	 * This abandons a slot in the queue.
	 * We need to notify the client and mark the queue for shutdown.
	 * No actual tear down will happen on this thread.
	 * In case we are already shutting down swallow the error.
	 */
	client_queue->released = true;
	if (client_queue->driver->client_ops->handle_client_queue_shutdown(
		    client_queue->arfw_queue,
		    AR_QUEUE_SHUTDOWN_PROTOCOL_STATE) == -ESHUTDOWN)
		AR_LOG_PCI_QUEUE_DBG(
			client_queue, AR_LOG_WRITE,
			"Protocol error in send queue, but the queue is already shutting down [err: %d].",
			ret);
	else
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_WRITE,
			"Protocol error in send queue. Initiated client queue shutdown [err: %d].",
			ret);

	*consumed = true;
	return false;
}

// Internal function to populate an io request based off ring index
static void io_request_from_index(ar_client_queue_t *queue, uint16_t index,
				  ar_firmware_io_request_t *buffer)
{
	/*
	 * Find the expected buffer based off the updated index.
	 */
	uint8_t *queue_base = queue->queue_params.queue_location;

	buffer->index = queue->data_ring->cached_ring_index;
	buffer->data_size = queue->queue_params.element_size;
	buffer->data = queue_base + (buffer->index * buffer->data_size);
}

/*
 * Receive queue.
 * To perform inplace conversion for the multi buffer case, we are making several
 * assumptions which are checked on during the compile time:
 *   - The pcie_ap_dst_multi_buf_element_t and ar_firmware_msg_sg_recv_t structures should be the
 *     same.
 *   - The pcie_ap_dst_data_inline_item_t and pcie_ap_dst_data_multi_buffer_list_item_t should be
 *     identical. And the data and buf_list offset should be the same, so the addresses will be
 *     the same.
 * If these assumptions are true, then we could pass data as is without any conversion.
 */
static_assert(sizeof(pcie_ap_dst_multi_buf_element_t) ==
	      sizeof(ar_firmware_msg_sg_recv_t));
static_assert(offsetof(pcie_ap_dst_multi_buf_element_t, size) ==
	      offsetof(ar_firmware_msg_sg_recv_t, size));
static_assert(offsetof(pcie_ap_dst_multi_buf_element_t, id) ==
	      offsetof(ar_firmware_msg_sg_recv_t, roundtrip));
static_assert(offsetof(pcie_ap_dst_multi_buf_element_t, _unused) ==
	      offsetof(ar_firmware_msg_sg_recv_t, reserved));
static_assert(sizeof(pcie_ap_dst_data_inline_item_t) ==
	      sizeof(pcie_ap_dst_data_multi_buffer_list_item_t));
static_assert(offsetof(pcie_ap_dst_data_inline_item_t, common_header) ==
	      offsetof(pcie_ap_dst_data_multi_buffer_list_item_t,
		       common_header));
static_assert(offsetof(pcie_ap_dst_data_inline_item_t, ipc_header) ==
	      offsetof(pcie_ap_dst_data_multi_buffer_list_item_t, ipc_header));
static_assert(offsetof(pcie_ap_dst_data_inline_item_t, data) ==
	      offsetof(pcie_ap_dst_data_multi_buffer_list_item_t, buf_list));

static int service_rcv_data_msg(ar_client_queue_t *queue,
				ar_pci_client_ring_entry_t *ring_entry)
{
	int ret, i;
	pcie_data_msg_item_t *data_msg;
	pcie_ap_dst_multi_buf_element_t *buffers;
	ar_firmware_message_header_t *ar_ipc_header;
	uint16_t len_aligned;

	data_msg = &ring_entry->ap_dst_data.data_msg;
	ar_ipc_header = &ring_entry->ar_ipc_header;

	ar_ipc_header->data_location = ARFW_BUFFER_LOC_EXTERNAL;
	ar_ipc_header->inline_msg_len = data_msg->inline_msg_len;
	ret = 0;
	if (data_msg->num_bufs) {
		// Protocol between AP and FW expects the SG buffer start to
		// be aligned.
		len_aligned = ALIGN(data_msg->inline_msg_len, sizeof(uint64_t));
		ar_ipc_header->data_size =
			len_aligned + data_msg->num_bufs * sizeof(*buffers);
		buffers = (pcie_ap_dst_multi_buf_element_t
				   *)(data_msg->inline_data + len_aligned);
		for (i = 0; i < data_msg->num_bufs; i++) {
			ret = queue->driver->client_ops
				      ->handle_client_payload_pend_released(
					      queue->arfw_queue, buffers[i].id);
			if (ret) {
				AR_LOG_PCI_QUEUE_ERR(
					queue, AR_LOG_READ,
					"Can't pend release buffer id: %08x",
					buffers[i].id);
				break;
			}
			// No other coversion to the ar_firmware_msg_sg_recv_t structure is needed.
			// This is in place conversion protected by the compile static assert checks
			// above.
		}
	} else {
		ar_ipc_header->data_size = data_msg->inline_msg_len;
	}

	return ret;
}

static void service_rcv_queue(struct pci_dev *dev, ar_client_queue_t *queue,
			      uint16_t new_ring_index)
{
	int i;

	if (AR_UNLIKELY(new_ring_index >= queue->queue_params.depth)) {
		AR_LOG_PCI_QUEUE_ERR(
			queue, AR_LOG_WRITE,
			"new_ring_index: 0x%04x is broken, it should be less than queue depth: 0x%04x.",
			new_ring_index, queue->queue_params.depth);
		return;
	}

	// Attempt to consume all read data and dispatch it
	while (queue->data_ring->cached_ring_index != new_ring_index) {
		int ret = 0;
		ar_pci_client_ring_entry_t *ring_entry;
		ar_firmware_message_header_t *ar_ipc_header;
		pcie_ipc_common_header_t *pci_ipc_common_header;
		ar_firmware_io_request_t buffer;

		io_request_from_index(
			queue, queue->data_ring->cached_ring_index, &buffer);

		// Pointer for ar firmware transport header to client
		ring_entry = (ar_pci_client_ring_entry_t *)buffer.data;

		// We should only see ap destination message on this ring
		if (ring_entry->common_header.msg_type != AP_DST_DATA_MSG) {
			AR_LOG_PCI_QUEUE_ERR(
				queue, AR_LOG_READ,
				"Unexpected payload format on data ring [msg_type: %u]",
				ring_entry->common_header.msg_type);
			ret = -EINVAL;
			goto done;
		}

		ar_ipc_header = &ring_entry->ar_ipc_header;
		pci_ipc_common_header = &ring_entry->ipc_common_header;

		// Populate common ARFW IPC portions of the header
		ar_ipc_header->msg_id =
			pci_ipc_common_header->ipc_header.msg_id;
		ar_ipc_header->tracking_id =
			pci_ipc_common_header->ipc_header.tracking_num;
		ar_ipc_header->sequence_id =
			pci_ipc_common_header->ipc_header.sequence_num;

		// Populate the payload location specific portions of the message
		if ((ring_entry->common_header.flag & AP_DST_IPC_PYLD_INLINE) !=
		    0) {
			// Inline messages need data size and location set
			pcie_ap_dst_data_inline_item_t *pyld_inline =
				&ring_entry->ap_dst_data.pyld_inline;

			if (pyld_inline->total_len != 0)
				ar_ipc_header->data_location =
					ARFW_BUFFER_LOC_IN_LINE;
			else
				ar_ipc_header->data_location =
					ARFW_BUFFER_LOC_NONE;

			ar_ipc_header->data_size = pyld_inline->total_len;
			ar_ipc_header->inline_msg_len = pyld_inline->total_len;

		} else if ((ring_entry->common_header.flag &
			    AP_DST_IPC_PYLD_BUFID) != 0) {
			// TODO: T200523652: support the deprecated AP_DST_IPC_PYLD_BUFID message
			// type. This type is used if there is only one external buffer in the
			// message. Waiting for support from the FW endpoints to fully change
			// to the multiple buffer message type.
			// As soon as intergation work will be completed this could be removed.
			// For now make the transformation from the pcie_ap_dst_data_buf_item_t
			// received from FW to the pcie_ap_dst_data_multi_buffer_list_item_t type
			// which is handled properly by user space. In this case pend id and
			// length is passed inside the SG list and not in the AR IPC header.
			// the pcie_ap_src_data_pyld_list_item_t structure.
			pcie_ap_dst_data_buf_item_t *pyld_buf =
				&ring_entry->ap_dst_data.pyld_buf;
			pcie_ap_dst_data_multi_buffer_list_item_t *pyld_multibuf =
				&ring_entry->ap_dst_data.pyld_multi_buf;

			// pcie_ap_dst_data_buf_item_t has no SG list, but since we will make
			// translation to pcie_ap_dst_data_multi_buffer_list_item_t we should
			// be sure that there are enough space.
			AR_ASSERT(queue->max_num_sg_entries >= 1);

			ar_ipc_header->data_location = ARFW_BUFFER_LOC_EXTERNAL;
			// In case of SG list data size should contain the size of the SG array.
			ar_ipc_header->data_size =
				sizeof(pyld_multibuf->buf_list[0]);
			ar_ipc_header->inline_msg_len = 0;

			ret = queue->driver->client_ops
				      ->handle_client_payload_pend_released(
					      queue->arfw_queue,
					      pyld_buf->dst_buf_id);
			// Perform translation to the one item SG list, starting from the end since it
			// is inplace translation.
			pyld_multibuf->buf_list[0].id = pyld_buf->dst_buf_id;
			pyld_multibuf->buf_list[0].size = pyld_buf->total_len;
		} else if ((ring_entry->common_header.flag &
			    AP_DST_IPC_PYLD_LIST) != 0) {
			pcie_ap_dst_data_multi_buffer_list_item_t *pyld_buf =
				&ring_entry->ap_dst_data.pyld_multi_buf;

			ar_ipc_header->data_location = ARFW_BUFFER_LOC_EXTERNAL;
			// fill in the IPC and buffer size
			ar_ipc_header->data_size =
				pyld_buf->num_bufs *
				sizeof(pyld_buf->buf_list[0]);
			ar_ipc_header->inline_msg_len = 0;

			for (i = 0; i < pyld_buf->num_bufs; i++) {
				ret = queue->driver->client_ops
					      ->handle_client_payload_pend_released(
						      queue->arfw_queue,
						      pyld_buf->buf_list[i].id);
				// No coversion to the ar_firmware_msg_sg_recv_t structure is needed.
				// This is in place conversion protected by the compile static assert checks
				// above.
			}
		} else if ((ring_entry->common_header.flag &
			    AP_DST_IPC_DATA_MSG) != 0) {
			ret = service_rcv_data_msg(queue, ring_entry);
		} else {
			AR_LOG_PCI_QUEUE_ERR(
				queue, AR_LOG_READ,
				"Unsupported payload type from firmware");
			ret = -EINVAL;
		}

done:
		// if parsing went correctly, callback to the client
		if (ret == 0) {
			AR_LOG_PCI_QUEUE_DBG(
				queue, AR_LOG_READ,
				"Received Ring entry [loc: %d, index: %u, msg_id: 0x%x, ar seq: %u, tracking: %d, seq: %u]",
				ar_ipc_header->data_location,
				queue->data_ring->cached_ring_index,
				ring_entry->ipc_common_header.ipc_header.msg_id,
				ring_entry->ipc_common_header.ipc_header
					.sequence_num,
				ring_entry->ipc_common_header.ipc_header
					.tracking_num,
				ring_entry->common_header.seq_num);

			ret = queue->driver->client_ops
				      ->handle_client_queue_produce_request(
					      queue->arfw_queue,
					      (struct arfw_io_request *)&buffer);
			if (ret) {
				AR_LOG_PCI_QUEUE_ERR(
					queue, AR_LOG_READ,
					"Failed to produce data to client [err: %d]",
					ret);
				// Todo (dpredmore): How should we handle this?
			}
		} else {
			/*
			 * This abandons a slot in the queue.
			 * We need to notify the client and mark the queue for shutdown.
			 * No actual tear down will happen on this thread.
			 * In case we are already shutting down swallow the error.
			 */
			queue->released = true;
			if (queue->driver->client_ops
				    ->handle_client_queue_shutdown(
					    queue->arfw_queue,
					    AR_QUEUE_SHUTDOWN_PROTOCOL_STATE) ==
			    -ESHUTDOWN)
				AR_LOG_PCI_QUEUE_DBG(
					queue, AR_LOG_READ,
					"Protocol error in recv queue, but the queue is already shutting down [err: %d].",
					ret);
			else
				AR_LOG_PCI_QUEUE_ERR(
					queue, AR_LOG_READ,
					"Protocol error in recv queue. Initiated client queue shutdown [err: %d].",
					ret);
		}

		// increment, modulo ring size
		queue->data_ring->cached_ring_index =
			(queue->data_ring->cached_ring_index + 1) %
			queue->queue_params.depth;

		// no need to continue consuming released queues
		if (queue->released)
			break;
	}
}

void ar_client_rcv_ring_list_service(ar_pci_client_list_t *queue_list)
{
	ar_client_queue_t *queue = NULL;

	mutex_lock(&queue_list->lock);

	// If release started due to deprobe we should not service data rings.
	if (AR_UNLIKELY(queue_list->released))
		goto unlock_queues;

	list_for_each_entry(queue, &queue_list->list_node, node) {
		uint16_t new_write_index;
		int ready;

		if (AR_UNLIKELY(queue->released)) {
			AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_READ,
					     "Queue is released and skipped");
			continue;
		}

		ready = ar_atomic_load(&queue->ready, AR_MEMORY_ORDER_SEQ_CST);
		if (AR_UNLIKELY(!ready)) {
			AR_LOG_PCI_DBG(AR_LOG_READ,
				       "Queue not ready [queue %p]",
				       queue->arfw_queue);
			continue;
		}

		new_write_index = *queue->data_ring->cached_arp_index;
		if (queue->data_ring->cached_ring_index != new_write_index) {
			AR_LOG_PCI_QUEUE_DBG(
				queue, AR_LOG_READ,
				"AP_DST_DATA_RING Data received [cached wr_idx: %u, bar wr_idx: %u]",
				queue->data_ring->cached_ring_index,
				new_write_index);
			service_rcv_queue(queue->driver->dev, queue,
					  new_write_index);
		}
	}

unlock_queues:
	mutex_unlock(&queue_list->lock);
}

static void service_send_queue(ar_client_queue_t *queue,
			       uint16_t new_ring_index)
{
	const struct arfw_pci_proto_ops *proto_ops;

	AR_ASSERT(queue);
	AR_ASSERT(queue->driver);
	if (AR_UNLIKELY(new_ring_index >= queue->queue_params.depth)) {
		AR_LOG_PCI_QUEUE_ERR(
			queue, AR_LOG_WRITE,
			"new_ring_index: 0x%04x is broken, it should be less than queue depth: 0x%04x.",
			new_ring_index, queue->queue_params.depth);
		return;
	}

	proto_ops = queue->driver->proto_ops;
	AR_ASSERT(proto_ops);
	AR_ASSERT(proto_ops->arfw_pci_proto_service_ring_entry);
	while (queue->data_ring->cached_ring_index != new_ring_index) {
		ar_firmware_io_request_t buffer;
		ar_pci_client_ring_entry_t *ring_entry;

		io_request_from_index(queue, new_ring_index, &buffer);

		ring_entry = (ar_pci_client_ring_entry_t *)buffer.data;

		AR_LOG_PCI_QUEUE_DBG(
			queue, AR_LOG_WRITE,
			"Ring entry sent [index: %u, new_ring_index: %u, msg_id: 0x%x, session: %u, tracking: %d, seq: %u]",
			queue->data_ring->cached_ring_index, new_ring_index,
			ring_entry->ipc_common_header.ipc_header.msg_id,
			ring_entry->ipc_common_header.ipc_header.sequence_num,
			ring_entry->ipc_common_header.ipc_header.tracking_num,
			ring_entry->common_header.seq_num);

		proto_ops->arfw_pci_proto_service_ring_entry(ring_entry);

		// Mark the queue entry read for re-use
		if (queue->driver->client_ops
			    ->handle_client_queue_index_consumed(
				    queue->arfw_queue,
				    queue->data_ring->cached_ring_index))
			AR_LOG_PCI_QUEUE_ERR(
				queue, AR_LOG_WRITE,
				"consumed failed: Ring entry sent [index: %u, new_ring_index: %u, msg_id: 0x%x, session: %u, tracking: %d, seq: %u]",
				queue->data_ring->cached_ring_index,
				new_ring_index,
				ring_entry->ipc_common_header.ipc_header.msg_id,
				ring_entry->ipc_common_header.ipc_header
					.sequence_num,
				ring_entry->ipc_common_header.ipc_header
					.tracking_num,
				ring_entry->common_header.seq_num);

		queue->data_ring->cached_ring_index =
			(queue->data_ring->cached_ring_index + 1) %
			queue->queue_params.depth;
	}
}

void ar_client_send_ring_list_service(ar_pci_client_list_t *queue_list)
{
	ar_client_queue_t *queue = NULL;

	mutex_lock(&queue_list->lock);

	// If release started due to deprobe we should not service data rings.
	if (AR_UNLIKELY(queue_list->released))
		goto unlock_queues;

	list_for_each_entry(queue, &queue_list->list_node, node) {
		uint16_t new_read_index;

		if (AR_UNLIKELY(queue->released)) {
			AR_LOG_PCI_QUEUE_DBG(queue, AR_LOG_READ,
					     "Queue is released and skipped");
			continue;
		}

		new_read_index = *queue->data_ring->cached_arp_index;
		if (queue->data_ring->cached_ring_index != new_read_index) {
			AR_LOG_PCI_QUEUE_DBG(
				queue, AR_LOG_WRITE,
				"AP_SRC_DATA_RING Msg [cached rd_idx: %u, bar rd_idx: %u]",
				queue->data_ring->cached_ring_index,
				new_read_index);
			service_send_queue(queue, new_read_index);
		}
	}

unlock_queues:
	mutex_unlock(&queue_list->lock);
}

static ar_client_queue_t *ar_client_queue_create(
	ar_pci_driver_t *driver, arfw_client_queue_t arfw_queue,
	const struct arfw_client_queue_create_params *queue_params,
	ar_pci_bar_data_ring_t *data_ring, uint16_t seq_num)
{
	struct pci_dev *pci = driver->dev;
	ar_client_queue_t *queue =
		devm_kzalloc(&pci->dev, sizeof(ar_client_queue_t), GFP_KERNEL);

	if (queue == NULL)
		return NULL;

	queue->queue_params = *queue_params;
	queue->data_ring = data_ring;
	queue->seq_num = seq_num;
	queue->driver = driver;
	queue->arfw_queue = arfw_queue;

	queue->max_num_sg_entries = (queue->queue_params.element_size -
				     offsetof(ar_pci_client_ring_entry_t,
					      ap_src_data.pyld_list.buf_list)) /
				    sizeof(pcie_sg_el_t);

	return queue;
}

int ar_pci_allocate_send_data_ring_slot(struct pci_dev *dev,
					ar_pci_bar_data_ring_t **ring_entry,
					uint32_t *ring_index)
{
	int ret = -ENOSPC;
	uint32_t index;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	mutex_lock(&driver->data_ring_lock);
	for (index = 0; index < driver->ap_src_data_ring_max; index++) {
		if (driver->send_data_rings[index].in_use == false) {
			driver->send_data_rings[index].in_use = true;
			*ring_entry = &driver->send_data_rings[index];
			*ring_index = index;
			ret = 0;
			break;
		}
	}
	mutex_unlock(&driver->data_ring_lock);

	return ret;
}

int ar_pci_allocate_rcv_data_ring_slot(struct pci_dev *dev,
				       ar_pci_bar_data_ring_t **ring_entry,
				       uint32_t *ring_index)
{
	int ret = -ENOSPC;
	uint32_t index;
	ar_pci_driver_t *driver = pci_get_drvdata(dev);

	mutex_lock(&driver->data_ring_lock);
	for (index = 0; index < driver->ap_dst_data_ring_max; index++) {
		if (driver->rcv_data_rings[index].in_use == false) {
			driver->rcv_data_rings[index].in_use = true;
			*ring_entry = &driver->rcv_data_rings[index];
			*ring_index = index;
			ret = 0;
			break;
		}
	}
	mutex_unlock(&driver->data_ring_lock);

	return ret;
}

void ar_pci_release_data_ring_slot(ar_pci_driver_t *driver,
				   ar_pci_bar_data_ring_t *ring_entry)
{
	mutex_lock(&driver->data_ring_lock);
	ring_entry->in_use = false;
	mutex_unlock(&driver->data_ring_lock);
}

static int ar_pci_create_data_ring_process_nack(ar_client_queue_t *client_queue,
						uint8_t flag)
{
	int err;

	AR_ASSERT(client_queue);

	switch (flag) {
	case PCIE_CR_RING_REQ_FAIL_DUP:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, ring already exists");
		err = -EEXIST;
		break;
	case PCIE_CR_RING_REQ_FAIL_INVALID_CNT:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, invalid ring count");
		err = -ERANGE;
		break;
	case PCIE_CR_RING_REQ_FAIL_INVALID_RING_TYPE:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, invalid ring type");
		err = -EINVAL;
		break;
	case PCIE_CR_RING_REQ_FAIL_INVALID_ITEM_SIZE:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, invalid inline message length");
		err = -E2BIG;
		break;
	case PCIE_CR_RING_REQ_FAIL_INVALID_EP:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, endpoint unreachable");
		err = -ENXIO;
		break;
	case PCIE_CR_RING_REQ_FAIL_INVALID_DEPTH:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, depth too small");
		err = -EINVAL;
		break;
	case PCIE_CR_RING_REQ_FAIL_RING_ID_IN_USE:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, ring id mismatch - slot in use");
		err = -EFAULT;
		break;
	case PCIE_CR_RING_REQ_FAIL_RING_SHUTDOWN:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, shutdown in progress");
		err = -ESHUTDOWN;
		break;
	case PCIE_CR_RING_REQ_FAIL_RING_OPENING:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, open request while still openining");
		err = -EALREADY;
		break;
	case PCIE_CR_RING_REQ_FAIL_TRANSPORT_NOT_READY:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, the transport is not ready");
		err = -ENOTCONN;
		break;
	default:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware nacked create queue request, unknown error");
		err = -EIO;
		break;
	}

	return err;
}

static void
ar_pci_create_data_ring_cmd_reply(ar_pci_cmd_reply_promise_t *promise,
				  pcie_ctrl_resp_t *cmd_reply)
{
	int err = 0;
	ar_client_queue_t *client_queue;
	ar_future_t *future;

	AR_ASSERT(promise);
	AR_ASSERT(cmd_reply);

	future = promise->base.future;
	client_queue = promise->queue;
	AR_ASSERT(client_queue);
	AR_ASSERT(client_queue->driver);

	if (cmd_reply->create.common_header.msg_type != CREATE_RING_RSP) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_CREATE_QUEUE,
			"Firmware returned bad response type [msg type: %u]",
			cmd_reply->create.common_header.msg_type);
		err = -EIO;
		goto error;
	}

	if (cmd_reply->create.common_header.flag != 0) {
		err = ar_pci_create_data_ring_process_nack(
			client_queue, cmd_reply->create.common_header.flag);
		goto error;
	}

	// Reserve slots for read queues if the upper layer hasn't already.
	if (client_queue->queue_params.queue_direction == AR_QUEUE_FW_TO_HLOS &&
	    client_queue->queue_params.initial_read_index < 0)
		ar_client_rcv_ring_update_read_ptr(client_queue->arfw_queue,
						   client_queue);

	client_queue->seq_num = 0;

	ar_client_ring_add_active(
		client_queue->queue_params.queue_direction ==
				AR_QUEUE_HLOS_TO_FW ?
			      &client_queue->driver->client_send_queues :
			      &client_queue->driver->client_rcv_queues,
		client_queue);

	kfree(cmd_reply);
	ar_future_complete_data(future, client_queue);
	return;

error:
	kfree(cmd_reply);
	ar_future_complete(future, err);
}

static void ar_pci_create_data_ring_complete(int code, void *data,
					     void *context)
{
	ar_pci_cmd_reply_promise_t *promise =
		(ar_pci_cmd_reply_promise_t *)context;
	ar_client_queue_t *client_queue;

	AR_ASSERT(promise);
	client_queue = promise->queue;
	AR_ASSERT(client_queue);
	AR_ASSERT(client_queue->driver);

	if (code) {
		ar_pci_release_data_ring_slot(client_queue->driver,
					      client_queue->data_ring);
		devm_kfree(&client_queue->driver->dev->dev, client_queue);
	}
}

static ar_pci_cmd_reply_promise_t *create_cmd_reply_promise(
	uint16_t seq_num, ar_future_t *future,
	void (*complete)(ar_pci_cmd_reply_promise_t *p, pcie_ctrl_resp_t *r))
{
	ar_pci_cmd_reply_promise_t *promise;

	AR_ASSERT(future);
	AR_ASSERT(future->dev);

	promise = devm_kzalloc(future->dev, sizeof(ar_pci_cmd_reply_promise_t),
			       GFP_KERNEL);
	if (!promise)
		return NULL;

	promise->seq_num = seq_num;
	promise->complete = complete;
	ar_future_set_promise(future, &promise->base);

	return promise;
}

ar_future_t *ar_pci_create_data_ring(
	struct pci_dev *dev,
	const struct arfw_client_queue_create_params *queue_params,
	arfw_client_queue_t arfw_queue,
	struct arfw_queue_mem_region *queue_data, unsigned long timeout_ms)
{
	int ret;
	unsigned long flags;
	uint8_t headroom;
	ar_pci_bar_data_ring_t *ring_entry = NULL;
	uint32_t ring_index, ring_id;
	uint16_t seq_num;
	ar_future_t *future;
	ar_pci_cmd_reply_promise_t *reply_promise;
	ar_client_queue_t *client_queue;
	pcie_ctrl_req_t packet;
	pcie_create_ring_req_t *create = &packet.create;
	ar_pci_driver_t *driver;
	ar_pci_queue_data_t *data_context = queue_data->context;

	AR_ASSERT(dev);
	driver = pci_get_drvdata(dev);
	AR_ASSERT(data_context);

	/*
	 * The headroom is the portion used by the ArFirmware class interface, not
	 * transferred by firmware
	 */
	headroom = CLIENT_RING_HEADER_SIZE;

	if (queue_params->queue_direction == AR_QUEUE_HLOS_TO_FW) {
		ret = ar_pci_allocate_send_data_ring_slot(dev, &ring_entry,
							  &ring_index);
	} else {
		ret = ar_pci_allocate_rcv_data_ring_slot(dev, &ring_entry,
							 &ring_index);
	}

	if (ret) {
		AR_LOG_PCI_QUEUE_PARAMS_ERR(
			&dev->dev, queue_params, AR_LOG_CREATE_QUEUE,
			"Failed to allocate data ring [err: %d]", ret);
		goto error_slot;
	}

	if (queue_params->queue_direction == AR_QUEUE_HLOS_TO_FW)
		arfw_mem_util_iomem_write_16(driver->ap_update_block_addr,
					     ring_entry->wr_index_reg, 0);
	else
		arfw_mem_util_iomem_write_16(
			driver->ap_update_block_addr, ring_entry->rd_index_reg,
			(queue_params->queue_direction == AR_QUEUE_FW_TO_HLOS &&
			 queue_params->initial_read_index >= 0) ?
				      queue_params->initial_read_index :
				      0);

	ring_entry->cached_ring_index = 0;
	*ring_entry->cached_ap_index = 0;

	ring_entry->create_ring_id.ring_type =
		queue_params->queue_direction == AR_QUEUE_HLOS_TO_FW ?
			      AP_SRC_DATA_RING :
			      AP_DST_DATA_RING;
	ring_entry->create_ring_id.ring_cnt = (uint8_t)ring_index;

	seq_num = ar_pci_bar_next_cmd_sequence_number(dev);

	future = ar_create_future(&dev->dev);
	if (future == NULL) {
		AR_LOG_PCI_QUEUE_PARAMS_ERR(&dev->dev, queue_params,
					    AR_LOG_CREATE_QUEUE,
					    "Failed to create future");
		ret = -ENOMEM;
		goto error_future;
	}

	reply_promise = create_cmd_reply_promise(
		seq_num, future, ar_pci_create_data_ring_cmd_reply);
	if (reply_promise == NULL) {
		AR_LOG_PCI_QUEUE_PARAMS_ERR(&dev->dev, queue_params,
					    AR_LOG_CREATE_QUEUE,
					    "Failed to create promise");
		ret = -ENOMEM;
		goto error_promise;
	}

	client_queue = ar_client_queue_create(driver, arfw_queue, queue_params,
					      ring_entry, seq_num);
	if (client_queue == NULL) {
		AR_LOG_PCI_QUEUE_PARAMS_ERR(&dev->dev, queue_params,
					    AR_LOG_CREATE_QUEUE,
					    "Failed to create client queue");
		ret = -ENOMEM;
		goto error_queue;
	}

	reply_promise->base.lock = &driver->cmd_reply_lock;
	reply_promise->queue = client_queue;

	ar_future_set_timeout(future, timeout_ms);
	ar_future_set_on_complete(future, ar_pci_create_data_ring_complete,
				  reply_promise);

	spin_lock_irqsave(reply_promise->base.lock, flags);
	list_add_tail(&reply_promise->base.list, &driver->cmd_reply_list);
	spin_unlock_irqrestore(reply_promise->base.lock, flags);

	create->common_header.msg_type = CREATE_RING_REQ;
	create->common_header.seq_num = seq_num;
	create->priority = 0;
	create->ring_id = ring_entry->create_ring_id;
	create->ring_size = queue_params->depth;
	create->item_size = queue_params->element_size - headroom;
	create->head_room = headroom;
	create->tail_room = 0;
	create->ap_ep_id = queue_params->hlos_endpoint_id;
	create->arp_ep_id = queue_params->fw_endpoint_id;
	create->ring_addr = data_context->dma_addr;

	ring_id = ring_entry->create_ring_id.id;

	AR_LOG_PCI_QUEUE_PARAMS_DBG(
		&dev->dev, queue_params, AR_LOG_WRITE_CTRL,
		"Create Ring Request [queue: %p, type: %s]", arfw_queue,
		ring_type_name(packet.create.ring_id.ring_type));

	AR_LOG_PCI_QUEUE_PARAMS_DBG(
		&dev->dev, queue_params, AR_LOG_WRITE_CTRL,
		"- [ring_idx: %u, ring_type: %u, ring_size: %u]",
		packet.create.ring_id.ring_cnt, packet.create.ring_id.ring_type,
		queue_params->depth);

	AR_LOG_PCI_QUEUE_PARAMS_DBG(
		&dev->dev, queue_params, AR_LOG_WRITE_CTRL,
		"- [item_size: %u, head_room: %u, ,tail_room: %u]",
		packet.create.item_size, packet.create.head_room,
		packet.create.tail_room);

	AR_LOG_PCI_QUEUE_PARAMS_DBG(
		&dev->dev, queue_params, AR_LOG_WRITE_CTRL,
		"- [AP ID: 0x%x, FW ID: %u, dma_addr: %p, seq_nr: %u]",
		packet.create.ap_ep_id, packet.create.arp_ep_id,
		(void *)create->ring_addr, seq_num);

	ret = ar_pci_bar_submit_control_packet(
		dev, &packet,
		!atomic_read_acquire(&driver->duty_cycle.enabled));
	if (ret) {
		AR_LOG_PCI_QUEUE_PARAMS_ERR(
			&dev->dev, queue_params, AR_LOG_CREATE_QUEUE,
			"Failed to send create ring request [err: %d]", ret);
		goto error_submit;
	}

	AR_LOG_PCI_QUEUE_PARAMS_DBG(&dev->dev, queue_params,
				    AR_LOG_CREATE_QUEUE,
				    "Queue create request [id: %u]", ring_id);
	return future;

error_submit:
	spin_lock_irqsave(reply_promise->base.lock, flags);
	list_del(&reply_promise->base.list);
	spin_unlock_irqrestore(reply_promise->base.lock, flags);
	devm_kfree(&driver->dev->dev, client_queue);
error_queue:
	devm_kfree(&dev->dev, reply_promise);
error_promise:
	devm_kfree(&dev->dev, future);
error_future:
	ar_pci_release_data_ring_slot(driver, ring_entry);
error_slot:
	return ERR_PTR(ret);
}

static int
ar_pci_destroy_data_ring_process_nack(ar_client_queue_t *client_queue,
				      uint8_t flag)
{
	int err;

	AR_ASSERT(client_queue);

	switch (flag) {
	case PCIE_DEL_RING_REQ_FAIL_ACTIVE:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_DESTROY_QUEUE,
			"Firmware nacked destroy queue request, ring is still active");
		err = -EBUSY;
		break;
	case PCIE_DEL_RING_REQ_FAIL_INVALID_RING_ID:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_DESTROY_QUEUE,
			"Firmware nacked destroy queue request, ring does not exist");
		err = -EINVAL;
		break;
	case PCIE_DEL_RING_REQ_FAIL:
		/* fallthrough */
	default:
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_DESTROY_QUEUE,
			"Firmware nacked destroy queue request, unknown error");
		err = -EIO;
		break;
	}

	return err;
}

static void
ar_pci_destroy_data_ring_cmd_reply(ar_pci_cmd_reply_promise_t *promise,
				   pcie_ctrl_resp_t *cmd_reply)
{
	int err = 0;
	ar_client_queue_t *client_queue;
	ar_future_t *future;

	AR_ASSERT(promise);
	AR_ASSERT(cmd_reply);

	client_queue = promise->queue;
	future = promise->base.future;

	if (cmd_reply->del_ring.common_header.msg_type != DELETE_RING_RSP) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_DESTROY_QUEUE,
			"Firmware returned bad response type [msg type: %u]",
			cmd_reply->del_ring.common_header.msg_type);
		err = -EIO;
	}

	if (cmd_reply->del_ring.common_header.flag != 0) {
		err = ar_pci_destroy_data_ring_process_nack(
			client_queue, cmd_reply->del_ring.common_header.flag);
	}

	kfree(cmd_reply);
	ar_future_complete(future, err);
}

static void ar_pci_destroy_data_ring_drain(ar_client_queue_t *client_queue)
{
	uint16_t wr_index;

	AR_ASSERT(client_queue);

	ar_client_rcv_ring_update_read_ptr(client_queue->arfw_queue,
					   client_queue);
	wr_index = arfw_mem_util_iomem_read_16(
		client_queue->driver->cp_update_block_addr,
		client_queue->data_ring->wr_index_reg);
	arfw_mem_util_iomem_write_16(client_queue->driver->ap_update_block_addr,
				     client_queue->data_ring->rd_index_reg,
				     wr_index);
}

static void ar_pci_destroy_data_ring_clean_up(ar_client_queue_t *client_queue,
					      int code)
{
	AR_ASSERT(client_queue);
	AR_ASSERT(client_queue->driver);
	AR_ASSERT(client_queue->driver->dev);

	ar_client_ring_remove_active(client_queue);
	ar_client_ring_remove_replies(client_queue->driver, client_queue);

	/* Especially on the emulated environment there is a possibility
	 * that the promise hits a timeout error. In this case it
	 * is dangerous to release the slot. If the slot is released, then
	 * it could be reused for new queue, which leads to different race
	 * conditions on the control queue. Depending on the race the FW
	 * endpoint could crash or stop responsing.
	 * To mitigate it, we leak the data ring slot.
	 */

	if (!code)
		ar_pci_release_data_ring_slot(client_queue->driver,
					      client_queue->data_ring);
	else
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_DESTROY_QUEUE,
			"Data ring destroy completion error = %d, create_ring_id = 0x%x",
			code, client_queue->data_ring->create_ring_id.id);

	devm_kfree(&client_queue->driver->dev->dev, client_queue);
}

static void ar_pci_destroy_data_ring_complete(int code, void *data,
					      void *context)
{
	ar_pci_cmd_reply_promise_t *promise =
		(ar_pci_cmd_reply_promise_t *)context;
	ar_client_queue_t *client_queue;

	AR_ASSERT(promise);
	client_queue = promise->queue;

	ar_pci_destroy_data_ring_clean_up(client_queue, code);
}

ar_future_t *ar_pci_destroy_data_ring(struct pci_dev *dev,
				      ar_client_queue_t *client_queue,
				      unsigned long timeout_ms)
{
	int err;
	unsigned long flags;
	uint16_t seq_num;
	ar_future_t *future;
	ar_pci_cmd_reply_promise_t *reply_promise;
	pcie_ctrl_req_t packet;
	ar_pci_driver_t *driver;

	AR_ASSERT(dev);
	AR_ASSERT(client_queue);
	driver = pci_get_drvdata(dev);

	if (!driver->link_is_up) {
		// We do not have a stable link so there is no way
		// for us to communicate with the firmware. Let's clean up locally
		// and return fast, no need to try sending messages there.
		ar_pci_destroy_data_ring_clean_up(client_queue, 0);
		err = -ENOLINK;
		goto error_link;
	}

	// Drain the queue
	if (client_queue->queue_params.queue_direction == AR_QUEUE_FW_TO_HLOS)
		ar_pci_destroy_data_ring_drain(client_queue);

	seq_num = ar_pci_bar_next_cmd_sequence_number(dev);

	future = ar_create_future(&dev->dev);
	if (future == NULL) {
		AR_LOG_PCI_QUEUE_ERR(client_queue, AR_LOG_DESTROY_QUEUE,
				     "Failed to create future");
		err = -ENOMEM;
		goto error_future;
	}

	reply_promise = create_cmd_reply_promise(
		seq_num, future, ar_pci_destroy_data_ring_cmd_reply);
	if (reply_promise == NULL) {
		AR_LOG_PCI_QUEUE_ERR(client_queue, AR_LOG_DESTROY_QUEUE,
				     "Failed to create reply promise");
		err = -ENOMEM;
		goto error_promise;
	}

	reply_promise->base.lock = &driver->cmd_reply_lock;
	reply_promise->queue = client_queue;

	ar_future_set_timeout(future, timeout_ms);
	ar_future_set_on_complete(future, ar_pci_destroy_data_ring_complete,
				  reply_promise);

	spin_lock_irqsave(reply_promise->base.lock, flags);
	list_add_tail(&reply_promise->base.list, &driver->cmd_reply_list);
	spin_unlock_irqrestore(reply_promise->base.lock, flags);

	packet.del_ring.common_header.msg_type = DELETE_RING_REQ;
	packet.del_ring.common_header.seq_num = seq_num;
	packet.del_ring.ring_id = client_queue->data_ring->create_ring_id;

	// We need to exclude this queue from processing because
	// firmware could reset the metadata and zero-out the indexes.
	// That might race with AP and we could end up reading garbage on data rings.
	client_queue->released = true;

	err = ar_pci_bar_submit_control_packet(
		dev, &packet,
		!atomic_read_acquire(&driver->duty_cycle.enabled));
	if (err) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_DESTROY_QUEUE,
			"Failed to submit command packet [err: %d]", err);
		goto error_submit;
	}

	return future;

error_submit:
	spin_lock_irqsave(reply_promise->base.lock, flags);
	list_del(&reply_promise->base.list);
	spin_unlock_irqrestore(reply_promise->base.lock, flags);
	devm_kfree(&dev->dev, reply_promise);
error_promise:
	devm_kfree(&dev->dev, future);
error_future:
error_link:
	return ERR_PTR(err);
}

static void
ar_pci_destroy_all_data_rings_cmd_reply(ar_pci_cmd_reply_promise_t *promise,
					pcie_ctrl_resp_t *cmd_reply)
{
	int err = 0;
	ar_future_t *future;
	struct pci_dev *dev;

	AR_ASSERT(promise);
	AR_ASSERT(cmd_reply);
	AR_ASSERT(promise->dev);

	dev = promise->dev;
	future = promise->base.future;

	AR_ASSERT(future);

	if (cmd_reply->delete_all_rings_resp.common_header.msg_type !=
	    DELETE_ALL_RINGS_RSP) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_DESTROY_QUEUE,
			"Firmware returned bad response type [msg type: %u]",
			cmd_reply->delete_all_rings_resp.common_header.msg_type);
		err = -EIO;
	}

	kfree(cmd_reply);
	ar_future_complete(future, err);
}

static void ar_pci_destroy_all_data_rings_complete(int code, void *data,
						   void *context)
{
	ar_pci_cmd_reply_promise_t *promise =
		(ar_pci_cmd_reply_promise_t *)context;
	ar_pci_driver_t *driver;
	ar_client_queue_t *client_queue, *temp;
	ar_pci_client_list_t *send_queues;
	ar_pci_client_list_t *rcv_queues;

	(void)code;
	AR_ASSERT(promise);
	AR_ASSERT(promise->dev);
	driver = pci_get_drvdata(promise->dev);
	AR_ASSERT(driver);

	send_queues = &driver->client_send_queues;
	rcv_queues = &driver->client_rcv_queues;

	/* Unlike the destroy single queue, we always release the data ring slot.
	 * This queue will not be reused anyway, so we can avoid the race condition.
	 * FW guarantees that the queues are deleted.
	 */
	list_for_each_entry_safe(client_queue, temp, &send_queues->list_node,
				 node)
		ar_pci_destroy_data_ring_clean_up(client_queue, 0);

	list_for_each_entry_safe(client_queue, temp, &rcv_queues->list_node,
				 node)
		ar_pci_destroy_data_ring_clean_up(client_queue, 0);
}

ar_future_t *ar_pci_destroy_all_data_rings(struct pci_dev *dev,
					   unsigned long timeout_ms)
{
	int err;
	unsigned long flags;
	uint16_t seq_num;
	ar_future_t *future;
	ar_pci_cmd_reply_promise_t *reply_promise;
	pcie_ctrl_req_t packet;
	ar_pci_driver_t *driver;
	ar_pci_client_list_t *send_queues;
	ar_pci_client_list_t *rcv_queues;
	ar_client_queue_t *client_queue, *temp;

	AR_ASSERT(dev);

	driver = pci_get_drvdata(dev);
	send_queues = &driver->client_send_queues;
	rcv_queues = &driver->client_rcv_queues;

	// We do not have a stable link so there is no way
	// for us to communicate with the firmware. Let's clean up locally
	// and return fast, no need to try sending messages there.
	if (!driver->link_is_up) {
		// Clean up all send queues. Pass 0 (success) to release
		// data ring slots — the link is down so firmware cannot be
		// using them and there is no control queue race to guard
		// against. This matches ar_pci_destroy_data_ring() link-down
		// handling which also passes 0.
		list_for_each_entry_safe(client_queue, temp,
					 &send_queues->list_node, node) {
			ar_pci_destroy_data_ring_clean_up(client_queue, 0);
		}
		// Clean up all receive queues
		list_for_each_entry_safe(client_queue, temp,
					 &rcv_queues->list_node, node) {
			ar_pci_destroy_data_ring_clean_up(client_queue, 0);
		}
		err = -ENOLINK;
		goto error_link;
	}

	// Drain the receive queues
	list_for_each_entry_safe(client_queue, temp, &rcv_queues->list_node,
				 node) {
		ar_pci_destroy_data_ring_drain(client_queue);
	}

	seq_num = ar_pci_bar_next_cmd_sequence_number(dev);

	future = ar_create_future(&dev->dev);
	if (future == NULL) {
		AR_LOG_PCI_DEV_ERR(
			&dev->dev, AR_LOG_DESTROY_QUEUE,
			"Failed to create future to destroy all queues");
		err = -ENOMEM;
		goto error_future;
	}

	reply_promise = create_cmd_reply_promise(
		seq_num, future, ar_pci_destroy_all_data_rings_cmd_reply);
	if (reply_promise == NULL) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_DESTROY_QUEUE,
				   "Failed to reply promise");
		err = -ENOMEM;
		goto error_promise;
	}

	reply_promise->base.lock = &driver->cmd_reply_lock;
	reply_promise->dev = dev;

	ar_future_set_timeout(future, timeout_ms);
	ar_future_set_on_complete(
		future, ar_pci_destroy_all_data_rings_complete, reply_promise);

	spin_lock_irqsave(reply_promise->base.lock, flags);
	list_add_tail(&reply_promise->base.list, &driver->cmd_reply_list);
	spin_unlock_irqrestore(reply_promise->base.lock, flags);

	packet.delete_all_rings_req.common_header.msg_type =
		DELETE_ALL_RINGS_REQ;
	packet.delete_all_rings_req.common_header.seq_num = seq_num;

	// We need to exclude this queue from processing because
	// firmware could reset the metadata and zero-out the indexes.
	// That might race with AP and we could end up reading garbage on data rings.

	list_for_each_entry_safe(client_queue, temp, &send_queues->list_node,
				 node) {
		client_queue->released = true;
	}
	list_for_each_entry_safe(client_queue, temp, &rcv_queues->list_node,
				 node) {
		client_queue->released = true;
	}

	err = ar_pci_bar_submit_control_packet(
		dev, &packet,
		!atomic_read_acquire(&driver->duty_cycle.enabled));
	if (err) {
		AR_LOG_PCI_DEV_ERR(&dev->dev, AR_LOG_DESTROY_QUEUE,
				   "Failed to submit command for destroy all.");
		goto error_submit;
	}

	return future;

error_submit:
	spin_lock_irqsave(reply_promise->base.lock, flags);
	list_del(&reply_promise->base.list);
	spin_unlock_irqrestore(reply_promise->base.lock, flags);
	devm_kfree(&dev->dev, reply_promise);
error_promise:
	devm_kfree(&dev->dev, future);
error_future:
error_link:
	AR_LOG_PCI_DEV_ERR(
		&dev->dev, AR_LOG_DESTROY_QUEUE,
		"Failed to destroy all HW driver queues, cleanup locally %d",
		err);
	return ERR_PTR(err);
}

int ar_pci_submit_receive_buffer(struct pci_dev *dev,
				 ar_client_queue_t *client_queue,
				 const struct arfw_payload_pend_req *buffer)
{
	int ret;

	pcie_ap_src_buf_item_t src_buffer_packet = {
		.common_header.msg_type = AP_SRC_BUF_MSG,
		/*
		 * This flag is set for the backwards compatibility, to be sure that
		 * newer AP could communicate with the old (earlier than v5.0) FW.
		 * The newer FW just ignores this flag.
		 */
		.common_header.flag = AP_SRC_IPC_PYLD_SG_INLINE,
		.common_header.seq_num =
			ar_pci_bar_next_cmd_sequence_number(dev),
		.ipc_header.ap_ep_id =
			client_queue->queue_params.hlos_endpoint_id,
		.ipc_header.arp_ep_id =
			client_queue->queue_params.fw_endpoint_id,
		.ipc_header.msg_id = 0,
		.dst_buf_id = buffer->roundtrip.id,
		.total_buf_len =
			dma_buffer_len(buffer->sg_list, buffer->sg_list_len),
		.num_sg_elements = buffer->sg_list_len,
	};

	if (buffer->sg_list_len > client_queue->max_num_sg_entries)
		return -E2BIG;

	construct_pcie_sg_list(src_buffer_packet.sg_el, buffer->sg_list,
			       buffer->sg_list_len);

	AR_LOG_PCI_QUEUE_DBG(
		client_queue, AR_LOG_PEND,
		"Send pend buffer [roundtrip_id: 0x%x, len: 0x%x, num_sg_el: %d]",
		src_buffer_packet.dst_buf_id, src_buffer_packet.total_buf_len,
		src_buffer_packet.num_sg_elements);

	ret = ar_pci_bar_submit_ap_src_buffer(
		dev, &src_buffer_packet,
		!atomic_read_acquire(
			&client_queue->driver->duty_cycle.enabled));
	if (ret) {
		AR_LOG_PCI_QUEUE_ERR(
			client_queue, AR_LOG_PEND,
			"Failed to submit src buffer [queue: %p, err: %d]",
			client_queue->arfw_queue, ret);
	}

	return ret;
}
