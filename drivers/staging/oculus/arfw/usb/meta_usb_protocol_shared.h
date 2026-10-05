/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file meta_usb_protocol_shared.h
 *
 * @brief a shared header file between AP and CP for USB protocol bits
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef META_USB_PROTOCOL_SHARED_H
#define META_USB_PROTOCOL_SHARED_H

#pragma pack(push, 1)

#define META_USB_VENDOR_ID 0x2833
#define META_USB_PRODUCT_ID 0x0001

#define META_USB_AP_PROTOCOL_MAJOR_VERSION 0x0000
#define META_USB_AP_PROTOCOL_MINOR_VERSION 0x0001
#define META_USB_CP_PROTOCOL_MAJOR_VERSION 0x0000
#define META_USB_CP_PROTOCOL_MINOR_VERSION 0x0001

#define META_USB_FLAG_OK 0
#define META_USB_FLAG_NACK 1

#define META_USB_EP_CTRL_IN_OFFSET 0
#define META_USB_EP_CTRL_OUT_OFFSET 1

#define META_USB_RING_TYPES_MAX 2

typedef enum {
	META_USB_CREATE_RING_MSG_OUT = 0,
	META_USB_CREATE_RING_MSG_IN = 1,
	META_USB_DELETE_RING_MSG_OUT = 2,
	META_USB_DELETE_RING_MSG_IN = 3,
	META_USB_QUERY_INFO_MSG_OUT = 4,
	META_USB_QUERY_INFO_MSG_IN = 5,
	META_USB_SYNC_ROOM_MSG_OUT = 6,
	META_USB_SYNC_ROOM_MSG_IN = 7,
	META_USB_DATA_MSG_OUT = 8,
	META_USB_DATA_MSG_IN = 9,
	META_USB_CTRL_GEN_MSG_IN = 10, // Generic ACK/NACK reply for a ctrl msg.
	META_USB_INVALID_MSG_OUT = 0xFF // Invalid message type. Do not use.

} meta_usb_msg_type_t;

typedef enum {
	META_USB_DATA_RING_OUT = 0, // AP -> CP
	META_USB_DATA_RING_IN = 1, // CP -> AP

	/**
     * The control rings are created at initialization itself
     * by both CP and AP.
     **/
	META_USB_CTRL_RING_OUT = 0x81, // AP -> CP, AP_SRC_CTRL_RING
	META_USB_CTRL_RING_IN = 0x82 // CP -> AP, AP_DST_CTRL_RING
} meta_usb_ring_type_t;

typedef union {
	uint32_t as_uint32;
	struct {
		uint16_t minor;
		uint16_t major;
	};
} meta_usb_protocol_version_t;

typedef struct {
	union {
		uint16_t id;
		struct {
			uint8_t ring_slot;
			uint8_t ring_type;
		};
	};
} meta_usb_ring_id_t;

typedef struct {
	uint8_t msg_type;
	uint8_t flag;
	uint16_t seq_num;
} meta_usb_common_header_t;

typedef struct {
	uint16_t msg_id;
	uint16_t ap_ep_id;
	uint16_t cp_ep_id;
	uint8_t track_num;
	uint8_t seq_num;
} meta_usb_ipc_header_t;

typedef struct {
	meta_usb_common_header_t common_header;
	meta_usb_ring_id_t ring_id;
	uint8_t head_room;
	uint8_t tail_room;
	uint16_t ring_size;
	uint16_t item_size;
	uint16_t ap_ep_id;
	uint16_t cp_ep_id;
} meta_usb_create_ring_msg_out_t;

typedef struct {
	meta_usb_common_header_t common_header;
	// The USB Endpoint ID offset for this USB interface, for the inline messages
	uint8_t ep_inl;
	// The USB Endpoint ID offset for this USB interface, for the external buffers
	uint8_t ep_ext;
} meta_usb_create_ring_msg_in_t;

