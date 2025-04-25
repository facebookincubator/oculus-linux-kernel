/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_dp_api.h
 *
 */

#if !defined(_WLAN_DP_API_H_)
#define _WLAN_DP_API_H_

#include <cdp_txrx_cmn_struct.h>

/**
 * wlan_dp_update_peer_map_unmap_version() - update peer map unmap version
 * @version: Peer map unmap version pointer to be updated
 *
 * Return: None
 */
void wlan_dp_update_peer_map_unmap_version(uint8_t *version);

/**
 * wlan_dp_runtime_suspend() - Runtime suspend DP handler
 * @soc: CDP SoC handle
 * @pdev_id: DP PDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_dp_runtime_suspend(ol_txrx_soc_handle soc, uint8_t pdev_id);

/**
 * wlan_dp_runtime_resume() - Runtime suspend DP handler
 * @soc: CDP SoC handle
 * @pdev_id: DP PDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_dp_runtime_resume(ol_txrx_soc_handle soc, uint8_t pdev_id);

/**
 * wlan_dp_print_fisa_rx_stats() - Dump fisa stats
 * @stats_id: ID for the stats to be dumped
 *
 * Return: None
 */
void wlan_dp_print_fisa_rx_stats(enum cdp_fisa_stats_id stats_id);

/**
 * wlan_dp_set_fst_in_cmem() - Set flag to indicate FST is in CMEM
 * @fst_in_cmem: Flag to indicate FST is in CMEM
 *
 * Return: None
 */
void wlan_dp_set_fst_in_cmem(bool fst_in_cmem);

/**
 * wlan_dp_set_fisa_dynamic_aggr_size_support - Set flag to indicate dynamic
 *						MSDU aggregation size programming supported
 * @dynamic_aggr_size_support: Flag to indicate dynamic aggregation size support
 *
 * Return: None
 */
void wlan_dp_set_fisa_dynamic_aggr_size_support(bool dynamic_aggr_size_support);

#ifdef WLAN_FEATURE_LOCAL_PKT_CAPTURE
/**
 * wlan_dp_is_local_pkt_capture_enabled() - Get local packet capture config
 * @psoc: pointer to psoc object
 *
 * Return: true if local packet capture is enabled from ini
 *         false otherwise
 */
bool
wlan_dp_is_local_pkt_capture_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
wlan_dp_is_local_pkt_capture_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif /* WLAN_FEATURE_LOCAL_PKT_CAPTURE */

#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION
/**
 * wlan_dp_fpm_is_tid_override() - check network buffer marked with tid override
 * @nbuf: pointer to network buffer
 * @tid: buffer to get tid id skb marked with tid override tag
 *
 * Return: True if skb marked with tid override tag
 */
bool wlan_dp_fpm_is_tid_override(qdf_nbuf_t nbuf, uint8_t *tid);
#endif

/**
 * wlan_dp_update_def_link() - update DP interface default link
 * @psoc: psoc handle
 * @intf_mac: interface MAC address
 * @vdev: objmgr vdev handle to set the def_link in dp_intf
 *
 */
void wlan_dp_update_def_link(struct wlan_objmgr_psoc *psoc,
			     struct qdf_mac_addr *intf_mac,
			     struct wlan_objmgr_vdev *vdev);
#endif
