/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file fb_pcie_drvr_shared.h
 *
 * @brief The shared header file between the AP and Avogadro
 *
 * @details
 *       The shared header file between the AP and
 *       Avogadro with common API for the messages exchanged between the AP and
 *       Avogadro. This file will also contain the shared memory area structures,
 *       interrupt definitions between the AP and Avogadro for the PCIe link.
 *       This assumes Little Endian Format for the headers.
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef FB_PCIE_DRVR_SHARED_H
#define FB_PCIE_DRVR_SHARED_H

#pragma pack(push, 1)

/// Macros and Constants

/// Current expected version of ARP protocol. See ProtocolHistory.txt
#define PCIE_CP_PROTOCOL_MAJOR_VERSION (0x5)
#define PCIE_CP_PROTOCOL_MINOR_VERSION (0x0)

/// Current version for the legacy BAR layout. See ProtocolHistory.txt
#define PCIE_CP_LEGACY_BAR_MAJOR_VERSION (0x4)
#define PCIE_CP_LEGACY_BAR_MINOR_VERSION (0x4)

/// Minimum support ARP protocol version
#define MIN_PCIE_ARP_PROTOCOL_MAJOR_VERSION (0x0001)

/// AP protocol version
#define PCIE_AP_PROTOCOL_MAJOR_VERSION (0x5)
#define PCIE_AP_PROTOCOL_MINOR_VERSION (0x3)

/// Current version for the legacy BAR layout. See ProtocolHistory.txt
#define PCIE_AP_LEGACY_BAR_MAJOR_VERSION (0x0)
#define PCIE_AP_LEGACY_BAR_MINOR_VERSION (0x3)

/// AP version for receiving mixed buffer messages from firmware
/// This still uses the legacy BAR layout
#define PCIE_AP_LEGACY_MIXED_BUFFER_MAJOR_VERSION (0x1)
#define PCIE_AP_LEGACY_MIXED_BUFFER_MINOR_VERSION (0x0)

#define PCIE_HANDSHAKE_MAGIC (0xA6ADA6AD)
#define PCIE_DEFERRED_MAGIC (0xb7bfb7b7)

/// Maximum elements in the ring
#define PCIE_AP_SRC_CTRL_RING_EL (32)
#define PCIE_AP_DST_CTRL_RING_EL (32)
#define PCIE_AP_SRC_BUF_RING_EL (64)

/// For ARP to AP-cache align elements in BAR memory.
#define PCIE_AP_CACHE_LINE_SIZE (64)

/// AP Memory Address Type
typedef uint64_t pcie_addr_t;

/// pcie_bar_register_area_t;
typedef volatile struct {
	/// AP write, ARP read/write
	uint32_t doorbell_interrupt;
	/// ARP write, Alfred read
	uint32_t doorbell_int_mask;
} pcie_bar_register_area_t;

#define PCIE_MAX_SG_EL_PER_MSG 8

#define PCIE_AP_SRC_BUF_MAX 1

/// pcie_ap_src_ring_stats_t
typedef struct {
	/**
	 * ARP write; AP read Total data packets on the
	 * Data transmit ring read by the ARP
	 */
	uint32_t total_count;
	/**
	 * ARP write; AP read Data packets on the Data
	 * transmit ring dropped by ARP due to error
	 */
	uint32_t error_count;
} pcie_ap_src_ring_stats_t;

/// pcie_ap_dst_ring_stats_t
typedef struct {
	/**
	 * ARP write; AP read Total messages sent on the
	 * Receive data ring by the ARP
	 */
	uint32_t total_count;
	/**
	 * ARP write; AP read Count of messages dropped
	 * by the ARP for the Receive data ring
	 */
	uint32_t drop_count;
} pcie_ap_dst_ring_stats_t;

/// STATUS BLOCK BEGINS
typedef struct {
	/// Permanent Ring Entries Updated by the AP

	/// Address in AP memory for the Control Tx ring
	pcie_addr_t ap_src_ctrl_addr;
	/// Address in AP memory for the Control Rx ring
	pcie_addr_t ap_dst_ctrl_addr;
	/// Address in AP memory for the AP src buffer ring
	pcie_addr_t ap_src_buf_addr;
	/// head_room stride needed by AP for each message of the static rings
	uint8_t ap_src_ctrl_head_room;
	uint8_t ap_dst_ctrl_head_room;
	uint8_t ap_src_buf_head_room;
	/// tail_room stride needed by AP for each message of the static rings
	uint8_t ap_src_ctrl_tail_room;
	uint8_t ap_dst_ctrl_tail_room;
	uint8_t ap_src_buf_tail_room;
	/// AP write: Write Index for the Control Tx ring
	uint16_t ap_src_ctrl_wr_index;
	/// AP write: Read Index for the Control Rx ring
	uint16_t ap_dst_ctrl_rd_index;
	/// AP write: Write Index for the src buffer ring
	uint16_t ap_src_buf_wr_index;
	/// align AP update fields to 64 byte boundary for cache alignment.
	uint8_t rsvd_ap[28];
} pcie_bar_status_block_area_ap_write_t;

typedef struct {
	/// Permanent Ring Entries, stats, status updated by the ARP

	/// ARP write: Read Index for the Control transmit ring
	uint16_t ap_src_ctrl_rd_index;
	/// ARP write: Write Index for the Control receive ring
	uint16_t ap_dst_ctrl_wr_index;
	/// ARP write: Read Index for the AP src buf ring
	uint16_t ap_src_buf_rd_index;
	/// ARP write: Status for the ap_dst_ctrl ring Eg: Full; error
	uint16_t ap_dst_ctrl_status;
	/// ARP write: Status for the ap_src_ctrl ring Eg: Full; error
	uint16_t ap_src_ctrl_status;
	/// ARP write: Status for the ap_src_buf ring Eg: Full; error
	uint16_t ap_src_buf_status;
	/// ARP write: Total messages sent on the control receive ring by the ARP
	uint32_t ap_dst_ctrl_total_count;
	/**
	 * ARP write;	 Count of messages dropped by the ARP for the Control receive
	 * ring
	 */
	uint32_t ap_dst_ctrl_drop_count;
	/// ARP write: Total messages on the control Tx ring received by the ARP
	uint32_t ap_src_ctrl_total_count;
	/// ARP write: Total messages on the AP src buf ring received by the ARP
	uint32_t ap_src_buf_total_count;
} pcie_bar_status_block_area_arp_write_t;