// meta_usb_create_ring_msg_in_t NACK flags, sent by CP
#define META_USB_CREATE_RING_FAIL_DUP 0x1
#define META_USB_CREATE_RING_FAIL_INVALID_RING_SLOT 0x2
#define META_USB_CREATE_RING_FAIL_INVALID_RING_TYPE 0x3
#define META_USB_CREATE_RING_FAIL_INVALID_ITEM_SIZE 0x4
#define META_USB_CREATE_RING_FAIL_INVALID_EP 0x5
#define META_USB_CREATE_RING_FAIL_INVALID_DEPTH 0x6
#define META_USB_CREATE_RING_FAIL_RING_ID_IN_USE 0x7
#define META_USB_CREATE_RING_FAIL_RING_SHUTDOWN 0x8
#define META_USB_CREATE_RING_FAIL_RING_OPENING 0x9
#define META_USB_CREATE_RING_FAIL_TRANSPORT_NOT_READY 0xA
#define META_USB_CREATE_RING_FAIL_INVALID_RING_SIZE 0x10

typedef struct {
	meta_usb_common_header_t common_header;
	meta_usb_ring_id_t ring_id;
} meta_usb_delete_ring_msg_out_t;

typedef struct {
	meta_usb_common_header_t common_header;
} meta_usb_delete_ring_msg_in_t;

// Flags for meta_usb_delete_ring_msg_in_t response sent by CP
#define META_USB_DELETE_RING_FAIL 0x1
#define META_USB_DELETE_RING_FAIL_ACTIVE 0x2
#define META_USB_DELETE_RING_FAIL_INVALID_RING_ID 0x3

typedef struct {
	meta_usb_common_header_t common_header;
	meta_usb_protocol_version_t ap_version;
} meta_usb_query_info_msg_out_t;

typedef struct {
	meta_usb_common_header_t common_header; // 4 bytes
	meta_usb_protocol_version_t cp_version;
	uint16_t cp_rings_max; // Max number of Janus sessions supported by CP
	uint16_t cp_inline_max; // Max inline message size supported by CP
	// Bitmask of inline endpoints supported by CP. This includes both IN and OUT EPs used for
	// inline messages.
	uint32_t cp_inline_eps;
	uint16_t cp_inline_sid; // StreamID for inline messages
	uint16_t cp_sid_max; // Max number of streamIDs supported by CP
} meta_usb_query_info_msg_in_t;

typedef struct {
	uint16_t slot;
	uint16_t room;
} meta_usb_sync_room_entry_t;

/**
 * The generic control message is used to send a generic control message from
 * CP to the AP. This is used to nack unknown requests.
 */
typedef struct {
	meta_usb_common_header_t common_header;
} meta_usb_ctrl_generic_msg_in_t;

typedef struct {
	meta_usb_common_header_t common_header;
	meta_usb_sync_room_entry_t entries[];
} meta_usb_sync_room_msg_out_t;

typedef struct {
	meta_usb_common_header_t common_header;
	meta_usb_sync_room_entry_t entries[];
} meta_usb_sync_room_msg_in_t;

typedef union {
	meta_usb_common_header_t common_header;
	meta_usb_create_ring_msg_out_t create_ring;
	meta_usb_delete_ring_msg_out_t delete_ring;
	meta_usb_query_info_msg_out_t query_info;
	meta_usb_sync_room_msg_out_t sync_room;
} meta_usb_ctrl_msg_out_t;

typedef union {
	meta_usb_common_header_t common_header;
	meta_usb_create_ring_msg_in_t create_ring;
	meta_usb_delete_ring_msg_in_t delete_ring;
	meta_usb_query_info_msg_in_t query_info;
	meta_usb_sync_room_msg_in_t sync_room;
	meta_usb_ctrl_generic_msg_in_t generic;
} meta_usb_ctrl_msg_in_t;

typedef struct {
	uint64_t sid;
	uint32_t size;
	uint16_t mem_id;
	uint16_t reserved;
} meta_usb_data_buf_t;

typedef struct {
	meta_usb_common_header_t common_header;
	meta_usb_ipc_header_t ipc_header;
	meta_usb_ring_id_t ring_id;
	uint16_t inline_size;
	uint8_t num_bufs;
	uint8_t data[];
} meta_usb_data_msg_t;

#pragma pack(pop)

#endif // !META_USB_PROTOCOL_SHARED_H
