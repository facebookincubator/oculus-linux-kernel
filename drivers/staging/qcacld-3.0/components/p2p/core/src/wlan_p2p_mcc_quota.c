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

/**
 * DOC: This file contains main P2P mcc quota event process core function
 * implementation
 */

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include "wlan_p2p_mcc_quota_public_struct.h"
#include "wlan_p2p_public_struct.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_p2p_main.h"
#include "wlan_p2p_mcc_quota.h"

/**
 * struct wlan_mcc_quota_context - context for vdev iterate handler
 * @indicated: mcc quota info indicated to os
 * @quota_info: mcc quota information
 * @p2p_soc_obj: p2p soc object
 */
struct wlan_mcc_quota_context {
	bool indicated;
	struct mcc_quota_info *quota_info;
	struct p2p_soc_priv_obj *p2p_soc_obj;
};

/**
 * wlan_indicate_quota_vdev_handler() - Vdev iterate handler to indicate
 * mcc quota event to vdev object
 * @psoc: psoc object
 * @obj: vdev object
 * @args: handler context
 *
 * Return: void
 */
static void wlan_indicate_quota_vdev_handler(struct wlan_objmgr_psoc *psoc,
					     void *obj, void *args)
{
	struct wlan_objmgr_vdev *vdev = obj;
	struct wlan_mcc_quota_context *context = args;
	struct p2p_soc_priv_obj *p2p_soc_obj = context->p2p_soc_obj;
	enum QDF_OPMODE op_mode;
	QDF_STATUS status;

	op_mode = wlan_vdev_mlme_get_opmode(vdev);
	if (op_mode != QDF_STA_MODE &&
	    op_mode != QDF_SAP_MODE &&
	    op_mode != QDF_P2P_CLIENT_MODE &&
	    op_mode != QDF_P2P_GO_MODE)
		return;

	status = p2p_soc_obj->mcc_quota_ev_os_if_cb(psoc, vdev,
						    context->quota_info);
	if (status != QDF_STATUS_SUCCESS)
		return;

	context->indicated = true;
}

QDF_STATUS p2p_mcc_quota_event_process(struct wlan_objmgr_psoc *psoc,
				       struct mcc_quota_info *event_info)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct wlan_mcc_quota_context context;

	if (!event_info) {
		p2p_err("invalid mcc quota event information");
		return QDF_STATUS_E_INVAL;
	}

	if (!psoc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}
	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							    WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("p2p soc object is NULL");
		return QDF_STATUS_E_INVAL;
	}
	if (!p2p_soc_obj->mcc_quota_ev_os_if_cb) {
		p2p_err("mcc_quota_ev_os_if_cb is NULL");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_zero(&context, sizeof(struct wlan_mcc_quota_context));
	context.quota_info = event_info;
	context.p2p_soc_obj = p2p_soc_obj;
	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     wlan_indicate_quota_vdev_handler,
				     &context, true, WLAN_P2P_ID);

	if (context.indicated)
		return QDF_STATUS_SUCCESS;
	else
		return p2p_soc_obj->mcc_quota_ev_os_if_cb(psoc, NULL,
							  event_info);
}
