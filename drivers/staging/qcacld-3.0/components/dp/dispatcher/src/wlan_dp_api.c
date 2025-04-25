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
 * DOC: wlan_dp_api.c
 *
 */

#include "wlan_dp_main.h"
#include "wlan_dp_api.h"
#include <wlan_dp_fisa_rx.h>
#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION
#include "wlan_fpm_table.h"
#endif


void wlan_dp_update_peer_map_unmap_version(uint8_t *version)
{
	__wlan_dp_update_peer_map_unmap_version(version);
}

QDF_STATUS wlan_dp_runtime_suspend(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	return __wlan_dp_runtime_suspend(soc, pdev_id);
}

QDF_STATUS wlan_dp_runtime_resume(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	return __wlan_dp_runtime_resume(soc, pdev_id);
}

void wlan_dp_print_fisa_rx_stats(enum cdp_fisa_stats_id stats_id)
{
	dp_print_fisa_rx_stats(stats_id);
}

void wlan_dp_set_fst_in_cmem(bool fst_in_cmem)
{
	dp_set_fst_in_cmem(fst_in_cmem);
}

void wlan_dp_set_fisa_dynamic_aggr_size_support(bool dynamic_aggr_size_support)
{
	dp_set_fisa_dynamic_aggr_size_support(dynamic_aggr_size_support);
}

#ifdef WLAN_FEATURE_LOCAL_PKT_CAPTURE
bool wlan_dp_is_local_pkt_capture_enabled(struct wlan_objmgr_psoc *psoc)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	return cdp_cfg_get(soc, cfg_dp_local_pkt_capture);
}
#endif

#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION
bool wlan_dp_fpm_is_tid_override(qdf_nbuf_t nbuf, uint8_t *tid)
{
	return fpm_is_tid_override(nbuf, tid);
}
#endif

void wlan_dp_update_def_link(struct wlan_objmgr_psoc *psoc,
			     struct qdf_mac_addr *intf_mac,
			     struct wlan_objmgr_vdev *vdev)
{
	__wlan_dp_update_def_link(psoc, intf_mac, vdev);
}
