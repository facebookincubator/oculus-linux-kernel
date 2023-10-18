/*
 * Copyright (c) 2012-2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: Public API implementation of action_oui called by north bound HDD/OSIF.
 */

#include "wlan_action_oui_ucfg_api.h"
#include "wlan_action_oui_main.h"
#include "target_if_action_oui.h"
#include "wlan_action_oui_tgt_api.h"
#include <qdf_str.h>

QDF_STATUS ucfg_action_oui_init(void)
{
	QDF_STATUS status;

	ACTION_OUI_ENTER();

	status = wlan_objmgr_register_psoc_create_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_create_notification, NULL);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_err("Failed to register psoc create handler");
		goto exit;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_destroy_notification, NULL);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_debug("psoc create/delete notifications registered");
		goto exit;
	}

	action_oui_err("Failed to register psoc delete handler");
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_ACTION_OUI,
			action_oui_psoc_create_notification, NULL);

exit:
	ACTION_OUI_EXIT();
	return status;
}

void ucfg_action_oui_deinit(void)
{
	QDF_STATUS status;

	ACTION_OUI_ENTER();

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_create_notification, NULL);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_err("Failed to unregister psoc create handler");

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_destroy_notification,
				NULL);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_err("Failed to unregister psoc delete handler");

	ACTION_OUI_EXIT();
}

void ucfg_action_oui_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	action_oui_psoc_enable(psoc);
}

void ucfg_action_oui_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	action_oui_psoc_disable(psoc);
}

bool ucfg_action_oui_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct action_oui_psoc_priv *psoc_priv;

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		return false;
	}

	return psoc_priv->action_oui_enable;
}

uint8_t *
ucfg_action_oui_get_config(struct wlan_objmgr_psoc *psoc,
			   enum action_oui_id action_id)
{
	struct action_oui_psoc_priv *psoc_priv;

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		return "";
	}
	if (action_id >= ACTION_OUI_MAXIMUM_ID) {
		action_oui_err("Invalid action_oui id: %u", action_id);
		return "";
	}

	return psoc_priv->action_oui_str[action_id];
}


QDF_STATUS
ucfg_action_oui_parse(struct wlan_objmgr_psoc *psoc,
		      const uint8_t *in_str,
		      enum action_oui_id action_id)
{
	return action_oui_parse_string(psoc, in_str, action_id);
}

QDF_STATUS
ucfg_action_oui_cleanup(struct wlan_objmgr_psoc *psoc,
			enum action_oui_id action_id)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	ACTION_OUI_ENTER();

	if (action_id >= ACTION_OUI_MAXIMUM_ID) {
		action_oui_err("Invalid action_oui id: %u", action_id);
		goto exit;
	}

	if (!psoc) {
		action_oui_err("psoc is NULL");
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	status = wlan_action_oui_cleanup(psoc_priv, action_id);
exit:
	ACTION_OUI_EXIT();
	return status;
}

QDF_STATUS ucfg_action_oui_send(struct wlan_objmgr_psoc *psoc)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t id;

	if (!psoc) {
		action_oui_err("psoc is NULL");
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	for (id = 0; id < ACTION_OUI_MAXIMUM_ID; id++) {
		if (id >= ACTION_OUI_HOST_ONLY)
			continue;
		status = action_oui_send(psoc_priv, id);
		if (!QDF_IS_STATUS_SUCCESS(status))
			action_oui_debug("Failed to send: %u", id);
	}

exit:
	return status;
}

QDF_STATUS ucfg_action_oui_send_by_id(struct wlan_objmgr_psoc *psoc,
				      enum action_oui_id id)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	ACTION_OUI_ENTER();

	if (!psoc) {
		action_oui_err("psoc is NULL");
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	if (id >= ACTION_OUI_HOST_ONLY) {
		action_oui_err("id %d not for firmware", id);
		status = QDF_STATUS_SUCCESS;
		goto exit;
	}

	status = action_oui_send(psoc_priv, id);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_debug("Failed to send: %u", id);
exit:
	ACTION_OUI_EXIT();

	return status;
}

bool ucfg_action_oui_search(struct wlan_objmgr_psoc *psoc,
			    struct action_oui_search_attr *attr,
			    enum action_oui_id action_id)
{
	return wlan_action_oui_search(psoc, attr, action_id);
}
