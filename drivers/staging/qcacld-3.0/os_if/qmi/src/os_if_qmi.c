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
 * DOC: os_if_qmi.c
 *
 * This file contains definitions of wrapper APIs for QMI HLOS APIs
 */

#include "os_if_qmi.h"
#include "wlan_qmi_ucfg_api.h"

QDF_STATUS os_if_qmi_handle_init(struct qmi_handle *qmi_hdl,
				 qdf_size_t recv_buf_size,
				 const struct qmi_ops *ops,
				 const struct qmi_msg_handler *qmi_msg_handlers)
{
	int ret;

	ret = qmi_handle_init(qmi_hdl, recv_buf_size, ops, qmi_msg_handlers);
	if (ret < 0) {
		osif_err("QMI handle initialization failed %d", ret);
		return qdf_status_from_os_return(ret);
	}

	return QDF_STATUS_SUCCESS;
}

void os_if_qmi_handle_release(struct qmi_handle *qmi_hdl)
{
	qmi_handle_release(qmi_hdl);
}

QDF_STATUS os_if_qmi_add_lookup(struct qmi_handle *qmi_hdl,
				unsigned int service, unsigned int version,
				unsigned int instance)
{
	int ret;

	ret = qmi_add_lookup(qmi_hdl, service, version, instance);
	if (ret < 0) {
		osif_err("QMI add lookup failed %d", ret);
		return qdf_status_from_os_return(ret);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS os_if_qmi_connect_to_svc(struct qmi_handle *qmi_hdl,
				    struct qmi_service *qmi_svc)
{
	struct sockaddr_qrtr sq = { 0 };
	int ret;

	osif_debug("QMI server arriving: node %u port %u", qmi_svc->node,
		   qmi_svc->port);

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = qmi_svc->node;
	sq.sq_port = qmi_svc->port;

	ret = kernel_connect(qmi_hdl->sock, (struct sockaddr *)&sq,
			     sizeof(sq), 0);
	if (ret < 0) {
		osif_err("Failed to connect to QMI remote service %d", ret);
		return qdf_status_from_os_return(ret);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS os_if_qmi_txn_init(struct qmi_handle *qmi_hdl,
			      struct qmi_txn *qmi_txn,
			      struct qmi_elem_info *qmi_ei, void *resp)
{
	int ret;

	ret = qmi_txn_init(qmi_hdl, qmi_txn, qmi_ei, resp);
	if (ret < 0)
		return qdf_status_from_os_return(ret);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS os_if_qmi_send_request(struct qmi_handle *qmi_hdl,
				  struct sockaddr_qrtr *sq,
				  struct qmi_txn *qmi_txn, int msg_id,
				  uint32_t len, struct qmi_elem_info *ei,
				  const void *req)
{
	int ret;

	ret = qmi_send_request(qmi_hdl, sq, qmi_txn, msg_id, len, ei, req);
	if (ret < 0)
		return qdf_status_from_os_return(ret);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS os_if_qmi_txn_wait(struct qmi_txn *qmi_txn, unsigned long timeout)
{
	int ret;

	ret = qmi_txn_wait(qmi_txn, timeout);
	if (ret < 0)
		return qdf_status_from_os_return(ret);

	return QDF_STATUS_SUCCESS;
}

void os_if_qmi_txn_cancel(struct qmi_txn *qmi_txn)
{
	qmi_txn_cancel(qmi_txn);
}

void os_if_qmi_register_callbacks(struct wlan_objmgr_psoc *psoc,
				  struct wlan_qmi_psoc_callbacks *cb_obj)
{
	os_if_qmi_wfds_register_callbacks(cb_obj);
	ucfg_qmi_register_os_if_callbacks(psoc, cb_obj);
}
