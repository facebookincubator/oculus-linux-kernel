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
 * DOC: wlan_afc_main.c
 * This file provides implementation for UMAC AFC common APIs.
 */

#include <wlan_afc_main.h>
#include "wlan_afc_priv.h"
#include "wlan_reg_ucfg_api.h"
#include "wlan_cfg80211_afc.h"

/**
 * wlan_send_afc_request() - PDEV callback function to send AFC request
 * @pdev: Pointer to PDEV object
 * @afc_req: Pointer to AFC request from regulatory component
 * @arg: Pointer to argument list of callback function
 *
 * Return: None
 */
static void
wlan_send_afc_request(struct wlan_objmgr_pdev *pdev,
		      struct wlan_afc_host_request *afc_req,
		      void *arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_afc_psoc_priv *afc_priv;

	psoc = wlan_pdev_get_psoc(pdev);

	afc_priv = afc_psoc_priv(psoc);
	if (!afc_priv) {
		afc_err("psoc AFC private null");
		return;
	}

	if (afc_priv->cbs.afc_req_func)
		afc_priv->cbs.afc_req_func(pdev, afc_req);
}

/**
 * wlan_send_afc_power_event() - PDEV callback function to send AFC power
 * update complete event.
 * @pdev: Pointer to PDEV object
 * @afc_pwr_evt: Pointer to AFC power event from regulatory component
 * @arg: Pointer to argument list of callback function
 *
 * Return: None
 */
static void
wlan_send_afc_power_event(struct wlan_objmgr_pdev *pdev,
			  struct reg_fw_afc_power_event *afc_pwr_evt,
			  void *arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_afc_psoc_priv *afc_priv;

	psoc = wlan_pdev_get_psoc(pdev);

	afc_priv = afc_psoc_priv(psoc);
	if (!afc_priv) {
		afc_err("psoc AFC private null");
		return;
	}

	if (afc_priv->cbs.afc_updated_func)
		afc_priv->cbs.afc_updated_func(pdev, afc_pwr_evt);
}

int wlan_afc_data_send(struct wlan_objmgr_psoc *psoc,
		       struct wlan_objmgr_pdev *pdev,
		       struct wlan_afc_host_resp *data,
		       uint32_t len)
{
	struct wlan_afc_psoc_priv *afc_priv;

	afc_priv = afc_psoc_priv(psoc);
	if (!afc_priv) {
		afc_err("psoc AFC private null");
		return -EINVAL;
	}

	if (afc_priv->cbs.afc_rsp_cp_func)
		return afc_priv->cbs.afc_rsp_cp_func(psoc, pdev, data, len);

	return -EINVAL;
}

QDF_STATUS wlan_afc_register_data_send_cb(struct wlan_objmgr_psoc *psoc,
					  send_response_to_afcmem func)
{
	struct wlan_afc_psoc_priv *afc_priv;

	afc_priv = afc_psoc_priv(psoc);
	if (!afc_priv) {
		afc_err("psoc AFC private null");
		return QDF_STATUS_E_FAILURE;
	}

	afc_priv->cbs.afc_rsp_cp_func = func;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_afc_psoc_created_notification(struct wlan_objmgr_psoc *psoc,
				   void *arg)
{
	QDF_STATUS status;
	struct wlan_afc_psoc_priv *afc_priv;

	afc_priv = qdf_mem_malloc(sizeof(*afc_priv));
	if (!afc_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						       WLAN_UMAC_COMP_AFC,
						       afc_priv,
						       QDF_STATUS_SUCCESS);

	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(afc_priv);
		afc_err("Failed to attach psoc AFC component");
		return status;
	}

	afc_priv->psoc = psoc;
	afc_priv->cbs.afc_req_func = wlan_cfg80211_afc_send_request;
	afc_priv->cbs.afc_updated_func = wlan_cfg80211_afc_send_update_complete;

	return status;
}

QDF_STATUS
wlan_afc_psoc_destroyed_notification(struct wlan_objmgr_psoc *psoc,
				     void *arg)
{
	QDF_STATUS status;
	void *afc_priv;

	afc_priv = afc_psoc_priv(psoc);
	if (!afc_priv) {
		afc_err("Failed to get psoc AFC component private");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_AFC,
						       afc_priv);
	if (QDF_IS_STATUS_ERROR(status))
		afc_err("Failed to detach AFC component");

	qdf_mem_free(afc_priv);

	return status;
}

QDF_STATUS
wlan_afc_pdev_obj_create_handler(struct wlan_objmgr_pdev *pdev,
				 void *arg)
{
	QDF_STATUS status;

	status = ucfg_reg_register_afc_req_rx_callback(pdev,
						       wlan_send_afc_request,
						       NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		afc_err("Failed to register AFC request callback");
		return status;
	}

	status = ucfg_reg_register_afc_power_event_callback(pdev,
							    wlan_send_afc_power_event,
							    NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		afc_err("Failed to register AFC power callback");
		ucfg_reg_unregister_afc_req_rx_callback(pdev,
							wlan_send_afc_request);
		return status;
	}

	return status;
}

QDF_STATUS
wlan_afc_pdev_obj_destroy_handler(struct wlan_objmgr_pdev *pdev, void *arg)
{
	ucfg_reg_unregister_afc_req_rx_callback(pdev,
						wlan_send_afc_request);
	ucfg_reg_unregister_afc_power_event_callback(pdev,
						     wlan_send_afc_power_event);

	return QDF_STATUS_SUCCESS;
}
