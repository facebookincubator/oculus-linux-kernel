/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *  DOC: target_if_twt_cmd.c
 *  This file contains twt component's target related function definitions
 */
#include <target_if_twt.h>
#include <target_if_twt_cmd.h>
#include <target_if_ext_twt.h>
#include "twt/core/src/wlan_twt_common.h"

QDF_STATUS
target_if_twt_enable_req(struct wlan_objmgr_psoc *psoc,
			 struct twt_enable_param *req)
{
	QDF_STATUS ret;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_pdev *pdev;

	if (!psoc) {
		target_if_err("null psoc");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, req->pdev_id,
					  WLAN_TWT_ID);
	if (!pdev) {
		target_if_err("null pdev");
		return QDF_STATUS_E_FAILURE;
	}

	wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);
	if (!wmi_handle) {
		target_if_err("null wmi handle");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_TWT_ID);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_twt_enable_cmd(wmi_handle, req);
	if (QDF_IS_STATUS_ERROR(ret))
		target_if_err("Failed to enable TWT(ret=%d)", ret);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_TWT_ID);

	return ret;
}

QDF_STATUS
target_if_twt_disable_req(struct wlan_objmgr_psoc *psoc,
			  struct twt_disable_param *req)
{
	QDF_STATUS ret;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_pdev *pdev;

	if (!psoc) {
		target_if_err("null psoc");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, req->pdev_id,
					  WLAN_TWT_ID);
	if (!pdev) {
		target_if_err("null pdev");
		return QDF_STATUS_E_FAILURE;
	}

	wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);
	if (!wmi_handle) {
		target_if_err("null wmi handle");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_TWT_ID);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_twt_disable_cmd(wmi_handle, req);
	if (QDF_IS_STATUS_ERROR(ret))
		target_if_err("Failed to disable TWT(ret=%d)", ret);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_TWT_ID);

	return ret;
}

