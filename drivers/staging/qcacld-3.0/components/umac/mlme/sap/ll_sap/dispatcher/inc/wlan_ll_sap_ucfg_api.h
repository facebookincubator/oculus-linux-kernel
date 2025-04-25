/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: This file contains ll_sap north bound interface declarations
 */

#ifndef _WLAN_LL_SAP_UCFG_API_H_
#define _WLAN_LL_SAP_UCFG_API_H_
#include "wlan_ll_sap_public_structs.h"

#ifdef WLAN_FEATURE_LL_LT_SAP

/**
 * ucfg_ll_sap_init() - initializes ll_sap component
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_ll_sap_init(void);

/**
 * ucfg_ll_sap_deinit() - De-initializes ll_sap component
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_ll_sap_deinit(void);

/**
 * ucfg_is_ll_lt_sap_supported() - Check if ll_lt_sap is supported or not
 *@psoc: Psoc pointer
 *
 * Return: True/False
 */
bool ucfg_is_ll_lt_sap_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_ll_lt_sap_request_for_audio_transport_switch() - Request to switch the
 * audio transport medium
 * @vdev: Vdev on which the request is received
 * @req_type: Requested transport switch type
 *
 * Return: Accepted/Rejected
 */
QDF_STATUS ucfg_ll_lt_sap_request_for_audio_transport_switch(
					struct wlan_objmgr_vdev *vdev,
					enum bearer_switch_req_type req_type);

/**
 * ucfg_ll_lt_sap_deliver_audio_transport_switch_resp() - Deliver audio
 * transport switch response
 * @vdev: Vdev on which the request is received
 * @req_type: Transport switch type for which the response is received
 * @status: Status of the response
 *
 * Return: None
 */
void ucfg_ll_lt_sap_deliver_audio_transport_switch_resp(
					struct wlan_objmgr_vdev *vdev,
					enum bearer_switch_req_type req_type,
					enum bearer_switch_status status);

/**
 * ucfg_ll_sap_register_cb() - Register ll_sap osif callbacks
 * @ll_sap_global_ops: Ops which needs to be registered
 *
 * Return: None
 */
void ucfg_ll_sap_register_cb(struct ll_sap_ops *ll_sap_global_ops);

/**
 * ucfg_ll_sap_unregister_cb() - Un-register ll_sap osif callbacks
 *
 * Return: None
 */
void ucfg_ll_sap_unregister_cb(void);

/**
 * ucfg_ll_sap_psoc_enable() - Enable ll_lt_sap psoc
 * @psoc: objmgr psoc pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_ll_sap_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_ll_sap_psoc_disable() - Disable ll_lt_sap psoc
 * @psoc: objmgr psoc pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_ll_sap_psoc_disable(struct wlan_objmgr_psoc *psoc);

#else
static inline QDF_STATUS ucfg_ll_sap_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_ll_sap_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool ucfg_is_ll_lt_sap_supported(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline QDF_STATUS
ucfg_ll_lt_sap_request_for_audio_transport_switch(
					struct wlan_objmgr_vdev *vdev,
					enum bearer_switch_req_type req_type)
{
	return QDF_STATUS_E_INVAL;
}

static inline void
ucfg_ll_lt_sap_deliver_audio_transport_switch_resp(
					struct wlan_objmgr_vdev *vdev,
					enum bearer_switch_req_type req_type,
					enum bearer_switch_status status)
{
}

static inline void ucfg_ll_sap_register_cb(struct ll_sap_ops *ll_sap_global_ops)
{
}

static inline void ucfg_ll_sap_unregister_cb(void)
{
}

static inline QDF_STATUS ucfg_ll_sap_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_ll_sap_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_LL_LT_SAP */
#endif /* _WLAN_LL_SAP_UCFG_API_H_ */

