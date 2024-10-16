/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_vdev_mgr_api.c
 *
 * This file provides definitions to component APIs to get/set mlme fields in
 * vdev mlme core data structures
 */

#include "include/wlan_vdev_mlme.h"
#include <wlan_vdev_mlme_api.h>
#include <qdf_module.h>
#include <wlan_vdev_mgr_api.h>

void wlan_vdev_mgr_get_param_bssid(struct wlan_objmgr_vdev *vdev,
				   uint8_t *bssid)
{
	struct vdev_mlme_mgmt *mlme_mgmt;
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(
						vdev, WLAN_UMAC_COMP_MLME);

	if (!vdev_mlme) {
		mlme_err("VDEV_MLME is NULL");
		return;
	}

	mlme_mgmt = &vdev_mlme->mgmt;

	qdf_mem_copy(bssid, mlme_mgmt->generic.bssid,
		     QDF_MAC_ADDR_SIZE);
}

qdf_export_symbol(wlan_vdev_mgr_get_param_bssid);
