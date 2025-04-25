/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_afc_main.h
 *
 * This Head file provides declaration for UMAC AFC common APIs.
 */

#ifndef __WLAN_AFC_MAIN_H__
#define __WLAN_AFC_MAIN_H__

#include <qdf_trace.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <reg_services_public_struct.h>

#ifdef CONFIG_AFC_SUPPORT

#define afc_alert(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_AFC, params)
#define afc_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_AFC, params)
#define afc_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_AFC, params)
#define afc_notice(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_AFC, params)
#define afc_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_AFC, params)
#define afc_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_AFC, params)

/**
 * afc_psoc_priv() - Helper function to get AFC private object of PSOC
 * @psoc: Pointer to PSOC object
 *
 * Return: AFC private object of PSOC
 */
static inline struct wlan_afc_psoc_priv *
afc_psoc_priv(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_afc_psoc_priv *afc_priv;

	afc_priv = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							 WLAN_UMAC_COMP_AFC);
	return afc_priv;
}

/**
 * typedef send_response_to_afcmem() - Function prototype of send AFC response
 * to share memory.
 * @psoc: Pointer to PSOC object
 * @pdev: Pointer to PDEV object
 * @data: AFC response data
 * @len: AFC response length
 *
 * Return: 0 if success, otherwise failure
 */
typedef int (*send_response_to_afcmem)(struct wlan_objmgr_psoc *psoc,
				       struct wlan_objmgr_pdev *pdev,
				       struct wlan_afc_host_resp *data,
				       uint32_t len);

/**
 * typedef osif_send_afc_request() - Function prototype of sending
 * AFC request to osif.
 * @pdev: Pointer to PDEV object
 * @afc_req: Pointer to AFC request
 *
 * Return: 0 if success, otherwise failure
 */
typedef int
(*osif_send_afc_request)(struct wlan_objmgr_pdev *pdev,
			 struct wlan_afc_host_request *afc_req);

/**
 * typedef osif_send_afc_power_update_complete() - Function prototype of sending
 * AFC update complete event to osif.
 * @pdev: Pointer to PDEV object
 * @afc_pwr_evt: Pointer to AFC power update event
 *
 * Return: 0 if success, otherwise failure
 */
typedef int
(*osif_send_afc_power_update_complete)(struct wlan_objmgr_pdev *pdev,
				       struct reg_fw_afc_power_event *afc_pwr_evt);

/**
 * wlan_afc_data_send() - Function to send AFC response through platform
 * interface
 * @psoc: Pointer to PSOC object
 * @pdev: Pointer to PDEV object
 * @data: Pointer to AFC response data
 * @len: AFC response data length in bytes
 *
 * Return: 0 if success, otherwise failure
 */
int wlan_afc_data_send(struct wlan_objmgr_psoc *psoc,
		       struct wlan_objmgr_pdev *pdev,
		       struct wlan_afc_host_resp *data,
		       uint32_t len);

/**
 * wlan_afc_register_data_send_cb() - Function to register data send AFC
 * callback
 * @psoc: Pointer to PSOC object
 * @func: AFC PLD function to be register
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_afc_register_data_send_cb(struct wlan_objmgr_psoc *psoc,
					  send_response_to_afcmem func);

/**
 * wlan_afc_psoc_created_notification() - Callback function to be invoked when
 * POSC create AFC component.
 * @psoc: Pointer to PSOC object
 * @arg: Pointer to argument list
 *
 * Return: QDF STATUS
 */
QDF_STATUS
wlan_afc_psoc_created_notification(struct wlan_objmgr_psoc *psoc,
				   void *arg);

/**
 * wlan_afc_psoc_destroyed_notification() - Callback function to be invoked when
 * PSOC destroy AFC component.
 * @psoc: Pointer to PSOC object
 * @arg: Pointer to argument list
 *
 * Return: QDF STATUS
 */
QDF_STATUS
wlan_afc_psoc_destroyed_notification(struct wlan_objmgr_psoc *psoc,
				     void *arg);

/**
 * wlan_afc_pdev_obj_create_handler() - Callback function of PDEV create AFC
 * component.
 * @pdev: Pointer to PDEV object
 * @arg: Pointer to argument list
 *
 * Return: QDF STATUS
 */
QDF_STATUS
wlan_afc_pdev_obj_create_handler(struct wlan_objmgr_pdev *pdev,
				 void *arg);

/**
 * wlan_afc_pdev_obj_destroy_handler() - Callback function of PDEV destroy AFC
 * component.
 * @pdev: Pointer to PDEV object
 * @arg: Pointer to argument list
 *
 * Return: QDF STATUS
 */
QDF_STATUS
wlan_afc_pdev_obj_destroy_handler(struct wlan_objmgr_pdev *pdev, void *arg);
#endif
#endif
