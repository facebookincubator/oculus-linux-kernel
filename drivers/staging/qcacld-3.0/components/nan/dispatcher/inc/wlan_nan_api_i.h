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
 * DOC: contains prototypes for NAN component
 */

#ifndef _WLAN_NAN_API_I_H_
#define _WLAN_NAN_API_I_H_

#include "wlan_objmgr_cmn.h"
#include "nan_public_structs.h"

#ifdef WLAN_FEATURE_NAN
/**
 * wlan_nan_get_ndi_state: get ndi state from vdev obj
 * @vdev: pointer to vdev object
 *
 * Return: ndi state
 */
enum nan_datapath_state wlan_nan_get_ndi_state(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_nan_get_vdev_id_from_bssid() - get NAN vdev_id for BSSID
 * @pdev: PDEV object
 * @bssid: BSSID present in mgmt frame
 * @dbg_id:   Object Manager ref debug id
 *
 * This is wrapper API for nan_get_vdev_id_from_bssid()
 *
 * Return: NAN vdev_id
 */
uint8_t wlan_nan_get_vdev_id_from_bssid(struct wlan_objmgr_pdev *pdev,
					tSirMacAddr bssid,
					wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_nan_is_disc_active() - Check if NAN discovery is active
 * @psoc: Pointer to PSOC object
 *
 * Return: True if Discovery is active
 */
bool wlan_nan_is_disc_active(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_nan_is_eht_capable() - Get NAN EHT capability
 * @psoc: pointer to psoc object
 *
 * Return: Boolean flag indicating whether the NAN EHT capability is
 * disabled or not
 */
bool wlan_nan_is_eht_capable(struct wlan_objmgr_psoc *psoc);

#else
static inline
enum nan_datapath_state wlan_nan_get_ndi_state(struct wlan_objmgr_vdev *vdev)
{
	return NAN_DATA_INVALID_STATE;
}

static inline
uint8_t wlan_nan_get_vdev_id_from_bssid(struct wlan_objmgr_pdev *pdev,
					tSirMacAddr bssid,
					wlan_objmgr_ref_dbgid dbg_id)
{
	return INVALID_VDEV_ID;
}

static inline
bool wlan_nan_is_disc_active(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool
wlan_nan_is_eht_capable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif /*WLAN_FEATURE_NAN */
#endif /*_WLAN_NAN_API_I_H_ */
