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
 * DOC: contains definitions for NAN component
 */

#include "nan_public_structs.h"
#include "wlan_nan_api.h"
#include "../../core/src/nan_main_i.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_nan_api_i.h"
#include "cfg_nan_api.h"

inline enum nan_datapath_state wlan_nan_get_ndi_state(
					struct wlan_objmgr_vdev *vdev)
{
	enum nan_datapath_state val;
	struct nan_vdev_priv_obj *priv_obj = nan_get_vdev_priv_obj(vdev);

	if (!priv_obj) {
		nan_err("priv_obj is null");
		return NAN_DATA_INVALID_STATE;
	}

	qdf_spin_lock_bh(&priv_obj->lock);
	val = priv_obj->state;
	qdf_spin_unlock_bh(&priv_obj->lock);

	return val;
}

uint8_t wlan_nan_get_vdev_id_from_bssid(struct wlan_objmgr_pdev *pdev,
					tSirMacAddr bssid,
					wlan_objmgr_ref_dbgid dbg_id)
{
	return nan_get_vdev_id_from_bssid(pdev, bssid, dbg_id);
}

bool wlan_nan_is_disc_active(struct wlan_objmgr_psoc *psoc)
{
	return nan_is_disc_active(psoc);
}

bool wlan_nan_is_eht_capable(struct wlan_objmgr_psoc *psoc)
{
	return cfg_nan_is_eht_cap_enable(psoc);
}