/// pcie_bar_status_block_area_t;
typedef struct {
	/// Fields updated by the AP
	pcie_bar_status_block_area_ap_write_t ap_updates;

	/// Fields updated by the ARP.
	pcie_bar_status_block_area_arp_write_t arp_updates;

} pcie_bar_status_block_area_t;

typedef union {
	uint32_t as_uint32;
	struct {
		uint16_t minor;
		uint16_t major;
	};
} pcie_protocol_version_t;

/**
 * Common header for all BAR layouts past and present.
 */
typedef struct {
	/**
	 * CP Write; AP read
	 * Initialization handshake to initiate communication between ARP and AP
	 */
	uint32_t initial_handshake_magic;

	/// CP Write; AP read. Protocol version of CP firmware
	pcie_protocol_version_t cp_protocol_version;

	/// AP write; CP read Protocol version of AP
	pcie_protocol_version_t ap_protocol_version;
} pcie_bar_header_t;

/**
 * Offset to fields updated by ARP for the data rings
 *
 * Each field is treated as array of type uint16_t, with
 * ap_src/dst_data_ring_max entries.
 */
typedef struct {
	/// Read index for the ap_src rings
	uint32_t ap_src_ring_rd_index_addr;
	/// Write index for the ap_dst rings
	uint32_t ap_dst_ring_wr_index_addr;
	/// Status for the ap_src rings
	uint32_t ap_src_ring_status_addr;
	/// Status for the ap_dst rings
	uint32_t ap_dst_ring_status_addr;
} pcie_ring_arp_update_addrs_t;

/**
 * Offset to fields updated by AP for the data rings
 *
 * Each field is treated as array of type uint16_t, with
 * ap_src/dst_data_ring_max entries.
 */
typedef struct {
	/// Read index for the ap_dst rings
	uint32_t ap_dst_ring_rd_index_addr;
	/// Write index for the ap_src rings
	uint32_t ap_src_ring_wr_index_addr;
} pcie_ring_ap_update_addrs_t;

/**
 * Offset to variable ring statistics updated by the ARP
 * Each should be viewed as an array of type pcie_ap_src/dst_ring_stats_t,
 * with ap_src/dst_data_ring_max entries.
 */
typedef struct {
	/// pcie_ap_src_ring_stats_t
	uint32_t ap_src_data_stats_addr;
	/// pcie_ap_dst_ring_stats_t
	uint32_t ap_dst_data_stats_addr;
} pcie_stats_ap_update_addrs_t;

/// pcie_bar_memory_area_t;
typedef struct {
	/**
	 * ARP Write; AP read
	 * Initialization handshake to initiate communication between ARP and AP
	 */
	uint32_t initial_handshake_magic;

	/// ARP Write; AP read. Protocol version of ARP firmware
	pcie_protocol_version_t arp_protocol_version;

	/// AP write; ARP read Protocol version of AP
	pcie_protocol_version_t ap_protocol_version;

	/**
	 * ARP Write; AP read Address of the status block in the PCIe shared memory
	 * BAR area.
	 * Offset from beginning of shared memory (BAR1) to status block.
	 */
	uint32_t status_block_addr;
	/**
	 * On a firmware assert; ARP firmware will update this with the address
	 * of the line that asserted
	 */
	uint32_t fw_assert_addr;
	/**
	 * Address of log buffer in the PCIe shared memory BAR area.
	 * Offset from beginning of shared memory (BAR1) to log buffer
	 * The ARP will write firmware logs to this area
	 */
	uint32_t log_buffer_addr;
	/**
	 * ARP write; AP read
	 * Size of the log buffer in the PCIe shared memory BAR area
	 */
	uint32_t log_buffer_size;
	/**
	 * ARP write; AP read
	 * Write pointer in the log buffer for valid ARP logs
	 */
	uint32_t log_buffer_wr_ptr;

	/// Begin ARP Protocol 3.0 fields

	/// ARP write; AP read. The TBD revision data of the firmware build
	uint64_t arp_build_revision;

	/// AP write; ARM read. The TBD revision of the AP OS build
	uint64_t ap_build_revision;

	/**
	 * ARP write; AP read. The maximum count available for data rings.
	 * AP source/AP dest are expected to match.
	 */
	uint16_t ap_src_data_ring_max;
	uint16_t ap_dst_data_ring_max;

	/**
	 * ARP Write; AP read Address of each of the ring read/write/status blocks.
	 * Offset from beginning of shared memory (BAR1) to status block.
	 */
	pcie_ring_arp_update_addrs_t arp_update_addrs;
	pcie_ring_ap_update_addrs_t ap_update_addrs;
	pcie_stats_ap_update_addrs_t arp_stats_addrs;

	/**
	 * ARP write; AP read. Offset of ARP's doorbell in BAR0
	 */
	uint32_t doorbell_offset;

#ifdef CONFIG_AR_PCI_POLLING
	/**
	 * No MSI or DB interrupts on FERM yet.
	 * Use counters in shared memory instead.
	 */
	uint32_t arp_intr_counter; // ARP updates counter to notify AP
	uint32_t ap_intr_ctr; // AP updates counter to notify ARP
#endif
} pcie_bar_memory_area_t;

/**
 * ap_data_ring_index_entry_t contains the AP updated read/write indices for data ring messages.
 */
typedef struct {
	/// AP write; CP read.
	uint16_t ap_src_data_wr_index;
	uint16_t ap_dst_data_rd_index;
} ap_data_ring_index_entry_t;

/**
 * cp_data_ring_index_entry_t contains the CP updated read/write indices for data ring messages.
 */
typedef struct {
	/// AP read; CP write.
	uint16_t ap_dst_data_wr_index;
	uint16_t ap_src_data_rd_index;
} cp_data_ring_index_entry_t;

/**
 * ap_update_block_t contains the AP updated read/write indices for control and data ring messages.
 */
typedef struct {
	/// AP write; CP read.
	/// Control ring indexes
	uint16_t ap_src_ctrl_wr_index;
	uint16_t ap_dst_ctrl_rd_index;
	uint16_t ap_src_buf_wr_index;

	/// Reserved for future use and alignment
	uint16_t reserved;

	/// Data ring memory
	ap_data_ring_index_entry_t data_ring[];
} ap_update_block_t;

typedef struct {
	/// AP read; CP write.
	/// Control ring indexes
	uint16_t ap_src_ctrl_rd_index;
	uint16_t ap_dst_ctrl_wr_index;
	uint16_t ap_src_buf_rd_index;

	/// Reserved for future use and alignment
	uint16_t reserved;

	/// Data ring memory
	cp_data_ring_index_entry_t data_ring[];
} cp_update_block_t;

/**
 * pcie_bar_data_t is the top level structure for the PCIe BAR data.
 * This will be the first structure in the data BAR.
 * This structure is to only be updated during initialization.
 */
typedef struct {
	pcie_bar_header_t header;

	/**
	 * AP write; CP read. Memory location of the update blocks in AP memory.
	 */
	pcie_addr_t ap_update_block_addr;
	pcie_addr_t cp_update_block_addr;

	/**
	 * AP write; CP read. Memory location of control rings in AP memory.
	 */
	pcie_addr_t ap_src_ctrl_addr;
	pcie_addr_t ap_dst_ctrl_addr;
	pcie_addr_t ap_src_buf_addr;

	/**
	 * CP write; AP read. The maximum count available for data rings.
	 * AP source/AP dest are expected to match.
	 */
	uint16_t data_ring_max;

	/// Reserved for future use and alignment
	uint16_t reserved;

	/**
	 * CP write; AP read. Offset of ARP's doorbell in BAR0
	 */
	uint32_t doorbell_offset;
} pcie_bar_data_t;

/// The msg_type field will be an enum. The different messages supported are:
typedef enum MESSAGE {
	CREATE_RING_REQ = 0,
	CREATE_RING_RSP = 1,
	/// Ping request from AP on pcie control ring
	CTRL_PING_REQ = 2,
	/// Ping response from ARP on pcie control ring
	CTRL_PING_RSP = 3,
	DISABLE_RING_REQ = 4,
	DISABLE_RING_RSP = 5,
	ENABLE_RING_REQ = 6,
	ENABLE_RING_RSP = 7,
	DELETE_RING_REQ = 8,
	DELETE_RING_RSP = 9,
	DEST_DATA_BUF_GET = 10, // 0xA
	AP_SRC_BUF_MSG = 11, // 0xB
	EVENT_MSG = 12, // 0xC
	AP_SRC_DATA_MSG = 13, // 0xD
	AP_DST_DATA_MSG = 14, // 0xE
	ARP_INFO_REQ = 15, // 0xF
	ARP_INFO_RSP = 16, // 0x10
	// 0x11  Request from AP to map an aperture buffer
	APERTURE_MAP_REQ = 17,
	// 0x12  Response from ARP for APERTURE_MAP_REQ
	APERTURE_MAP_RSP = 18,
	// 0x13  Request from AP to unmap an aperture buffer
	APERTURE_UNMAP_REQ = 19,
	// 0x14  Response from ARP for APERTURE_UNMAP_REQ
	APERTURE_UNMAP_RSP = 20,
	// 0x15  Request from AP to ARP to enter AR duty cycle
	AR_DUTY_CYCLE_REQ = 21,
	// 0x16  Response from ARP to AP for AR_DUTY_CYCLE_REQ
	AR_DUTY_CYCLE_RSP = 22,
	// 0x17  Generic response message for control ring
	GENERIC_CTRL_RSP = 23,
	// 0x18  Co-processor requests to enter D3 state for that PCIe function
	CP_D3_REQ = 24,
	// 0x19  AP requests to enter D3 state for that PCIe function
	AP_D3_REQ = 25,
	// 0x1A  Response from CP to AP for AP_D3_REQ
	CP_D3_RSP = 26,
	DELETE_ALL_RINGS_REQ = 27, // AP request to delete all rings
	DELETE_ALL_RINGS_RSP =
		28, //  Response from ARP to AP for DELETE_ALL_RINGS_REQ
	CP_WAKE_REQ =
		29, // 0x1D  CP Function0 requests to wake up n non-zero fns from D3hot state
	// Invalid message type. Do not use.
	MSG_TYPE_INVALID = 0xFF
} PCIE_MSG_TYPE_T;

static inline char *msg_type_name(PCIE_MSG_TYPE_T type)
{
	switch (type) {
	case CREATE_RING_REQ:
		return "CREATE_RING_REQ";
	case CREATE_RING_RSP:
		return "CREATE_RING_RSP";
	case CTRL_PING_REQ:
		return "CTRL_PING_REQ";
	case CTRL_PING_RSP:
		return "CTRL_PING_RSP";
	case DISABLE_RING_REQ:
		return "DISABLE_RING_REQ";
	case DISABLE_RING_RSP:
		return "DISABLE_RING_RSP";
	case ENABLE_RING_REQ:
		return "ENABLE_RING_REQ";
	case ENABLE_RING_RSP:
		return "ENABLE_RING_RSP";
	case DELETE_RING_REQ:
		return "DELETE_RING_REQ";
	case DELETE_RING_RSP:
		return "DELETE_RING_RSP";
	case DEST_DATA_BUF_GET:
		return "DEST_DATA_BUF_GET";
	case AP_SRC_BUF_MSG:
		return "AP_SRC_BUF_MSG";
	case EVENT_MSG:
		return "EVENT_MSG";
	case AP_SRC_DATA_MSG:
		return "AP_SRC_DATA_MSG";
	case AP_DST_DATA_MSG:
		return "AP_DST_DATA_MSG";
	case ARP_INFO_REQ:
		return "ARP_INFO_REQ";
	case ARP_INFO_RSP:
		return "ARP_INFO_RSP";
	case APERTURE_MAP_REQ:
		return "APERTURE_MAP_REQ";
	case APERTURE_MAP_RSP:
		return "APERTURE_MAP_RSP";
	case APERTURE_UNMAP_REQ:
		return "APERTURE_UNMAP_REQ";
	case APERTURE_UNMAP_RSP:
		return "APERTURE_UNMAP_RSP";
	case AR_DUTY_CYCLE_REQ:
		return "AR_DUTY_CYCLE_REQ";
	case AR_DUTY_CYCLE_RSP:
		return "AR_DUTY_CYCLE_RSP";
	case GENERIC_CTRL_RSP:
		return "GENERIC_CTRL_RSP";
	case CP_D3_REQ:
		return "CP_D3_REQ";
	case AP_D3_REQ:
		return "AP_D3_REQ";
	case CP_D3_RSP:
		return "CP_D3_RSP";
	case MSG_TYPE_INVALID:
		return "MSG_TYPE_INVALID";
	case DELETE_ALL_RINGS_REQ:
		return "DELETE_ALL_RINGS_REQ";
	case DELETE_ALL_RINGS_RSP:
		return "DELETE_ALL_RINGS_RSP";
	case CP_WAKE_REQ:
		return "CP_WAKE_REQ";
	}

	return "Unknown";
}

