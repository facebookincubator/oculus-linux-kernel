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
 * DOC: wlan_afc_ucfg_api.c
 *
 * This file has the AFC dispatcher API implementation which is exposed
 * to outside of AFC component.
 */

#include <wlan_afc_main.h>
#include <wlan_afc_ucfg_api.h>
#include <wlan_objmgr_global_obj.h>

QDF_STATUS ucfg_afc_register_data_send_cb(struct wlan_objmgr_psoc *psoc,
					  send_response_to_afcmem func)
{
	return wlan_afc_register_data_send_cb(psoc, func);
}

int ucfg_afc_data_send(struct wlan_objmgr_psoc *psoc,
		       struct wlan_objmgr_pdev *pdev,
		       struct wlan_afc_host_resp *data,
		       uint32_t len)
{
	return wlan_afc_data_send(psoc, pdev, data, len);
}

QDF_STATUS ucfg_afc_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_psoc_create_handler(WLAN_UMAC_COMP_AFC,
							  wlan_afc_psoc_created_notification,
							  NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		afc_err("Failed to register psoc create handler");
		goto fail_create_psoc;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(WLAN_UMAC_COMP_AFC,
							   wlan_afc_psoc_destroyed_notification,
							   NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		afc_err("Failed to register psoc delete handler");
		goto fail_psoc_destroy;
	}

	status = wlan_objmgr_register_pdev_create_handler(WLAN_UMAC_COMP_AFC,
							  wlan_afc_pdev_obj_create_handler,
							  NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		afc_err("Failed to register pdev create handler");
		goto fail_create_pdev;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(WLAN_UMAC_COMP_AFC,
							   wlan_afc_pdev_obj_destroy_handler,
							   NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		afc_err("Failed to register pdev delete handler");
		goto fail_pdev_destroy;
	}

	return status;

fail_pdev_destroy:
	wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_AFC,
						   wlan_afc_pdev_obj_create_handler,
						   NULL);
fail_create_pdev:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_AFC,
						    wlan_afc_psoc_destroyed_notification,
						    NULL);
fail_psoc_destroy:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_AFC,
						   wlan_afc_psoc_created_notification,
						   NULL);
fail_create_psoc:
	return status;
}

QDF_STATUS ucfg_afc_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_pdev_destroy_handler(WLAN_UMAC_COMP_AFC,
							     wlan_afc_pdev_obj_destroy_handler,
							     NULL);
	if (QDF_IS_STATUS_ERROR(status))
		afc_err("Failed to unregister pdev destroy handler");

	status = wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_AFC,
							    wlan_afc_pdev_obj_create_handler,
							    NULL);
	if (QDF_IS_STATUS_ERROR(status))
		afc_err("Failed to unregister pdev create handler");

	status = wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_AFC,
							     wlan_afc_psoc_destroyed_notification,
							     NULL);
	if (QDF_IS_STATUS_ERROR(status))
		afc_err("Failed to unregister psoc destroy handler");

	status = wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_AFC,
							    wlan_afc_psoc_created_notification,
							    NULL);
	if (QDF_IS_STATUS_ERROR(status))
		afc_err("Failed to unregister psoc create handler");

	return status;
}
