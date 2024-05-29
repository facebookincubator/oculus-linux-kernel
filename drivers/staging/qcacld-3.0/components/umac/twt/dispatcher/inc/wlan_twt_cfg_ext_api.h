/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_TWT_CFG_EXT_API_H
#define _WLAN_TWT_CFG_EXT_API_H

#if defined(WLAN_SUPPORT_TWT) && defined(WLAN_TWT_CONV_SUPPORTED)
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_twt_public_structs.h>
#include <wlan_mlme_twt_public_struct.h>

/**
 * wlan_twt_cfg_get_req_flag() - Get TWT requestor flag
 * @psoc: Pointer to global psoc object
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS
wlan_twt_cfg_get_req_flag(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_cfg_get_res_flag() - Get TWT responder flag
 * @psoc: Pointer to global psoc object
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS
wlan_twt_cfg_get_res_flag(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_cfg_get_support_in_11n() - Get TWT support on HT cap
 * @psoc: Pointer to global psoc object
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS
wlan_twt_cfg_get_support_in_11n(struct wlan_objmgr_psoc *psoc,
				bool *val);

/**
 * wlan_twt_cfg_get_support_requestor() - Get TWT support of requestor
 * @psoc: Pointer to global psoc object
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS
wlan_twt_cfg_get_support_requestor(struct wlan_objmgr_psoc *psoc,
				   bool *val);

/**
 * wlan_twt_get_requestor_cfg() - Get requestor TWT configuration
 * @psoc: Pointer to psoc object
 * @val: Pointer to value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_get_requestor_cfg(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_get_responder_cfg() - Get TWT responder configuration
 * @psoc: Pointer to PSOC object
 * @val: Pointer to value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_get_responder_cfg(struct wlan_objmgr_psoc *psoc, bool *val);

#ifdef FEATURE_SET
/**
 * wlan_twt_get_feature_info() - Get TWT feature set information
 * @psoc: Pointer to global psoc object
 * @twt_feature_set: pointer to output twt feature set structure
 *
 * Return: None
 */
void wlan_twt_get_feature_info(struct wlan_objmgr_psoc *psoc,
			       struct wlan_twt_features *twt_feature_set);
#endif

#else
static inline QDF_STATUS
wlan_twt_cfg_get_res_flag(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_twt_cfg_get_req_flag(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_twt_cfg_get_support_in_11n(struct wlan_objmgr_psoc *psoc,
				bool *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_twt_cfg_get_support_requestor(struct wlan_objmgr_psoc *psoc,
				   bool *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_twt_get_requestor_cfg(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_twt_get_responder_cfg(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif
