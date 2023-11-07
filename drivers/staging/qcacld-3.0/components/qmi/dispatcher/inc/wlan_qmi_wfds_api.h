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
 * DOC: wlan_qmi_wfds_api.h
 *
 * Contains QMI wrapper API declarations to connect and send messages to
 * QMI WFDS server
 */
#ifndef _WLAN_QMI_WFDS_API_H_
#define _WLAN_QMI_WFDS_API_H_

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_qmi_public_struct.h"

#ifdef QMI_WFDS
/**
 * wlan_qmi_wfds_init() - Initialize WFDS QMI client
 * @psoc: PSOC handle
 *
 * Returns: QDF status
 */
QDF_STATUS wlan_qmi_wfds_init(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_qmi_wfds_deinit() - Deinitialize WFDS QMI client
 * @psoc: PSOC handle
 *
 * Returns: None
 */
void wlan_qmi_wfds_deinit(struct wlan_objmgr_psoc *psoc);

/*
 * wlan_qmi_wfds_send_config_msg() - Send config message to QMI server
 *  to QMI server
 * @psoc: PSOC handle
 * @src_info: source information to be filled in QMI message
 *
 * Return: QDF status
 */
QDF_STATUS
wlan_qmi_wfds_send_config_msg(struct wlan_objmgr_psoc *psoc,
			      struct wlan_qmi_wfds_config_req_msg *src_info);

/*
 * wlan_qmi_wfds_send_req_mem_msg() - Send Request Memory message to QMI server
 * @psoc: PSOC handle
 * @src_info: source information to be filled in QMI message
 *
 * Return: QDF status
 */
QDF_STATUS
wlan_qmi_wfds_send_req_mem_msg(struct wlan_objmgr_psoc *psoc,
			       struct wlan_qmi_wfds_mem_req_msg *src_info);

/*
 * wlan_qmi_wfds_ipcc_map_n_cfg_msg() - Send the IPCC map and configure message
 *  to QMI server
 * @psoc: PSOC handle
 * @src_info: source information to be filled in QMI message
 *
 * Return: QDF status
 */
QDF_STATUS
wlan_qmi_wfds_ipcc_map_n_cfg_msg(struct wlan_objmgr_psoc *psoc,
			struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg *src_info);

/*
 * wlan_qmi_wfds_send_misc_req_msg() - Send the misc req message
 *  to QMI server
 * @psoc: PSOC handle
 * @is_ssr: true if SSR is in progress else false
 *
 * Return: QDF status
 */
QDF_STATUS
wlan_qmi_wfds_send_misc_req_msg(struct wlan_objmgr_psoc *psoc, bool is_ssr);
#else
static inline
QDF_STATUS wlan_qmi_wfds_init(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
void wlan_qmi_wfds_deinit(struct wlan_objmgr_psoc *psoc)
{
}

static inline QDF_STATUS
wlan_qmi_wfds_send_config_msg(struct wlan_objmgr_psoc *psoc,
			      struct wlan_qmi_wfds_config_req_msg *src_info);
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_qmi_wfds_send_req_mem_msg(struct wlan_objmgr_psoc *psoc,
			       struct wlan_qmi_wfds_mem_req_msg *src_info);
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_qmi_wfds_ipcc_map_n_cfg_msg(struct wlan_objmgr_psoc *psoc,
			struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg *src_info)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_qmi_wfds_send_misc_req_msg(struct wlan_objmgr_psoc *psoc, bool is_ssr)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif
