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
 * DOC: wlan_qmi_main.c
 *
 * QMI component core function definitions
 */

#include "wlan_qmi_main.h"
#include "wlan_qmi_public_struct.h"
#include "wlan_qmi_objmgr.h"

QDF_STATUS
qmi_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct wlan_qmi_psoc_context *qmi_ctx;
	QDF_STATUS status;

	qmi_ctx = qdf_mem_malloc(sizeof(*qmi_ctx));
	if (!qmi_ctx)
		return QDF_STATUS_E_NOMEM;

	qmi_ctx->psoc = psoc;

	status = wlan_objmgr_psoc_component_obj_attach(psoc, WLAN_UMAC_COMP_QMI,
						       qmi_ctx,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		qmi_err("Failed to attach psoc QMI component obj");
		qdf_mem_free(qmi_ctx);
		return status;
	}

	return status;
}

QDF_STATUS
qmi_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct wlan_qmi_psoc_context *qmi_ctx;
	QDF_STATUS status;

	qmi_ctx = qmi_psoc_get_priv(psoc);
	if (!qmi_ctx) {
		qmi_err("psoc priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc, WLAN_UMAC_COMP_QMI,
						       qmi_ctx);
	if (QDF_IS_STATUS_ERROR(status))
		qmi_err("Failed to detach psoc QMI component obj");

	qdf_mem_free(qmi_ctx);

	return status;
}
