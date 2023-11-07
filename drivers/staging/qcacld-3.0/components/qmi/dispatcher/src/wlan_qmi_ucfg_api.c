/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_qmi_ucfg_api.c
 *
 * QMI component north bound interface definitions
 */

#include "wlan_qmi_ucfg_api.h"
#include "wlan_qmi_main.h"
#include "wlan_qmi_objmgr.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_cmn.h"

QDF_STATUS ucfg_qmi_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_QMI,
			qmi_psoc_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		qmi_err("Failed to register psoc create handler for QMI");
		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_QMI,
			qmi_psoc_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		qmi_err("Failed to register psoc destroy handler for QMI");
		goto fail_destroy_psoc;
	}

	return status;

fail_destroy_psoc:
	wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_QMI,
				qmi_psoc_obj_create_notification, NULL);

	return status;
}

QDF_STATUS ucfg_qmi_deinit(void)
{
	QDF_STATUS status;

	qmi_debug("QMI module dispatcher deinit");

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_QMI,
				qmi_psoc_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		qmi_err("Failed to unregister QMI psoc delete handle:%d",
			status);

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_QMI,
				qmi_psoc_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		qmi_err("Failed to unregister QMI psoc create handle:%d",
			status);

	return status;
}

#ifdef QMI_WFDS
/**
 * ucfg_qmi_wfds_register_os_if_callbacks() - API to register wfds os if
 *  callbacks with QMI component
 * @qmi_ctx: QMI component context
 * @cb_obj: callback object
 *
 * Return: None
 */
static void
ucfg_qmi_wfds_register_os_if_callbacks(struct wlan_qmi_psoc_context *qmi_ctx,
				       struct wlan_qmi_psoc_callbacks *cb_obj)
{
	qmi_ctx->qmi_cbs.qmi_wfds_init = cb_obj->qmi_wfds_init;
	qmi_ctx->qmi_cbs.qmi_wfds_deinit = cb_obj->qmi_wfds_deinit;
	qmi_ctx->qmi_cbs.qmi_wfds_send_config_msg =
				cb_obj->qmi_wfds_send_config_msg;
	qmi_ctx->qmi_cbs.qmi_wfds_send_req_mem_msg =
				cb_obj->qmi_wfds_send_req_mem_msg;
	qmi_ctx->qmi_cbs.qmi_wfds_send_ipcc_map_n_cfg_msg =
				cb_obj->qmi_wfds_send_ipcc_map_n_cfg_msg;
	qmi_ctx->qmi_cbs.qmi_wfds_send_misc_req_msg =
				cb_obj->qmi_wfds_send_misc_req_msg;
}
#else
static inline void
ucfg_qmi_wfds_register_os_if_callbacks(struct wlan_qmi_psoc_context *qmi_ctx,
				       struct wlan_qmi_psoc_callbacks *cb_obj)
{
}
#endif

void ucfg_qmi_register_os_if_callbacks(struct wlan_objmgr_psoc *psoc,
				       struct wlan_qmi_psoc_callbacks *cb_obj)
{
	struct wlan_qmi_psoc_context *qmi_ctx = qmi_psoc_get_priv(psoc);

	if (!qmi_ctx) {
		qmi_err("QMI context is NULL");
		return;
	}

	ucfg_qmi_wfds_register_os_if_callbacks(qmi_ctx, cb_obj);
}
