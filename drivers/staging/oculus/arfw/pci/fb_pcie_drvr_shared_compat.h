/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file fb_pcie_drvr_shared_compat.h
 *
 * @brief Legacy fields from previous PCIe protocol versions.
 *
 * @details See fb_pcie_drvr_shared.h for current versions.
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef FB_PCIE_DRVR_SHARED_COMPAT_H
#define FB_PCIE_DRVR_SHARED_COMPAT_H

#pragma pack(push, 1)

/// Macros and Constants

/// Backwards compatibility fields
#define PCIE_ARP_PROTOCOL_V1 (0x0001)
#define PCIE_ARP_DOORBELL_OFFSET_V1 (0x2000)

/// Backwards compatibility fields
#define PCIE_ARP_PROTOCOL_V2 (0x0002)
#define PCIE_ARP_DOORBELL_OFFSET_V2 (0x2034)

#define PCIE_ARP_PROTOCOL_V3 (0x0003)
#define PCIE_ARP_PROTOCOL_V4 (0x0004)
#define PCIE_ARP_PROTOCOL_V5 (0x0005)

#define PCIE_ARP_PROTOCOL_MINOR_V0 (0x0)
#define PCIE_ARP_PROTOCOL_MINOR_V1 (0x1)
#define PCIE_ARP_PROTOCOL_MINOR_V4 (0x4)

#define PCIE_AP_PROTOCOL_V0 (0x0)
#define PCIE_AP_PROTOCOL_MINOR_V1 (0x1)

#define PCIE_AP_SRC_DATA_MAX_V2 8
#define PCIE_AP_DST_DATA_MAX_V2 8

/// Fields updated by ARP for the data rings
typedef struct {
	/// Read index for the ap_src rings
	uint16_t ap_src_ring_rd_index[PCIE_AP_SRC_DATA_MAX_V2];
	/// Write index for the ap_dst rings
	uint16_t ap_dst_ring_wr_index[PCIE_AP_DST_DATA_MAX_V2];

	/// Status for the ap_src rings
	uint16_t ap_src_ring_status[PCIE_AP_SRC_DATA_MAX_V2];
	/// Status for the ap_dst rings
	uint16_t ap_dst_ring_status[PCIE_AP_DST_DATA_MAX_V2];

	/// Make entire struct 64 byte aligned for cache coherency
} pcie_ring_arp_update_v2_t;

/// Fields updated by AP for the data rings
typedef struct {
	/// Read index for the ap_dst rings
	uint16_t ap_dst_ring_rd_index[PCIE_AP_DST_DATA_MAX_V2];
	/// Write index for the ap_src rings
	uint16_t ap_src_ring_wr_index[PCIE_AP_SRC_DATA_MAX_V2];

	/// Make entire struct 64 byte aligned for cache coherency
	uint8_t rsvd[32];
} pcie_ring_ap_update_v2_t;

/// pcie_bar_status_block_area_v2_t;
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
	/// AP write:  Write Index for the Control Tx ring
	uint16_t ap_src_ctrl_wr_index;
	/// AP write:  Read Index for the Control Rx ring
	uint16_t ap_dst_ctrl_rd_index;
	/// AP write: Write Index for the src buffer ring
	uint16_t ap_src_buf_wr_index;
	/// align AP update fields to 64 byte boundary for cache alignment.
	uint8_t rsvd_ap[28];

	/// Permanent Ring Entries, stats, status updated by the ARP

	/// ARP write: Read Index for the Control transmit ring
	uint16_t ap_src_ctrl_rd_index;
	/// ARP write: Write Index for the Control receive ring
	uint16_t ap_dst_ctrl_wr_index;
	/// ARP write: Read Index for the AP src buf ring
	uint16_t ap_src_buf_rd_index;
	/// ARP write;  Status for the ap_dst_ctrl ring Eg: Full; error
	uint16_t ap_dst_ctrl_status;
	/// ARP write;  Status for the ap_src_ctrl ring Eg: Full; error
	uint16_t ap_src_ctrl_status;
	/// ARP write;  Status for the ap_src_buf ring Eg: Full; error
	uint16_t ap_src_buf_status;
	/// ARP write: Total messages sent on the control receive ring by the ARP
	uint32_t ap_dst_ctrl_total_count;
	/**
	 * ARP write;  Count of messages dropped by the ARP for the Control receive
	 * ring
	 */
	uint32_t ap_dst_ctrl_drop_count;
	/// ARP write;  Total messages on the control Tx ring received by the ARP
	uint32_t ap_src_ctrl_total_count;
	/// ARP write;  Total messages on the AP src buf ring received by the ARP
	uint32_t ap_src_buf_total_count;
	/// align ARP update fields to 64 byte boundary for cache alignment.
	uint8_t rsvd_arp[36];

	/// Variable ring indices and updated by the AP
	pcie_ring_ap_update_v2_t ap_update;

	/// Variable ring indices and status updated by the ARP
	pcie_ring_arp_update_v2_t arp_update;

	/// Variable ring statistics updated by the ARP
	/// pcie_ap_src_ring_stats_t
	pcie_ap_src_ring_stats_t ap_src_data_stats[PCIE_AP_SRC_DATA_MAX_V2];
	/// pcie_ap_dst_ring_stats_t
	pcie_ap_dst_ring_stats_t ap_dst_data_stats[PCIE_AP_DST_DATA_MAX_V2];
	/// align to 64 byte boundary for cache alignment.
} pcie_bar_status_block_area_v2_t;

/// pcie_bar_memory_area_v2_t;
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
	/**
	 * ARP read/write; AP read/write
	 * Status block maintains read and write pointers of all the software rings.
	 * Size depends on the number of rings
	 */

	/**
	 * Align to 64 byte cache line boundary
	 */
	uint8_t rsvd_shm[32];

#if defined(CONFIG_PLATFORM_FERM)
	/**
	 *  No MSI or DB interrupts on FERM yet.
	 * Use counters in shared memory instead.
	 */
	uint32_t arp_intr_counter; // ARP updates counter to notify AP
	uint32_t ap_intr_ctr; // AP updates counter to notify ARP
#endif
} pcie_bar_memory_area_v2_t;

#pragma pack(pop)

#endif // !FB_PCIE_DRVR_SHARED_COMPAT_H
