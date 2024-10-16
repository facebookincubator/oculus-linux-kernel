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
 * DOC: wlan_qmi_main.h
 *
 * QMI component core function declarations
 */

#ifndef _WLAN_QMI_MAIN_H_
#define _WLAN_QMI_MAIN_H_

#include "wlan_objmgr_psoc_obj.h"
#include <qdf_trace.h>

#define qmi_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_QMI, params)
#define qmi_debug_rl(params...) \
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_QMI, params)

#define qmi_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_QMI, params)
#define qmi_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_QMI, params)
#define qmi_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_QMI, params)
#define qmi_fatal(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_QMI, params)

#define qmi_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_QMI, params)
#define qmi_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_QMI, params)
#define qmi_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_QMI, params)
#define qmi_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_QMI, params)
#define qmi_nofl_fatal(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_QMI, params)

/**
 * qmi_psoc_obj_create_notification() - Function to allocate per psoc QMI
 *  private object
 * @psoc: psoc context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when psoc is being
 * created and creates QMI soc context.
 *
 * Return: QDF status
 */
QDF_STATUS
qmi_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * qmi_psoc_obj_destroy_notification() - Free per psoc QMI private object
 * @psoc: psoc context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when psoc is being
 * deleted and delete QMI soc context.
 *
 * Return: QDF status
 */
QDF_STATUS
qmi_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg);
#endif
