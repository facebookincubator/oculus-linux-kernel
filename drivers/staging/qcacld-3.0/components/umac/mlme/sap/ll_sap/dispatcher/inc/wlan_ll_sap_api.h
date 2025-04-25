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
 * DOC: contains ll_lt_sap API definitions specific to the bearer
 * switch, channel selection functionalities
 */

#ifndef _WLAN_LL_LT_SAP_API_H_
#define _WLAN_LL_LT_SAP_API_H_

#include <wlan_cmn.h>
#include <wlan_objmgr_vdev_obj.h>
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_ll_sap_public_structs.h"
#include "wlan_cm_public_struct.h"
#include "wlan_policy_mgr_public_struct.h"

#ifdef WLAN_FEATURE_LL_LT_SAP
#ifdef WLAN_FEATURE_BEARER_SWITCH
/**
 * wlan_ll_lt_sap_bearer_switch_get_id() - Get the request id for bearer switch
 * request
 * @psoc: Pointer to psoc
 * Return: Bearer switch request id
 */
wlan_bs_req_id
wlan_ll_lt_sap_bearer_switch_get_id(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_ll_lt_sap_switch_bearer_to_ble() - Switch audio transport to BLE
 * @psoc: Pointer to psoc
 * @bs_request: Pointer to bearer switch request
 * Return: QDF_STATUS_SUCCESS on successful bearer switch else failure
 */
QDF_STATUS wlan_ll_lt_sap_switch_bearer_to_ble(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_bearer_switch_request *bs_request);

/**
 * wlan_ll_sap_switch_bearer_on_sta_connect_start() - Switch bearer during
 * station connection start
 * @psoc: Pointer to psoc
 * @scan_list: Pointer to the candidate list
 * @vdev_id: Vdev id of the requesting vdev
 * @cm_id: connection manager id of the current connect request
 * Return: QDF_STATUS_SUCCESS on successful bearer switch
 *         QDF_STATUS_E_ALREADY, if bearer switch is not required
 *         else failure
 */
QDF_STATUS wlan_ll_sap_switch_bearer_on_sta_connect_start(
						struct wlan_objmgr_psoc *psoc,
						qdf_list_t *scan_list,
						uint8_t vdev_id,
						wlan_cm_id cm_id);
#else
static inline wlan_bs_req_id
wlan_ll_lt_sap_bearer_switch_get_id(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline QDF_STATUS
wlan_ll_lt_sap_switch_bearer_to_ble(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_bearer_switch_request *bs_request)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_start(struct wlan_objmgr_psoc *psoc,
					       qdf_list_t *scan_list,
					       uint8_t vdev_id,
					       wlan_cm_id cm_id)

{
	return QDF_STATUS_E_ALREADY;
}

static inline QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_complete(struct wlan_objmgr_psoc *psoc,
						  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_ll_sap_switch_bearer_on_sta_connect_complete() - Switch bearer during
 * station connection complete
 * @psoc: Pointer to psoc
 * @vdev_id: Vdev id of the requesting vdev
 * Return: QDF_STATUS_SUCCESS on successful bearer switch
 *         QDF_STATUS_E_ALREADY, if bearer switch is not required
 *         else failure
 */
QDF_STATUS wlan_ll_sap_switch_bearer_on_sta_connect_complete(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id);

/**
 * wlan_ll_lt_sap_get_freq_list() - Get frequency list for LL_LT_SAP
 * @psoc: Pointer to psoc object
 * @freq_list: Pointer to wlan_ll_lt_sap_freq_list structure
 * @vdev_id: Vdev Id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ll_lt_sap_get_freq_list(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_ll_lt_sap_freq_list *freq_list,
				uint8_t vdev_id);

/**
 * wlan_ll_lt_sap_override_freq() - Return frequency on which LL_LT_SAP can
 * be started
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev Id of LL_LT_SAP
 * @chan_freq: current frequency of ll_lt_sap
 *
 * This function checks if ll_lt_sap can come up on the given frequency, if it
 * can come up on given frequency then return same frequency else return a
 * different frequency on which ll_lt_sap can come up
 *
 * Return: valid ll_lt_sap frequency
 */
qdf_freq_t wlan_ll_lt_sap_override_freq(struct wlan_objmgr_psoc *psoc,
					uint32_t vdev_id,
					qdf_freq_t chan_freq);

/**
 * wlan_get_ll_lt_sap_restart_freq() - Get restart frequency on which LL_LT_SAP
 * can be re-started
 * @pdev: Pointer to pdev object
 * @chan_freq: current frequency of ll_lt_sap
 * @vdev_id: Vdev Id of LL_LT_SAP
 * @csa_reason: Reason for the CSA
 *
 * This function checks if ll_lt_sap needs to be restarted, if yes, it returns
 * new valid frequency on which ll_lt_sap can be restarted else return same
 * frequency.
 *
 * Return: valid ll_lt_sap frequency
 */
qdf_freq_t
wlan_get_ll_lt_sap_restart_freq(struct wlan_objmgr_pdev *pdev,
				qdf_freq_t chan_freq,
				uint8_t vdev_id,
				enum sap_csa_reason_code *csa_reason);

/**
 * wlan_ll_sap_fw_bearer_switch_req() - FW bearer switch request
 * @psoc: Pointer to psoc object
 * @req_type: Bearer switch request type
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_ll_sap_fw_bearer_switch_req(struct wlan_objmgr_psoc *psoc,
				 enum bearer_switch_req_type req_type);
#else
static inline wlan_bs_req_id
wlan_ll_lt_sap_bearer_switch_get_id(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline QDF_STATUS
wlan_ll_lt_sap_switch_bearer_to_ble(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_bearer_switch_request *bs_request)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_start(struct wlan_objmgr_psoc *psoc,
					       qdf_list_t *scan_list,
					       uint8_t vdev_id,
					       wlan_cm_id cm_id)

{
	return QDF_STATUS_E_ALREADY;
}

static inline QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_complete(struct wlan_objmgr_psoc *psoc,
						  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_ll_lt_sap_get_freq_list(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_ll_lt_sap_freq_list *freq_list,
				uint8_t vdev_id)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
qdf_freq_t wlan_ll_lt_sap_override_freq(struct wlan_objmgr_psoc *psoc,
					uint32_t vdev_id,
					qdf_freq_t chan_freq)
{
	return chan_freq;
}

static inline
qdf_freq_t wlan_get_ll_lt_sap_restart_freq(struct wlan_objmgr_pdev *pdev,
					   qdf_freq_t chan_freq,
					   uint8_t vdev_id,
					   enum sap_csa_reason_code *csa_reason)
{
	return 0;
}
#endif /* WLAN_FEATURE_LL_LT_SAP */
#endif /* _WLAN_LL_LT_SAP_API_H_ */