/// For all requests that are successful; pass it for the success response.
#define PCIE_REQ_SUCCESS 0x0

/**
 * A single or multiple IPC payload buffer pointers are inline, and are part of
 * the PCIe payload (indirect buffer(s) where the PCIe EP firmware will DMA the
 * indirect buffers after it has read and processed the PCIe payload).
 */
#define AP_SRC_IPC_PYLD_SG_INLINE (0x04)

/**
 * Multiple IPC payload buffers are present, and the PCIe payload has a pointer
 * to the list of the payload buffers in AP memory. (2 levels of indirection,
 * where the PCie EP firmware first has to DMA the list of indirect buffer
 * addresses from AP, and then DMA the indirect buffers)
 */
#define AP_SRC_IPC_PYLD_SG_PTR (0x01)
/// The entire IPC payload is inline within the PCIe Payload (direct buffer)
#define AP_SRC_IPC_PYLD_INLINE (0x02)

/// The IPC payload is a list of indirect buffers as part of the message.
#define AP_SRC_IPC_PYLD_LIST (0x08)

/// The IPC payload is a list of PCIe aperture buffers.
#define AP_SRC_IPC_APERTURE_LIST (0x10)

/// The IPC payload is a mixed buffer message.
#define AP_SRC_IPC_DATA_MSG (0x20)

#define AP_SRC_IPC_PYLD_MASK (0x3F)

/**
 * The PCIe Payload only has a destination buffer ID. This destination buffer ID
 * is sent by AP to Avo as part of the ap_src_buf_msg. Avo EP firmware will have
 * DMAd the IPC payload to the pre-allocated destination buffer whose ID is
 * dst_buf_id.
 */
#define AP_DST_IPC_PYLD_BUFID (0x01)
/// The entire IPC payload is inline within the PCIe Payload (direct buffer)
#define AP_DST_IPC_PYLD_INLINE (0x02)

/// The IPC payload is a list of indirect buffers as part of the message.
#define AP_DST_IPC_PYLD_LIST (0x04)

/// The IPC payload is a list of PCIe aperture buffers.
#define AP_DST_IPC_APERTURE_LIST (0x8)

/// The IPC payload is a mixed buffer message.
#define AP_DST_IPC_DATA_MSG (0x10)

#define AP_DST_IPC_PYLD_MASK (0x1F)

/// pcie_common_header_t
typedef struct {
	/// A unique ID for a message type; This is of type PCIE_MSG_TYPE_T
	uint8_t msg_type;
	/**
	 * Bit flags for the message (these can be overloaded for
	 * the different message types)
	 */
	uint8_t flag;
	/// Sequence number for the message
	uint16_t seq_num;
} pcie_common_header_t;

/// pcie_ipc_header_t
typedef struct {
	/// message ID
	uint16_t msg_id;
	/// AP's ARFW session Endpoint Identifier.
	uint16_t ap_ep_id;
	/// ARP's ARFW session Endpoint Identifier.
	uint16_t arp_ep_id;
	/// tracking number: used by IPC layer to associate replies
	uint8_t tracking_num;
	/// sequence number: maintained by IPC layer. unique per session
	uint8_t sequence_num;
} pcie_ipc_header_t;

/**
 * Ring types for rings that can be created dynamically via the
 * pcie_create_ring_req_t message exchange on the control ring.
 **/
typedef enum RING {
	/**
	 * The 1st set are dynamically created. 2 bits are allocated
	 * in pcie_create_ring_req_t for these as part of pcie_ring_id_t.
	 **/
	AP_SRC_DATA_RING = 0,
	AP_DST_DATA_RING = 1,

	/**
	 * NOTE: Add any new RING_TYPES to be created dynamically prior to
	 * AP_SRC_BUF_RING, which is the start of the static rings.
	 **/
	/**
	 * The 2nd set of ring types are created at initialization itself
	 * by both ARP and AP, and are needed prior to establishing communication.
	 * They are not part of Create ring req/response.
	 **/
	AP_SRC_BUF_RING = 0x80,
	AP_SRC_CTRL_RING = 0x81,
	AP_DST_CTRL_RING = 0x82
} PCIE_RING_TYPE_T;

static inline const char *ring_type_name(PCIE_RING_TYPE_T type)
{
	switch (type) {
	case AP_SRC_DATA_RING:
		return "AP_SRC_DATA_RING";
	case AP_DST_DATA_RING:
		return "AP_DST_DATA_RING";
	case AP_SRC_BUF_RING:
		return "AP_SRC_BUF_RING";
	case AP_SRC_CTRL_RING:
		return "AP_SRC_CTRL_RING";
	case AP_DST_CTRL_RING:
		return "AP_DST_CTRL_RING";
	}

	return "Unknown queue";
}

/**
 * Structure for the Scatter-gather elements. Each element of a scatter-gather
 * list sent by the AP will be of type pcie_sg_el_t.
 */
typedef struct {
	pcie_addr_t buf_addr;
	uint32_t len;
} pcie_sg_el_t;

/// pcie_ring_id_t param for pcie_create_ring_req_t
/// Supports a max of 2^6 = 64 rings for each ring type
#define FB_PCIE_RING_CNT_BITS 6
/// Supports max 2^2 = 4 types of rings that can be dynamically created.
/// Currently AP_SRC_DATA_RING = 0 and AP_DST_DATA_RING = 1 are the 2 types
/// used.
#define FB_PCIE_RING_TYPE_BITS 2
#define FB_PCIE_MAX_RINGS_PER_TYPE (1 << FB_PCIE_RING_CNT_BITS)

/**
 * pcie_ring_id_t is a bit field that specifies a PCIE_RING_TYPE_T in the
 * upper FB_PCIE_RING_TYPE_BITS (2) bits and a max count for each type
 * in the lower FB_PCIE_RING_CNT_BITS (6) bits.
 *
 **/
typedef struct {
	union {
		uint8_t id;
		struct {
			uint8_t ring_cnt : FB_PCIE_RING_CNT_BITS;
			uint8_t ring_type : FB_PCIE_RING_TYPE_BITS;
		};
	};
} pcie_ring_id_t;

