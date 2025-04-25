/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *  DOC: mlo_global_h_shmem_arena_api.c
 *  This file contains definition of functions that MLO global
 *  shared memory arena exposes.
 */
#include <mlo_global_h_shmem_arena.h>

#ifdef WLAN_MGMT_RX_REO_SUPPORT
/**
 * mgmt_rx_reo_snapshot_is_valid() - Check if an MGMT Rx REO snapshot is valid
 * @snapshot_low: lower 32-bits of the snapshot
 * @snapshot_version: snapshot version
 *
 * Return: true if snapshot is valid, else false
 */
static bool mgmt_rx_reo_snapshot_is_valid(uint32_t snapshot_low,
					  uint8_t snapshot_version)
{
	return MLO_SHMEM_MGMT_RX_REO_SNAPSHOT_VALID_GET(snapshot_low,
							snapshot_version);
}

/**
 * mgmt_rx_reo_snapshot_get_mgmt_pkt_ctr() - Get the management packet counter
 * from an MGMT Rx REO snapshot
 * @snapshot_low: lower 32-bits of the snapshot
 * @snapshot_version: snapshot version
 *
 * Return: Management packet counter of the snapshot
 */
static uint16_t mgmt_rx_reo_snapshot_get_mgmt_pkt_ctr(uint32_t snapshot_low,
						      uint8_t snapshot_version)
{
	return MLO_SHMEM_MGMT_RX_REO_SNAPSHOT_MGMT_PKT_CTR_GET
					(snapshot_low, snapshot_version);
}

/**
 * mgmt_rx_reo_snapshot_get_redundant_mgmt_pkt_ctr() - Get the
 * redundant management packet counter from MGMT Rx REO snapshot
 * @snapshot_high: higher 32-bits of the snapshot
 *
 * Return: Redundant management packet counter of the snapshot
 */
static uint16_t mgmt_rx_reo_snapshot_get_redundant_mgmt_pkt_ctr
					(uint32_t snapshot_high)
{
	return MLO_SHMEM_MGMT_RX_REO_SNAPSHOT_MGMT_PKT_CTR_REDUNDANT_GET
							(snapshot_high);
}

/**
 * mgmt_rx_reo_snapshot_is_consistent() - Check if an MGMT Rx REO snapshot is
 * consistent
 * @snapshot_low: lower 32-bits of the snapshot
 * @snapshot_high: higher 32-bits of the snapshot
 * @snapshot_version: snapshot version
 *
 * Return: true if the snapshot is consistent, else false
 */
static bool mgmt_rx_reo_snapshot_is_consistent(uint32_t snapshot_low,
					       uint32_t snapshot_high,
					       uint8_t snapshot_version)
{
	return MLO_SHMEM_MGMT_RX_REO_SNAPSHOT_CHECK_CONSISTENCY(snapshot_low,
					snapshot_high, snapshot_version);
}

/**
 * mgmt_rx_reo_snapshot_get_global_timestamp() - Get the global timestamp from
 * MGMT Rx REO snapshot
 * @snapshot_low: lower 32-bits of the snapshot
 * @snapshot_high: higher 32-bits of the snapshot
 * @snapshot_version: snapshot version id
 *
 * Return: Global timestamp of the snapshot
 */
static uint32_t mgmt_rx_reo_snapshot_get_global_timestamp(
	uint32_t snapshot_low, uint32_t snapshot_high, uint8_t snapshot_version)
{
	return MLO_SHMEM_MGMT_RX_REO_SNAPSHOT_GLOBAL_TIMESTAMP_GET
		(snapshot_low, snapshot_high, snapshot_version);
}

QDF_STATUS mgmt_rx_reo_register_wifi3_0_ops(
	struct wlan_lmac_if_mgmt_rx_reo_low_level_ops *reo_low_level_ops)
{
	if (!reo_low_level_ops) {
		target_if_err("Low level ops of MGMT Rx REO is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	reo_low_level_ops->get_valid_link_bitmap =
		mgmt_rx_reo_get_valid_link_bitmap;
	reo_low_level_ops->get_num_links = mgmt_rx_reo_get_num_links;
	reo_low_level_ops->get_snapshot_address =
		mgmt_rx_reo_get_snapshot_address;
	reo_low_level_ops->get_snapshot_version =
		mgmt_rx_reo_get_snapshot_version;
	reo_low_level_ops->snapshot_is_valid =
		mgmt_rx_reo_snapshot_is_valid;
	reo_low_level_ops->snapshot_get_mgmt_pkt_ctr =
		mgmt_rx_reo_snapshot_get_mgmt_pkt_ctr;
	reo_low_level_ops->snapshot_get_redundant_mgmt_pkt_ctr =
		mgmt_rx_reo_snapshot_get_redundant_mgmt_pkt_ctr;
	reo_low_level_ops->snapshot_is_consistent =
		mgmt_rx_reo_snapshot_is_consistent;
	reo_low_level_ops->snapshot_get_global_timestamp =
		mgmt_rx_reo_snapshot_get_global_timestamp;

	reo_low_level_ops->implemented = true;

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_MLO_GLOBAL_SHMEM_SUPPORT
static inline
void global_shmem_register_target_recovery_ops(
	struct wlan_lmac_if_global_shmem_local_ops *shmem_local_ops)
{
	if (!shmem_local_ops) {
		target_if_err("Low level ops of global shmem is NULL");
		return;
	}

	shmem_local_ops->get_crash_reason_address =
		mlo_glb_h_shmem_arena_get_crash_reason_address;
	shmem_local_ops->get_recovery_mode_address =
		mlo_glb_h_shmem_arena_get_recovery_mode_address;
	shmem_local_ops->get_no_of_chips_from_crash_info =
		mlo_glb_h_shmem_arena_get_no_of_chips_from_crash_info;
}
#else
static inline
void global_shmem_register_target_recovery_ops(
	struct wlan_lmac_if_global_shmem_local_ops *shmem_local_ops)
{
	if (!shmem_local_ops) {
		target_if_err("Low level ops of global shmem is NULL");
		return;
	}

	shmem_local_ops->get_crash_reason_address = NULL;
	shmem_local_ops->get_no_of_chips_from_crash_info = NULL;
}
#endif

QDF_STATUS global_shmem_register_wifi3_0_ops(
		struct wlan_lmac_if_global_shmem_local_ops *shmem_local_ops)
{
	if (!shmem_local_ops) {
		target_if_err("Low level ops of global shmem is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	shmem_local_ops->init_shmem_arena_ctx =
		mlo_glb_h_shmem_arena_ctx_init;
	shmem_local_ops->deinit_shmem_arena_ctx =
		mlo_glb_h_shmem_arena_ctx_deinit;

	global_shmem_register_target_recovery_ops(shmem_local_ops);

	shmem_local_ops->implemented = true;

	return QDF_STATUS_SUCCESS;
}
