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
 * DOC: wlan_qmi_wfds_api.c
 *
 * QMI component north bound interface definitions
 */

#include "wlan_qmi_wfds_api.h"
#include "wlan_qmi_objmgr.h"
#include "wlan_qmi_priv.h"
#include "wlan_qmi_main.h"

QDF_STATUS wlan_qmi_wfds_init(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_qmi_psoc_context *qmi_ctx = qmi_psoc_get_priv(psoc);

	if (!qmi_ctx) {
		qmi_err("QMI context is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (qmi_ctx->qmi_cbs.qmi_wfds_init)
		return qmi_ctx->qmi_cbs.qmi_wfds_init();

	return QDF_STATUS_E_FAILURE;
}

void wlan_qmi_wfds_deinit(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_qmi_psoc_context *qmi_ctx = qmi_psoc_get_priv(psoc);

	if (!qmi_ctx) {
		qmi_err("QMI context is NULL");
		return;
	}

	if (qmi_ctx->qmi_cbs.qmi_wfds_deinit)
		qmi_ctx->qmi_cbs.qmi_wfds_deinit();
}

QDF_STATUS
wlan_qmi_wfds_send_config_msg(struct wlan_objmgr_psoc *psoc,
			      struct wlan_qmi_wfds_config_req_msg *src_info)
{
	struct wlan_qmi_psoc_context *qmi_ctx = qmi_psoc_get_priv(psoc);

	if (!qmi_ctx) {
		qmi_err("QMI context is NULL");
		return  QDF_STATUS_E_INVAL;
	}

	if (qmi_ctx->qmi_cbs.qmi_wfds_send_config_msg)
		return qmi_ctx->qmi_cbs.qmi_wfds_send_config_msg(src_info);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_qmi_wfds_send_req_mem_msg(struct wlan_objmgr_psoc *psoc,
			       struct wlan_qmi_wfds_mem_req_msg *src_info)
{
	struct wlan_qmi_psoc_context *qmi_ctx = qmi_psoc_get_priv(psoc);

	if (!qmi_ctx) {
		qmi_err("QMI context is NULL");
		return  QDF_STATUS_E_INVAL;
	}

	if (qmi_ctx->qmi_cbs.qmi_wfds_send_req_mem_msg)
		return qmi_ctx->qmi_cbs.qmi_wfds_send_req_mem_msg(src_info);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_qmi_wfds_ipcc_map_n_cfg_msg(struct wlan_objmgr_psoc *psoc,
			struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg *src_info)
{
	struct wlan_qmi_psoc_context *qmi_ctx = qmi_psoc_get_priv(psoc);

	if (!qmi_ctx) {
		qmi_err("QMI context is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (qmi_ctx->qmi_cbs.qmi_wfds_send_ipcc_map_n_cfg_msg)
		return qmi_ctx->qmi_cbs.qmi_wfds_send_ipcc_map_n_cfg_msg(src_info);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_qmi_wfds_send_misc_req_msg(struct wlan_objmgr_psoc *psoc, bool is_ssr)
{
	struct wlan_qmi_psoc_context *qmi_ctx = qmi_psoc_get_priv(psoc);

	if (!qmi_ctx) {
		qmi_err("QMI context is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (qmi_ctx->qmi_cbs.qmi_wfds_send_misc_req_msg)
		return qmi_ctx->qmi_cbs.qmi_wfds_send_misc_req_msg(is_ssr);

	return QDF_STATUS_E_FAILURE;
}
