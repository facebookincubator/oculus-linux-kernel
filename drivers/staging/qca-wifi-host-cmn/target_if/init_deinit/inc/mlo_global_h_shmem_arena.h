/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 *  DOC: mlo_global_h_shmem_arena.h
 *  This file contains APIs and data structures that are used to parse the MLO
 *  global shared memory arena.
 */

#ifndef _MLO_GLOBAL_H_SHMEM_ARENA_H_
#define _MLO_GLOBAL_H_SHMEM_ARENA_H_

#include <qdf_types.h>
#include <target_if.h>
#include "wmi.h"
#include <osdep.h>

#define MGMT_RX_REO_INVALID_SNAPSHOT_VERSION      (-1)

/**
 * wlan_host_mlo_glb_h_shmem_params - MLO global shared memory parameters
 * @major_version: Major version
 * @minor_version: Minor version
 */
struct wlan_host_mlo_glb_h_shmem_params {
	uint16_t major_version;
	uint16_t minor_version;
};

/**
 * wlan_host_mlo_glb_rx_reo_per_link_info - MGMT Rx REO information of a link in
 * MLO global shared memory
 * @link_id: Hardware link ID
 * @fw_consumed: Address of FW consumed snapshot
 * @fw_forwarded: Address of FW forwarded snapshot
 * @hw_forwarded: Address of HW forwarded snapshot
 */
struct wlan_host_mlo_glb_rx_reo_per_link_info {
	uint8_t link_id;
	void *fw_consumed;
	void *fw_forwarded;
	void *hw_forwarded;
};

/**
 * wlan_host_mlo_glb_rx_reo_snapshot_info - MGMT Rx REO information in MLO
 * global shared memory
 * @num_links: Number of valid links
 * @valid_link_bmap: Valid link bitmap
 * @link_info: pointer to an array of Rx REO per-link information
 * @hw_forwarded_snapshot_ver: HW forwarded snapshot version
 * @fw_forwarded_snapshot_ver: FW forwarded snapshot version
 * @fw_consumed_snapshot_ver: FW consumed snapshot version
 */
struct wlan_host_mlo_glb_rx_reo_snapshot_info {
	uint8_t num_links;
	uint16_t valid_link_bmap;
	struct wlan_host_mlo_glb_rx_reo_per_link_info *link_info;
	uint8_t hw_forwarded_snapshot_ver;
	uint8_t fw_forwarded_snapshot_ver;
	uint8_t fw_consumed_snapshot_ver;
};

/**
 * wlan_host_mlo_glb_per_chip_crash_info - per chip crash information in MLO
 * global shared memory
 * @chip_id: MLO Chip ID
 * @crash_reason: Address of the crash_reason corresponding to chip_id
 */
struct wlan_host_mlo_glb_per_chip_crash_info {
	uint8_t chip_id;
	void *crash_reason;
};

/**
 * wlan_host_mlo_glb_chip_crash_info - chip crash information in MLO
 * global shared memory
 * @no_of_chips: No of partner chip to which crash information is shared
 * @valid_chip_bmap: Bitmap to indicate the chip to which the crash information
 * is shared
 * @per_chip_crash_info: pointer to an array of crash information associated
 * with each chip
 */
struct wlan_host_mlo_glb_chip_crash_info {
	uint8_t no_of_chips;
	qdf_bitmap(valid_chip_bmap, QDF_CHAR_BIT);
	struct wlan_host_mlo_glb_per_chip_crash_info *per_chip_crash_info;
};

/**
 * wlan_host_mlo_glb_h_shmem_arena_ctx - MLO Global shared memory arena context
 * @shmem_params: shared memory parameters
 * @rx_reo_snapshot_info: MGMT Rx REO snapshot information
 * @init_count: Number of init calls
 */
struct wlan_host_mlo_glb_h_shmem_arena_ctx {
	struct wlan_host_mlo_glb_h_shmem_params shmem_params;
	struct wlan_host_mlo_glb_rx_reo_snapshot_info rx_reo_snapshot_info;
	struct wlan_host_mlo_glb_chip_crash_info chip_crash_info;
	qdf_atomic_t init_count;
};

#ifdef WLAN_MLO_GLOBAL_SHMEM_SUPPORT
/**
 * mlo_glb_h_shmem_arena_ctx_init() - Initialize MLO Global shared memory arena
 * context on Host
 * @arena_vaddr: Virtual address of the MLO Global shared memory arena
 * @arena_len: Length (in bytes) of the MLO Global shared memory arena
 * @grp_id: Id of the required MLO Group
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS mlo_glb_h_shmem_arena_ctx_init(void *arena_vaddr,
					  size_t arena_len,
					  uint8_t grp_id);

/**
 * mlo_glb_h_shmem_arena_ctx_deinit() - De-initialize MLO Global shared memory
 * arena context on Host
 * @grp_id: Id of the required MLO Group
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS mlo_glb_h_shmem_arena_ctx_deinit(uint8_t grp_id);
#endif

#ifdef WLAN_MLO_GLOBAL_SHMEM_SUPPORT
/**
 * mlo_glb_h_shmem_arena_get_crash_reason_address(): get the address of crash
 * reason associated with chip_id
 * @grp_id: Id of the required MLO Group
 * @chip_id: Id of the MLO chip
 *
 * Return: Address of crash_reason field from global shmem arena in case of
 * success, else returns NULL
 */
void *mlo_glb_h_shmem_arena_get_crash_reason_address(uint8_t grp_id,
						     uint8_t chip_id);

/**
 * mlo_glb_h_shmem_arena_get_no_of_chips_from_crash_info() - Get number of chips
 * from crash info
 * @grp_id: Id of the required MLO Group
 *
 * Return: number of chips participating in MLO from crash info shared by target
 * in case of sccess, else returns 0
 */
uint8_t mlo_glb_h_shmem_arena_get_no_of_chips_from_crash_info(uint8_t grp_id);
#endif

#ifdef WLAN_MGMT_RX_REO_SUPPORT
/**
 * mgmt_rx_reo_get_valid_link_bitmap() - Get valid link bitmap
 * @grp_id: Id of the required MLO Group
 *
 * Return: valid link bitmap
 */
uint16_t mgmt_rx_reo_get_valid_link_bitmap(uint8_t grp_id);

/**
 * mgmt_rx_reo_get_num_links() - Get number of links to be used by MGMT Rx REO
 * @grp_id: Id of the required MLO Group
 *
 * Return: number of links in case of success, else -1
 */
int mgmt_rx_reo_get_num_links(uint8_t grp_id);

/**
 * mgmt_rx_reo_get_snapshot_address() - Get the address of MGMT Rx REO snapshot
 * @grp_id: Id of the required MLO Group
 * @link_id: Link ID of the radio to which this snapshot belongs
 * @snapshot_id: ID of the snapshot
 *
 * Return: virtual address of the snapshot on success, else NULL
 */
void *mgmt_rx_reo_get_snapshot_address(
		uint8_t grp_id,
		uint8_t link_id,
		enum mgmt_rx_reo_shared_snapshot_id snapshot_id);

/**
 * mgmt_rx_reo_get_snapshot_version() - Get the version of MGMT Rx REO snapshot
 * @grp_id: Id of the required MLO Group
 * @snapshot_id: ID of the snapshot
 *
 * Return: Snapshot version
 */
int8_t mgmt_rx_reo_get_snapshot_version
			(uint8_t grp_id,
			 enum mgmt_rx_reo_shared_snapshot_id snapshot_id);
#endif /* WLAN_MGMT_RX_REO_SUPPORT */
#endif
