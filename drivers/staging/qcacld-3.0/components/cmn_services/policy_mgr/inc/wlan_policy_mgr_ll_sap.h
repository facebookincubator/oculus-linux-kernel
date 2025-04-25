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
 * DOC: contains policy manager ll_sap definitions specific to the ll_sap module
 */
#ifndef WLAN_POLICY_MGR_LL_SAP_H
#define WLAN_POLICY_MGR_LL_SAP_H

#include "wlan_objmgr_psoc_obj.h"

#ifdef WLAN_FEATURE_LL_LT_SAP
/**
 * policy_mgr_ll_lt_sap_get_valid_freq() - Check and get valid frequency for
 * the current interface (SAP/P2PGO/LL_LT_SAP)
 * @psoc: PSOC object
 * @pdev: PDEV pointer
 * @vdev_id: Vdev id of the current interface
 * @sap_ch_freq: Frequency of the current interface
 * @cc_switch_mode: Channel switch mode
 * @new_sap_freq: Updated frequency
 * @is_ll_lt_sap_present: Indicates if ll_lt_sap is present or not
 *
 * This API checks if ll_lt_sap is present or not and if ll_lt_sap is present
 * then if current frequency of the ll_lt_sap or concurrent SAP or concurrent
 * P2PGO is valid or not according to ll_lt_sap concurrency, if valid, does not
 * fill anything in the new_sap_freq and if not valid, update the new_sap_freq
 * with some new valid frequency.
 *
 * Return: true/false
 */
void policy_mgr_ll_lt_sap_get_valid_freq(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_pdev *pdev,
					 uint8_t vdev_id,
					 qdf_freq_t sap_ch_freq,
					 uint8_t cc_switch_mode,
					 qdf_freq_t *new_sap_freq,
					 bool *is_ll_lt_sap_present);

/**
 * wlan_policy_mgr_get_ll_lt_sap_vdev_id() - Get ll_lt_sap vdev id
 * @psoc: PSOC object
 *
 * API to find ll_lt_sap vdev id
 *
 * Return: vdev id
 */
uint8_t wlan_policy_mgr_get_ll_lt_sap_vdev_id(struct wlan_objmgr_psoc *psoc);

/**
 * __policy_mgr_is_ll_lt_sap_restart_required() - Check in ll_lt_sap restart is
 * required
 * @psoc: PSOC object
 * @func: Function pointer of the caller function.
 *
 * This API checks if ll_lt_sap restart is required or not
 *
 * Return: true/false
 */
bool __policy_mgr_is_ll_lt_sap_restart_required(struct wlan_objmgr_psoc *psoc,
						const char *func);

#define policy_mgr_is_ll_lt_sap_restart_required(psoc) \
	__policy_mgr_is_ll_lt_sap_restart_required(psoc, __func__)

/**
 * policy_mgr_ll_lt_sap_restart_concurrent_sap() - Check and restart
 * concurrent SAP or ll_lt_sap
 * @psoc: PSOC object
 * @is_ll_lt_sap_enabled: Indicates if ll_lt_sap is getting enabled or
 * getting disabled
 *
 * This API checks and restarts concurrent SAP or ll_lt_sap when ll_lt_sap comes
 * up or goes down.
 * Concurrent SAP and ll_lt_sap should always be on different MAC.
 * restart the concurrent SAP in below scenario:
 * If ll_lt_sap is coming up and HW is not sbs capable and concurrent SAP is
 * operating on 5 GHz, then move concurrent SAP to 2.4 Ghz MAC to allow
 * ll_lt_sap on 5 GHz
 * If ll_lt_sap is going down and if concurrent SAP is on 2.4 GHz then try to
 * restart concurrent SAP on its original user configured frequency
 * If ll_lt_sap interface has come up and in parallel if some other interface
 * comes up on the ll_lt_sap frequency, then ll_lt_sap needs to be restarted.
 *
 * Return: None
 */
void policy_mgr_ll_lt_sap_restart_concurrent_sap(struct wlan_objmgr_psoc *psoc,
						 bool is_ll_lt_sap_enabled);
#else

static inline bool
policy_mgr_is_ll_lt_sap_restart_required(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void policy_mgr_ll_lt_sap_get_valid_freq(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_pdev *pdev,
					 uint8_t vdev_id,
					 qdf_freq_t sap_ch_freq,
					 uint8_t cc_switch_mode,
					 qdf_freq_t *new_sap_freq,
					 bool *is_ll_lt_sap_present)
{
}

static inline
uint8_t wlan_policy_mgr_get_ll_lt_sap_vdev_id(struct wlan_objmgr_psoc *psoc)
{
	return WLAN_INVALID_VDEV_ID;
}

static inline void
policy_mgr_ll_lt_sap_restart_concurrent_sap(struct wlan_objmgr_psoc *psoc,
					    bool is_ll_lt_sap_enabled)
{
}
#endif
#endif /* WLAN_POLICY_MGR_LL_SAP_H */