/**
 * Create Ring Request
 * The AP sends the Create Ring Request message to the ARP on the ap_src_ctrl
 * Ring to request the ARP to create a ring. The rings created can be of type
 * ap_src_data or ap_dst_data. The ring_id field will identify the ring type,
 * and a unique ID for the ring type. The current possible ring types are
 * defined by PCIE_RING_TYPE_T
 */
typedef struct {
	pcie_common_header_t common_header;
	/**
	 * QoS priority for this data ring.
	 * We will initially support 4 priority levels( 0 to 4 with 0 being
	 * the highest and 4 the lowest priority). The priority will be used
	 * to determine the schedule to process the rings in the EP firmware.
	 */
	uint8_t priority;
	/**
	 * Unique Identifier for the ring to be created.
	 * The upper FB_PCIE_RING_TYPE_BITS bits will give the ring_type,
	 * PCIE_RING_TYPE_T, and the lower FB_PCIE_RING_CNT_BITS bits are for
	 * the ring count for a particular RING_TYPE, giving a unique ring_id.
	 * This allows us to create up to FB_PCIE_MAX_RINGS_PER_TYPE rings of each
	 * type.
	 */
	pcie_ring_id_t ring_id;
	/**
	 * Head room per message. This will be used by AP for any book-keeping
	 * per message. It can be 0 to 255 bytes. It is a stride or padding
	 * prior to each message in the ring. It will not be used/modified by ARP.
	 */
	uint8_t head_room;
	/**
	 * Tail room per message. This will be used by AP for any book-keeping
	 * per message. It can be 0 to 255 bytes. It is a stride or padding
	 * after each message in the ring. It will not be used/modified by ARP.
	 */
	uint8_t tail_room;
	/**
	 * Size of the ring.
	 * This is the maximum number of descriptors/messages in the ring.
	 */
	uint16_t ring_size;
	/// The size in bytes for each descriptor/message item of the ring.
	uint16_t item_size;
	/// Application Service Endpoint ID for this ring on AP side.
	uint16_t ap_ep_id;
	/// Application Service Endpoint ID for this ring on ARP side.
	uint16_t arp_ep_id;
	/**
	 * Address for the start of the ring in AP DRAM.
	 * NOTE: Each ring has to be in physically contiguous memory in APs memory.
	 */
	pcie_addr_t ring_addr;
} pcie_create_ring_req_t;

/// pcie_create_ring_resp_t flag param values to indicate that Create Ring
/// Request Failed.
/// Ring already created. Duplicate request.
#define PCIE_CR_RING_REQ_FAIL_DUP 0x1
/// Invalid ring count
#define PCIE_CR_RING_REQ_FAIL_INVALID_CNT 0x2
/// Invalid ring type
#define PCIE_CR_RING_REQ_FAIL_INVALID_RING_TYPE 0x3
/// Invalid inline message length
#define PCIE_CR_RING_REQ_FAIL_INVALID_ITEM_SIZE 0x4
/// Invalid endpoints
#define PCIE_CR_RING_REQ_FAIL_INVALID_EP 0x5
/// Invalid depth
#define PCIE_CR_RING_REQ_FAIL_INVALID_DEPTH 0x6
/// Ring ID in use
#define PCIE_CR_RING_REQ_FAIL_RING_ID_IN_USE 0x7
/// Ring is shtdown before opening
#define PCIE_CR_RING_REQ_FAIL_RING_SHUTDOWN 0x8
/// Ring got another open request while still opening
#define PCIE_CR_RING_REQ_FAIL_RING_OPENING 0x9
/// Transport for the session is not ready
#define PCIE_CR_RING_REQ_FAIL_TRANSPORT_NOT_READY 0xA

/// pcie_create_ring_resp_t
typedef struct {
	pcie_common_header_t common_header;
} pcie_create_ring_resp_t;

/**
 * pcie_ping_req_t
 * AP sends ping request to ARP on ap_src_ctrl_ring
 */
typedef struct {
	pcie_common_header_t common_header;
} pcie_ping_req_t;

/**
 * pcie_ping_resp_t
 * ARP sends ping response to AP on ap_dst_ctrl_ring, in response to
 * pcie_ping_req_t
 */
typedef struct {
	pcie_common_header_t common_header;
} pcie_ping_resp_t;

/**
 * pcie_delete_ring_req_t
 * AP sends request to ARP to delete the ring, specified by ring_id.
 */
typedef struct {
	pcie_common_header_t common_header;
	pcie_ring_id_t ring_id;
} pcie_delete_ring_req_t;

/// pcie_delete_ring_resp_t flag param failure statuses
#define PCIE_DEL_RING_REQ_FAIL 0x1
/**
 * Failed to delete ring, as it is still active. (read and write indices do not
 * match).
 */
#define PCIE_DEL_RING_REQ_FAIL_ACTIVE 0x2
/// Ring with ring_id is not found.
#define PCIE_DEL_RING_REQ_FAIL_INVALID_RING_ID 0x3

/**
 * pcie_delete_ring_resp_t
 * ARP sends delete ring response to AP on ap_dst_ctrl_ring. The status
 * of delete is sent in the flag param
 */
typedef struct {
	pcie_common_header_t common_header;
} pcie_delete_ring_resp_t;

/// pcie_disable_ring_req_t
typedef struct {
	pcie_common_header_t common_header;
	/**
	 * Unique Identifier for the ring to be disabled/enabled.
	 * The ring_id is created via the Create Ring Request message,
	 * and will not change when the ring is enabled/disabled
	 */
	pcie_ring_id_t ring_id;
} pcie_disable_ring_req_t;

/// pcie_disable_ring_resp_t
typedef struct {
	pcie_common_header_t common_header;
	/**
	 * Unique Identifier for the ring being disabled/enabled.
	 * The ring_id has to match that of the corresponding Disable Ring Request
	 * message
	 */
	pcie_ring_id_t ring_id;
	/**
	 * Status of the disable operation: success/ failure.
	 * If failure, set a failure cause.
	 */
	uint8_t status;
	/// Reserved, for 4 byte alignment.
	uint16_t rsvd;
	/**
	 * A destination buffer ID of the last buffer in the list of pre-allocated
	 * buffers being flushed. This field is only valid for disable operation on
	 * ap_dst_data_xx rings
	 */
	uint32_t dst_buf_id;
} pcie_disable_ring_resp_t;

/**
 * pcie_delete_all_rings_req_t
 * AP sends request to ARP to delete all rings in bulk.
 */
