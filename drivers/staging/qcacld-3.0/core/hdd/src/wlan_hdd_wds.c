/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_wds.c
 *
 * WLAN Host Device Driver file for wds (4 address frame header when
 * SA and TA are different) support.
 *
 */

/* Include Files */
#include <cdp_txrx_ctrl.h>
#include <wlan_hdd_main.h>
#include "wlan_hdd_wds.h"

void hdd_wds_config_dp_repeater_mode(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	ol_txrx_soc_handle soc;
	cdp_config_param_type vdev_param;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return;

	if (!wlan_mlme_get_wds_mode(psoc))
		return;

	soc = wlan_psoc_get_dp_handle(psoc);

	vdev_param.cdp_vdev_param_wds = true;
	if (cdp_txrx_set_vdev_param(soc, vdev->vdev_objmgr.vdev_id,
				    CDP_ENABLE_WDS,
				    vdev_param))
		hdd_debug("Failed to set WDS param on DP vdev");
}

void
hdd_wds_replace_peer_mac(void *soc, struct hdd_adapter *adapter,
			 uint8_t *mac_addr)
{
	struct cdp_ast_entry_info ast_entry_info = {0};
	cdp_config_param_type val;
	QDF_STATUS status;

	if (!cdp_find_peer_exist(soc, OL_TXRX_PDEV_ID, mac_addr)) {
		status = cdp_txrx_get_vdev_param(soc, adapter->deflink->vdev_id,
						 CDP_ENABLE_WDS, &val);
		if (!QDF_IS_STATUS_SUCCESS(status))
			return;

		if (!val.cdp_vdev_param_wds)
			return;

		if (!cdp_peer_get_ast_info_by_soc(soc,  mac_addr,
						  &ast_entry_info))
			return;

		qdf_mem_copy(mac_addr, ast_entry_info.peer_mac_addr,
			     QDF_MAC_ADDR_SIZE);
	}
}
