/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file ar_fw_message.h
 *
 * @brief Definition of the message sent to/from firmware.
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_FW_MESSAGE_H
#define AR_FW_MESSAGE_H

#ifndef __KERNEL__
#include <stddef.h>
#include <stdint.h>

#else

#include <linux/types.h>

#endif

#include <ar_utils.h>
#include <linux/arfw_types.h>

/**
 * Enumeration to classify the location of data related to a specific
 * transfer within the ring.
 */
typedef enum {
  /// No data payload
  ARFW_BUFFER_LOC_NONE = 0,
  /// Data is inline, location queried by device information
  ARFW_BUFFER_LOC_IN_LINE = 1,
  /// Data is located in the an external buffer
  ARFW_BUFFER_LOC_EXTERNAL = 2,
  /// Data location should not be processed (treat as inline)
  ARFW_BUFFER_LOC_PASSTHROUGH = 3,
  /// Data is located in the aperture
  ARFW_BUFFER_LOC_APERTURE = 4,
} ar_firmware_message_data_location_t;

// helper to detect if data location exists outside the queue.
static inline bool data_location_external_to_queue(ar_firmware_message_data_location_t location) {
  return location == ARFW_BUFFER_LOC_EXTERNAL || location == ARFW_BUFFER_LOC_APERTURE;
}

// union type for combining pend id and region id into a roundtrip id
typedef union {
  uint32_t id;
  struct {
    uint16_t pend_id;
    uint16_t region_id;
  };
} roundtrip_id_t;

_Static_assert(sizeof(roundtrip_id_t) == sizeof(uint32_t), "size mismatch");

/**
 * This structure defines a scatter gather element to be used in the send FW queues.
 * This structure should align with the arfw_dma_sg_elem structure. Which later passed
 * to the pcie_ap_src_data_multi_buffer_list_item_t structure defined in the
 * pcie_ap_dst_multi_buf_element_t file.
 */
typedef struct __packed {
  roundtrip_id_t roundtrip;
  uint32_t offset;
  uint64_t size;
} ar_firmware_msg_sg_elem_t;

#define AR_FIRMWARE_MSG_SG_SEND_SIZE (sizeof(ar_firmware_msg_sg_elem_t))

/**
 * This structre defines a scatter gather element to be used in the FW recv queues.
 * See pcie_ap_dst_multi_buf_element_t definition in the pcie_ap_dst_multi_buf_element_t
 * for more details.
 */
typedef struct __packed {
  uint64_t size;
  roundtrip_id_t roundtrip;
  uint32_t reserved;
} ar_firmware_msg_sg_recv_t;

_Static_assert(
    sizeof(ar_firmware_msg_sg_recv_t) == sizeof(ar_firmware_msg_sg_elem_t),
    "Size mismatch");

/**
 * This structure is used at the front of each data ring entry (send and
 * receive) for ar firmware queues. The structure itself will not be transferred
 * as part of the protocol, but instead serves as communication between library
 * and driver implementing the interface.
 */
typedef struct __packed {
  /**
   * The message ID. This may be part of the session negotiation or specific to
   * the app, informing it of the expected payload.
   */
  ar_msg_id_t msg_id;
  /// Tracking ID, used to match replies to messages
  ar_tracking_id_t tracking_id;
  /// Sequence ID counter
  ar_sequence_id_t sequence_id;
  /**
   * Payload size being transferred. For ARFW_BUFFER_LOC_EXTERNAL, this will be
   * a multiple of sizeof(ar_firmware_msg_sg_elem_t).
   *
   * For mixed messages (inline + external|aperture), this will be the size of
   * all the data in the message, including the inline data and padding between
   * the inline and external data.
   */
  uint16_t data_size;
  /**
   * Inline data size. For ARFW_BUFFER_LOC_EXTERNAL, this will be 0. For inline
   * data, this should match data_size.
   * The value of this field is the size of the inline data, if inline data is present.
   */
  uint16_t inline_msg_len;
  /// Cast to ar_firmware_message_data_location_t
  uint8_t data_location;
  /// Userspace creation timestamp in us
  uint32_t user_creation_timestamp_us;
  // Kernel creation timestamp in us
  uint32_t kernel_creation_timestamp_us;
  /// Do not respect the duty cycle, no effect if disabled
  bool immediate;
} ar_firmware_message_header_t;

#define THRESHOLD_TIME 5000 // time threshold in us for logging warnings

#define AR_FIRMWARE_FAST_PEND_PRE_MAGIC 0xdeadbeef
#define AR_FIRMWARE_FAST_PEND_MAGIC 0x9090fa57

union ar_firmware_fast_pend_header {
  uint64_t raw;
  struct __packed {
    uint32_t magic;
    uint16_t region_id;
    uint16_t pend_id;
  };
};

static inline void* arfw_sg_list_of_msg(
    ar_firmware_message_header_t* msg,
    size_t inline_data_offset) {
  size_t inline_msg_len_aligned = AR_ROUNDUP(msg->inline_msg_len, sizeof(uint64_t));
  return (void*)((uintptr_t)msg + inline_data_offset + inline_msg_len_aligned);
}

static inline size_t arfw_sg_buf_count_of_msg(ar_firmware_message_header_t* msg) {
  size_t inline_msg_len_aligned = AR_ROUNDUP(msg->inline_msg_len, sizeof(uint64_t));
  return (msg->data_size - inline_msg_len_aligned) / sizeof(ar_firmware_msg_sg_elem_t);
}

#endif // AR_FW_MESSAGE_H