typedef struct {
	pcie_common_header_t common_header;
} pcie_delete_all_rings_req_t;

/**
 * pcie_delete_all_rings_resp_t
 * ARP sends delete all rings response to AP on ap_dst_ctrl_ring. The status
 * of delete is sent in the flag param.
 */
typedef struct {
	pcie_common_header_t common_header;
} pcie_delete_all_rings_resp_t;

/**
 * The pcie_dst_buf_get_msg_t message is sent from the ARP to AP if it has to
 * request a data buffer from the AP to be used as a destination buffer for a
 * given service's payload.
 * Most AP services pre-allocate and send destination buffers on the
 * AP_SRC_CTRL_RING. The ARP only needs to call pcie_dst_buf_get_msg_t for those
 * services that have not pre-sent the destination buffers.
 */
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Length of data buffer requested
	uint32_t len;
} pcie_dst_buf_get_msg_t;

/// ap_src_data Ring Descriptors

/**
 * Entry element for a multi buffer message.
 * The size of this element must be exactly 16 bytes.
 * This is the common type for both indirect buffers that will be
 * DMA'd by ARP, and for aperture buffers that will only be mapped by ARP.
 *
 * mem_id field is the buffer ID and used by AP to identify which send buffers
 * was consumed by ARP.
 */
typedef struct {
	pcie_addr_t addr;
	uint32_t size;
	uint16_t mem_id;
	uint16_t reserved;
} pcie_ap_multi_buf_element_t;

/**
 * The data item entry where entire IPC payload from AP is inline within the
 * PCIe Payload. Flag AP_SRC_IPC_PYLD_INLINE is set for this message in the
 * common_header.
 */
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Total length of the data IPC payload, not including headers
	uint32_t total_len;
	uint8_t data[];
} pcie_ap_src_data_inline_item_t;

/**
 * The data item entry where the AP transmits IPC Payload pointer(s) as part of
 * the PCIe Payload. Actual IPC payload will be DMA'd by ARP after processing
 * the message. Flag	AP_SRC_IPC_PYLD_SG_INLINE is set for this message.
 **/
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Total number of scatter-gather buffers for the entire IPC payload
	uint32_t num_pyld_bufs;
	/// Total length of the entire IPC payload.
	uint32_t total_ipc_pyld_len;
	/// Start of scatter-gather buffer elements
	pcie_sg_el_t buf_list[];
} pcie_ap_src_data_pyld_list_item_t;

/**
 * The data item entry where the AP transmits a list of buffers to ARP.
 * The flag AP_SRC_IPC_PYLD_LIST will be set in the common_header.
 **/
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	uint32_t num_bufs;
	pcie_ap_multi_buf_element_t buf_list[];
} pcie_ap_src_data_multi_buffer_list_item_t;

/**
 * Unified message type for inline, external, and multi buffer messages. Also
 * supports aperture buffers. The flag in the common_header will indicate the
 * type of message.
 **/
typedef struct pcie_data_msg_item {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Length of the inline segment.
	uint16_t inline_msg_len;
	/// Number of [external|aperture] buffers in the list
	uint8_t num_bufs;
	/// reserved space for future use. (and alignment)
	uint8_t reserved;
	// Inline data follows immediately after this structure.
	uint8_t inline_data[];
	/**
	 * An array of pcie_ap_[src|dst]_multi_buf_element_t items follows after the inline_data.
	 * The array starts after roundup(inline_msg_len,
	 * 																alignof(pcie_ap_[src|dst]_multi_buf_element_t)) bytes.
	 */
} pcie_data_msg_item_t;

/**
 * The data item entry where the AP transmits a list of aperture buffers to ARP.
 * The flag AP_SRC_IPC_APERTURE_LIST will be set in the common_header.
 **/
typedef struct pcie_ap_src_data_aperture_msg_item {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Number of Aperture buffers in the list
	uint32_t num_bufs;
	/// Start offset of the list of aperture buffers within the message.
	uint32_t aperture_start_offset;
	pcie_ap_multi_buf_element_t buf_list[];
} pcie_ap_src_data_aperture_msg_item_t;

/// Common header and IPC header shared by AP src and dst messages
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
} pcie_ipc_common_header_t;

/**
 * The data item entry for common_header flag AP_SRC_IPC_PYLD_SG_PTR.
 * NOTE: This type is not currently implemented, and will be a future
 * enhancement, if required.
 **/
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Length of scatter-gather list in bytes.
	uint32_t sg_list_len;
	/// AP address in contiguous AP memory where the scatter-gather list resides.
	pcie_addr_t sg_list_addr;
} pcie_ap_src_data_sg_list_item_t;

/// pcie_ap_src_data_item_t
typedef union {
	pcie_common_header_t common_header;
	pcie_ipc_common_header_t ipc_common_header;
	pcie_ap_src_data_inline_item_t pyld_inline;
	pcie_ap_src_data_pyld_list_item_t pyld_list;
	pcie_ap_src_data_sg_list_item_t pyld_sg_list;
	pcie_ap_src_data_multi_buffer_list_item_t pyld_multi_buf;
	pcie_ap_src_data_aperture_msg_item_t pyld_aperture;
	pcie_data_msg_item_t data_msg;
} pcie_ap_src_data_item_t;

/**
 * Entry element for an AP destinaion multi buffer message.
 * The size of this element must be exactly 16 bytes.
 */
typedef struct {
	uint64_t size;
	uint32_t id;
	// Unused padding to make this struct 16 bytes long.
	uint32_t _unused;
} pcie_ap_dst_multi_buf_element_t;

/**
 * pcie_ap_dst_data_item_t
 *
 * The data item entry where ARP transmits the destination buffer ID to AP.
 * This buffer ID represents a pre-allocated destination buffer in AP, where
 * ARP has DMA'd the packet to. Flag AP_DST_IPC_PYLD_BUFID is set in
 * common_header.
 **/
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Total length of IPC payload
	uint32_t total_len;
	/// Destion buffer ID
	uint32_t dst_buf_id;
} pcie_ap_dst_data_buf_item_t;

/**
 * The data item entry where ARP transmits the entire IPC payload to AP
 * in the message itself. Flag AP_DST_IPC_PYLD_INLINE is set in
 * common_header.
 **/
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Total length of IPC payload
	uint32_t total_len;
	/// Start of IPC payload
	uint8_t data[];
} pcie_ap_dst_data_inline_item_t;

