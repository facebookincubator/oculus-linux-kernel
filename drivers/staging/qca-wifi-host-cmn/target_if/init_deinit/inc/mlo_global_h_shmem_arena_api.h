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
 *  DOC: mlo_global_h_shmem_arena.h
 *  This file contains APIs that are exposed by the the MLO
 *  global shared memory arena.
 */

#ifndef _MLO_GLOBAL_H_SHMEM_ARENA_API_H_
#define _MLO_GLOBAL_H_SHMEM_ARENA_API_H_

#include<qdf_types.h>
#include<wlan_lmac_if_def.h>

#ifdef WLAN_MLO_GLOBAL_SHMEM_SUPPORT
/**
 * global_shmem_register_wifi3_0_ops() - Register wifi3.0 operations of
 * global shared memory access module
 * @shmem_local_ops: Pointer to the local ops table of global shared mem module.
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS global_shmem_register_wifi3_0_ops(
		struct wlan_lmac_if_global_shmem_local_ops *shmem_local_ops);

#ifdef WLAN_MGMT_RX_REO_SUPPORT
/**
 * mgmt_rx_reo_register_wifi3_0_ops() - Register wifi3.0 operations of MGMT Rx
 * REO module
 * @reo_low_level_ops: Pointer to low level ops table of MGMT Rx REO module.
 *
 * This API fills @reo_low_level_ops table with the wifi3.0 layer functions of
 * MGMT Rx REO module.
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS mgmt_rx_reo_register_wifi3_0_ops(
	struct wlan_lmac_if_mgmt_rx_reo_low_level_ops *reo_low_level_ops);
#endif /* WLAN_MGMT_RX_REO_SUPPORT */
#endif /* WLAN_MLO_GLOBAL_SHMEM_SUPPORT */
#endif /* _MLO_GLOBAL_H_SHMEM_ARENA_API_H_ */
