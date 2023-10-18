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
 * DOC: wlan_qmi_ucfg_api.h
 *
 * QMI component north bound interface
 */

#ifndef _WLAN_QMI_UCFG_API_H_
#define _WLAN_QMI_UCFG_API_H_

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_qmi_public_struct.h>

#ifdef QMI_COMPONENT_ENABLE
/**
 * ucfg_qmi_init() - API to init QMI component
 *
 * This API is invoked from hdd_component_init during all component init.
 * This API will register all required handlers for psoc object
 * create/delete notification.
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_qmi_init(void);

/**
 * ucfg_qmi_deinit() - API to deinit QMI component
 *
 * This API is invoked from hdd_component_deinit during all component deinit.
 * This API will unregister all required handlers for psoc object
 * create/delete notification.
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_qmi_deinit(void);

/**
 * ucfg_qmi_register_os_if_callbacks() - API to register os if callbacks with
 *  QMI component
 * @psoc: PSOC handle
 * @cb_obj: callback object
 *
 * Return: None
 */
void ucfg_qmi_register_os_if_callbacks(struct wlan_objmgr_psoc *psoc,
				       struct wlan_qmi_psoc_callbacks *cb_obj);
#else
static inline QDF_STATUS ucfg_qmi_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_qmi_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_qmi_register_os_if_callbacks(struct wlan_objmgr_psoc *psoc,
				       struct wlan_qmi_psoc_callbacks *cb_obj)
{
}
#endif
#endif