/**
 * The data item entry where ARP transmits a list of destination buffers to AP.
 * The flag AP_DST_IPC_PYLD_LIST will be set in the common_header.
 **/
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	uint32_t num_bufs;
	pcie_ap_dst_multi_buf_element_t buf_list[];
} pcie_ap_dst_data_multi_buffer_list_item_t;

/**
 * The data item entry where the device transmits a list of aperture buffers to AP.
 * The flag AP_DST_IPC_APERTURE_LIST will be set in the common_header.
 **/
typedef struct pcie_ap_dst_data_aperture_msg_item {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/// Number of Aperture buffers in the list
	uint32_t num_bufs;
	/// Start offset of the list of aperture buffers within the message.
	uint32_t aperture_start_offset;
	pcie_ap_dst_multi_buf_element_t buf_list[];
} pcie_ap_dst_data_aperture_msg_item_t;

typedef union {
	pcie_common_header_t common_header;
	pcie_ipc_common_header_t ipc_common_header;
	pcie_ap_dst_data_inline_item_t pyld_inline;
	pcie_ap_dst_data_buf_item_t pyld_buf;
	pcie_ap_dst_data_multi_buffer_list_item_t pyld_multi_buf;
	pcie_ap_dst_data_aperture_msg_item_t pyld_aperture;
	pcie_data_msg_item_t data_msg;
} pcie_ap_dst_data_item_t;

/**
 * pcie_ap_src_buf_item_t
 * The pcie_ap_src_buf_item_t message is sent by AP on the AP_SRC_BUF_RING.
 * The msg_type is AP_SRC_BUF_MSG.
 * It contains the pre-allocated destination buffer addresses for the ARP to
 * use, or it contains the "on-demand" destination buffer sent by the AP in
 * response to the pcie_dst_buf_get_msg_t request message
 * by the ARP for a given AP service.
 * With input flag AP_SRC_IPC_PYLD_SG_INLINE: Msg will contain a list of AP
 * buffers inline for a given IPC payload. The max AP buffers is limited to
 * PCIE_MAX_SG_EL_PER_MSG. With input flag AP_SRC_IPC_PYLD_SG_PTR: Msg will
 * contain a pointer in AP to a scatter-gather list of buffers for a destination
 * IPC payload for the ARP to use. This flag will be used by AP if the S-G list
 * has more than PCIE_MAX_SG_EL_PER_MSG elements. The AP buffer containing the
 * SG list has to be in contiguous AP memory. Each destination buffer or the SG
 * list is specific for the (ipc_src_id, ipc_dstId) tuple of the message. Each
 * buffer also has a unique dest_buf_id that is opaque to ARP. The dest_buf_id
 * will be used if buffers have to be flushed, and is also part of
 * pcie_ap_dst_data_item_t msg when ARP fills and sends back this buffer to AP.
 */
typedef struct {
	pcie_common_header_t common_header;
	pcie_ipc_header_t ipc_header;
	/**
	 * The destination buffer ID. This will uniquely identify each
	 * pre-allocated destination buffer in the AP, and will be sent back
	 * to the AP by the ARP when buffers have to be flushed, or
	 * data is returned back to AP in this buffer.
	 */
	uint32_t dst_buf_id;
	/**
	 * Length in bytes of the IPC payload buffer that AP has pre-allocated.
	 * This can vary for the IPC src and dst EP IDs, and is usually the maximum
	 * IPC payload length for a given IPC session.
	 */
	uint32_t total_buf_len;
	/// The total number of scatter-gather elements for this buffer.
	uint32_t num_sg_elements;
	/**
	 * The address(es) of the buffer(s) in AP memory, if this is a INLINE SG list.
	 * The address to the scatter-gather list in AP memory for a SG pointer.
	 */
	pcie_sg_el_t sg_el[PCIE_MAX_SG_EL_PER_MSG];

	/**
	 * reserved for cache-line alignment. Each item in AP_SRC_BUF_RING is 128
	 * bytes.
	 */
	uint8_t reserved[8];
} pcie_ap_src_buf_item_t;

/**
 * The pcie_arp_info_req_t is sent from the AP to the ARP to query ARP
 * information that is shared accross sessions.
 * The ARP will return a pcie_arp_info_resp_t on a ctrl ring with the ARP
 * information.
 */
typedef struct {
	pcie_common_header_t common_header;
} pcie_arp_info_req_t;

/**
 * The pcie_arp_info_resp_t is the response to a pcie_arp_info_req_t.
 * This will be sent by the ARP after receiving a information request from the
 * AP.
 */
typedef struct {
	pcie_common_header_t common_header;
	uint16_t rcv_ring_pend_buff_count_max;
	uint8_t sys_name_len;
	char sys_name[12];
	/**
	 * Explicit padding to the max size of a control ring message.
	 * This will allow future expansion of this message while keeping
	 * a total size of 32 bytes.
	 */
	uint8_t reserved[13];
} pcie_arp_info_resp_t;

typedef struct pcie_aperture_map_req {
	pcie_common_header_t common_header;
	/**
   * The AP memory address that the AP wants to map the PCIe aperture to.
   **/
	pcie_addr_t buf_addr;
	/**
   * The length of the PCIe aperture.
   **/
	uint32_t len;
	/**
   * A unique ID for this PCIe aperture.
   **/
	uint32_t aperture_id;
} pcie_aperture_map_req_t;

typedef struct pcie_aperture_unmap_req {
	pcie_common_header_t common_header;
	/**
   * The unique ID of the PCIe aperture to unmap.
   **/
	uint32_t aperture_id;
} pcie_aperture_unmap_req_t;

/// pcie_aperture_map_resp_t flag param values to indicate that APERTURE_MAP_REQ
/// Failed.
/// Aperture with this aperture ID already exists. Duplicate request.
#define PCIE_APERTURE_MAP_REQ_FAIL_DUP 0x1
/// Aperture length invalid/ not supported.
#define PCIE_APERTURE_MAP_REQ_FAIL_INVALID_LEN 0x2
/// Failed for unknown reason.
#define PCIE_APERTURE_MAP_REQ_FAIL_UNKNOWN 0x3

typedef struct pcie_aperture_map_resp {
	pcie_common_header_t common_header;
	/**
   * A unique ID for this PCIe aperture.
   **/
	uint32_t aperture_id;
} pcie_aperture_map_resp_t;

/// pcie_aperture_unmap_resp_t flag param values to indicate that APERTURE_UNMAP_REQ
/// Failed.
/// Invalid aperture ID.
#define PCIE_APERTURE_UNMAP_FAIL_EINVAL 0x1

// PCIe aperture unmap response. The unmap operation is always successful.
typedef struct pcie_aperture_unmap_resp {
	pcie_common_header_t common_header;
	/**
   * The unique ID of the PCIe aperture unmapped.
   **/
	uint32_t aperture_id;
} pcie_aperture_unmap_resp_t;

#define AR_DUTY_CYCLE_CMD_DISABLE 0
#define AR_DUTY_CYCLE_CMD_ENABLE 1

typedef uint8_t AR_DUTY_CYCLE_CMD;

typedef struct {
	pcie_common_header_t common_header;

	// The frequency in millihertz at which the ARP should be enabled.
	uint64_t duty_cycle_freq_millihz;

	// desired duty cycle status
	AR_DUTY_CYCLE_CMD duty_cycle_cmd;
} ar_duty_cycle_req_t;

#define AR_DUTY_CYCLE_REQ_FAIL 0x1

typedef struct {
	pcie_common_header_t common_header;
	/**
     * Explicit padding to the max size of a control ring message.
     * This will allow future expansion of this message while keeping
     * a total size of 32 bytes.
     */
	uint8_t reserved[28];
} ar_duty_cycle_resp_t;

/**
 * The ar_cp_req_d3_t is a message from CP-> AP, requesting AP to place the
 * pcie function in D3 hot state. There is no ACK/response needed from AP for
 * this message. AP will start the process of placing the PCIe function in D3
 * when it receives this request. CP will get notification via D-state change
 * interrupt when the PCIe function is placed in D3.
 */
typedef struct {
	pcie_common_header_t common_header;
	/**
     * Explicit padding to the max size of a control ring message.
     * This will allow future expansion of this message while keeping
     * a total size of 32 bytes.
     */
	uint8_t reserved[28];
} ar_cp_req_d3_t;

#define AP_REQ_D3_REQ_FAIL_UNSUPPORTED 0x1

/**
 * The ar_cp_req_wake_t is a message from CP-> AP, requesting AP to place the
 * input pcie function in D0 state. There is no ACK/response needed from AP for
 * this message. AP will start the process of placing the PCIe function in D0
 * when it receives this request. CP will get notification via D-state change
 * interrupt when the PCIe function is placed in D0.
 */
typedef struct {
	pcie_common_header_t common_header;
	/* A bit_mask of the functions for whom wake is being requested.
    * Setting Bit0 is invalid, as fn0 is already awake, and sending this message.
    * Bit1 => fn1 wants to be woken up
    * Bit2 => fn2 wants to be woken up, and so on.
    * We only support 7 functions currently (fn0 to fn6). fn7 is not used.
    */
	uint8_t func_bit_mask;
	/**
     * Explicit padding to the max size of a control ring message.
     * This will allow future expansion of this message while keeping
     * a total size of 32 bytes.
     */
	uint8_t reserved[27];
} ar_cp_req_wake_t;

/**
 * The pcie_ap_req_d3_t is sent from the AP to the ARP to request ARP to
 * got to D3hot state. This is a debug feature for testing D3hot entry at Janus layer.
 * For production, AP will only write to PMCSR to take function to D3hot state, and f/w will
 * get interrupt for D state change, and then perform the necessary actions to take device to D3.
 * The ARP will return a pcie_ap_resp_d3_t on a ctrl ring with the ACK/NACK
 */
typedef struct {
	pcie_common_header_t common_header;
	/**
     * Explicit padding to the max size of a control ring message.
     * This will allow future expansion of this message while keeping
     * a total size of 32 bytes.
     */
	uint8_t reserved[28];
} pcie_ap_req_d3_t;

/**
 * The pcie_ap_resp_d3_t is the response to a pcie_ap_req_d3_t.
 * ARP will respond with PCIE_REQ_SUCCESS (0) or a failure flag (non-zero).
 */
typedef struct {
	pcie_common_header_t common_header;
	/**
     * Explicit padding to the max size of a control ring message.
     * This will allow future expansion of this message while keeping
     * a total size of 32 bytes.
     */
	uint8_t reserved[28];
} pcie_ap_resp_d3_t;

#define PCIE_GENERIC_CTRL_RESP_FAIL 0x1

/**
 * The generic control message is used to send a generic control message from
 * CP to the AP. This is used to nack unknown requestes.
 */
typedef struct {
	pcie_common_header_t common_header;
} pcie_ctrl_generic_ctl_resp_t;

/// pcie_ctrl_req_t
typedef union {
	pcie_common_header_t common_header;
	pcie_create_ring_req_t create;
	pcie_ping_req_t ping;
	pcie_disable_ring_req_t disable;
	pcie_delete_ring_req_t del_ring;
	pcie_arp_info_req_t arp_info;
	pcie_aperture_map_req_t aperture_map_req;
	pcie_aperture_unmap_req_t aperture_unmap_req;
	ar_duty_cycle_req_t ar_duty_cycle_req;
	pcie_delete_all_rings_req_t delete_all_rings_req;
	pcie_ap_req_d3_t ap_req_d3;
	uint8_t align[32];
} pcie_ctrl_req_t;

/// pcie_ctrl_resp_t
typedef union {
	pcie_common_header_t common_header;
	pcie_create_ring_resp_t create;
	pcie_ping_resp_t ping;
	pcie_disable_ring_resp_t disable;
	pcie_delete_ring_resp_t del_ring;
	pcie_dst_buf_get_msg_t buf_req;
	pcie_arp_info_resp_t arp_info;
	pcie_aperture_map_resp_t aperture_map_resp;
	pcie_aperture_unmap_resp_t aperture_unmap_resp;
	ar_duty_cycle_resp_t ar_duty_cycle_resp;
	pcie_delete_all_rings_resp_t delete_all_rings_resp;
	pcie_ctrl_generic_ctl_resp_t generic_resp;
	ar_cp_req_d3_t ar_cp_req_d3;
	pcie_ap_resp_d3_t ap_resp_d3;
	ar_cp_req_wake_t ar_cp_req_wake;
	uint8_t align[32];
} pcie_ctrl_resp_t;

/// Macros
#define PCIE_GET_MSG_TYPE(r) (((pcie_common_header_t *)(r))->msg_type)

#pragma pack(pop)

#endif // !FB_PCIE_DRVR_SHARED_H
