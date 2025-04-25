/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Implement various notification handlers which are accessed
 * internally in action_oui component only.
 */
#include "cfg_ucfg_api.h"
#include "wlan_action_oui_cfg.h"
#include "wlan_action_oui_main.h"
#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_tgt_api.h"
#include "target_if_action_oui.h"

/**
 * action_oui_allocate() - Allocates memory for various actions.
 * @psoc_priv: pointer to action_oui psoc priv obj
 *
 * This function allocates memory for all the action_oui types
 * and initializes the respective lists to store extensions
 * extracted from action_oui_extract().
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
action_oui_allocate(struct action_oui_psoc_priv *psoc_priv)
{
	struct action_oui_priv *oui_priv;
	uint32_t i;
	uint32_t j;

	for (i = 0; i < ACTION_OUI_MAXIMUM_ID; i++) {
		oui_priv = qdf_mem_malloc(sizeof(*oui_priv));
		if (!oui_priv) {
			action_oui_err("Mem alloc failed for oui_priv id: %u",
					i);
			goto free_mem;
		}
		oui_priv->id = i;
		qdf_list_create(&oui_priv->extension_list,
				ACTION_OUI_MAX_EXTENSIONS);
		qdf_mutex_create(&oui_priv->extension_lock);
		psoc_priv->oui_priv[i] = oui_priv;
	}

	return QDF_STATUS_SUCCESS;

free_mem:
	for (j = 0; j < i; j++) {
		oui_priv = psoc_priv->oui_priv[j];
		if (!oui_priv)
			continue;

		qdf_list_destroy(&oui_priv->extension_list);
		qdf_mutex_destroy(&oui_priv->extension_lock);
		psoc_priv->oui_priv[j] = NULL;
	}

	return QDF_STATUS_E_NOMEM;
}

/**
 * action_oui_destroy() - Deallocates memory for various actions.
 * @psoc_priv: pointer to action_oui psoc priv obj
 *
 * This function Deallocates memory for all the action_oui types.
 * As a part of deallocate, all extensions are destroyed.
 *
 * Return: None
 */
static void
action_oui_destroy(struct action_oui_psoc_priv *psoc_priv)
{
	struct action_oui_priv *oui_priv;
	struct action_oui_extension_priv *ext_priv;
	qdf_list_t *ext_list;
	QDF_STATUS status;
	qdf_list_node_t *node = NULL;
	uint32_t i;

	psoc_priv->total_extensions = 0;
	psoc_priv->max_extensions = 0;
	psoc_priv->host_only_extensions = 0;

	for (i = 0; i < ACTION_OUI_MAXIMUM_ID; i++) {
		oui_priv = psoc_priv->oui_priv[i];
		psoc_priv->oui_priv[i] = NULL;
		if (!oui_priv)
			continue;

		ext_list = &oui_priv->extension_list;
		qdf_mutex_acquire(&oui_priv->extension_lock);
		while (!qdf_list_empty(ext_list)) {
			status = qdf_list_remove_front(ext_list, &node);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				action_oui_err("Invalid delete in action: %u",
						oui_priv->id);
				break;
			}
			ext_priv = qdf_container_of(node,
					struct action_oui_extension_priv,
					item);
			qdf_mem_free(ext_priv);
			ext_priv = NULL;
		}

		qdf_list_destroy(ext_list);
		qdf_mutex_release(&oui_priv->extension_lock);
		qdf_mutex_destroy(&oui_priv->extension_lock);
		qdf_mem_free(oui_priv);
		oui_priv = NULL;
	}
}

static void action_oui_load_config(struct action_oui_psoc_priv *psoc_priv)
{
	struct wlan_objmgr_psoc *psoc = psoc_priv->psoc;

	psoc_priv->action_oui_enable =
		cfg_get(psoc, CFG_ENABLE_ACTION_OUI);

	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_CONNECT_1X1],
		      cfg_get(psoc, CFG_ACTION_OUI_CONNECT_1X1),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_ITO_EXTENSION],
		      cfg_get(psoc, CFG_ACTION_OUI_ITO_EXTENSION),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_CCKM_1X1],
		      cfg_get(psoc, CFG_ACTION_OUI_CCKM_1X1),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_ITO_ALTERNATE],
		      cfg_get(psoc, CFG_ACTION_OUI_ITO_ALTERNATE),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_SWITCH_TO_11N_MODE],
		      cfg_get(psoc, CFG_ACTION_OUI_SWITCH_TO_11N_MODE),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN],
		      cfg_get(psoc,
			      CFG_ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_DISABLE_AGGRESSIVE_TX],
		      cfg_get(psoc,
			      CFG_ACTION_OUI_DISABLE_AGGRESSIVE_TX),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str
					  [ACTION_OUI_DISABLE_AGGRESSIVE_EDCA],
		      cfg_get(psoc,
			      CFG_ACTION_OUI_DISABLE_AGGRESSIVE_EDCA),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_EXTEND_WOW_ITO],
		      cfg_get(psoc, CFG_ACTION_OUI_EXTEND_WOW_ITO),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_DISABLE_TWT],
		      cfg_get(psoc, CFG_ACTION_OUI_DISABLE_TWT),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_HOST_RECONN],
		      cfg_get(psoc, CFG_ACTION_OUI_RECONN_ASSOCTIMEOUT),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_TAKE_ALL_BAND_INFO],
		      cfg_get(psoc, CFG_ACTION_OUI_TAKE_ALL_BAND_INFO),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_11BE_OUI_ALLOW],
		      cfg_get(psoc, CFG_ACTION_OUI_11BE_ALLOW_LIST),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str
			[ACTION_OUI_DISABLE_DYNAMIC_QOS_NULL_TX_RATE],
		      cfg_get(psoc,
			      CFG_ACTION_OUI_DISABLE_DYNAMIC_QOS_NULL_TX_RATE),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str
			[ACTION_OUI_ENABLE_CTS2SELF_WITH_QOS_NULL],
		      cfg_get(psoc,
			      CFG_ACTION_OUI_ENABLE_CTS2SELF_WITH_QOS_NULL),
		      ACTION_OUI_MAX_STR_LEN);

	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_ENABLE_CTS2SELF],
		      cfg_get(psoc, CFG_ACTION_OUI_ENABLE_CTS2SELF),
		      ACTION_OUI_MAX_STR_LEN);

	qdf_str_lcopy(psoc_priv->action_oui_str
			[ACTION_OUI_SEND_SMPS_FRAME_WITH_OMN],
		      cfg_get(psoc,
			      CFG_ACTION_OUI_SEND_SMPS_FRAME_WITH_OMN),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str
			[ACTION_OUI_RESTRICT_MAX_MLO_LINKS],
		      cfg_get(psoc, CFG_ACTION_OUI_RESTRICT_MAX_MLO_LINKS),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str
			[ACTION_OUI_AUTH_ASSOC_6MBPS_2GHZ],
		      cfg_get(psoc, CFG_ACTION_OUI_AUTH_ASSOC_6MBPS_2GHZ),
		      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_DISABLE_BFORMEE],
		      cfg_get(psoc, CFG_ACTION_OUI_DISABLE_BFORMEE),
			      ACTION_OUI_MAX_STR_LEN);
	qdf_str_lcopy(psoc_priv->action_oui_str[ACTION_OUI_LIMIT_BW],
		      cfg_get(psoc, CFG_ACTION_OUI_LIMIT_BW),
			      ACTION_OUI_MAX_STR_LEN);
}

static void action_oui_parse_config(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	uint32_t id;
	uint8_t *str;
	struct action_oui_psoc_priv *psoc_priv;

	if (!psoc) {
		action_oui_err("Invalid psoc");
		return;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		return;
	}
	if (!psoc_priv->action_oui_enable) {
		action_oui_debug("action_oui is not enable");
		return;
	}
	for (id = 0; id < ACTION_OUI_MAXIMUM_ID; id++) {
		str = psoc_priv->action_oui_str[id];
		if (!qdf_str_len(str))
			continue;

		status = action_oui_parse_string(psoc, str, id);
		if (!QDF_IS_STATUS_SUCCESS(status))
			action_oui_err("Failed to parse action_oui str: %u",
				       id);
	}

	/* FW allocates memory for the extensions only during init time.
	 * Therefore, send additional legspace for configuring new
	 * extensions during runtime.
	 * The current max value is default extensions count + 10.
	 */
	psoc_priv->max_extensions = psoc_priv->total_extensions -
					psoc_priv->host_only_extensions +
					ACTION_OUI_MAX_ADDNL_EXTENSIONS;
	action_oui_debug("Extensions - Max: %d Total: %d host_only %d",
			 psoc_priv->max_extensions, psoc_priv->total_extensions,
			 psoc_priv->host_only_extensions);
}

static QDF_STATUS action_oui_send_config(struct wlan_objmgr_psoc *psoc)
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
	if (!psoc_priv->action_oui_enable) {
		action_oui_debug("action_oui is not enable");
		return QDF_STATUS_SUCCESS;
	}

	for (id = 0; id < ACTION_OUI_MAXIMUM_ID; id++) {
		if (id >= ACTION_OUI_HOST_ONLY)
			continue;
		status = action_oui_send(psoc_priv, id);
		if (!QDF_IS_STATUS_SUCCESS(status))
			action_oui_err("Failed to send: %u", id);
	}

exit:
	return status;
}

QDF_STATUS
action_oui_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status;

	ACTION_OUI_ENTER();

	psoc_priv = qdf_mem_malloc(sizeof(*psoc_priv));
	if (!psoc_priv) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
				WLAN_UMAC_COMP_ACTION_OUI,
				(void *)psoc_priv, QDF_STATUS_SUCCESS);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_err("Failed to attach priv with psoc");
		goto free_psoc_priv;
	}

	target_if_action_oui_register_tx_ops(&psoc_priv->tx_ops);
	psoc_priv->psoc = psoc;
	action_oui_load_config(psoc_priv);
	action_oui_debug("psoc priv attached");
	goto exit;
free_psoc_priv:
	qdf_mem_free(psoc_priv);
	status = QDF_STATUS_E_INVAL;
exit:
	ACTION_OUI_EXIT();
	return status;
}

QDF_STATUS
action_oui_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct action_oui_psoc_priv *psoc_priv = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	ACTION_OUI_ENTER();

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
					WLAN_UMAC_COMP_ACTION_OUI,
					(void *)psoc_priv);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_err("Failed to detach priv with psoc");

	qdf_mem_free(psoc_priv);

exit:
	ACTION_OUI_EXIT();
	return status;
}

void action_oui_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	ACTION_OUI_ENTER();

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	status = action_oui_allocate(psoc_priv);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_err("Failed to alloc action_oui");
		goto exit;
	}
	action_oui_parse_config(psoc);
	action_oui_send_config(psoc);
exit:
	ACTION_OUI_EXIT();
}

void action_oui_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	struct action_oui_psoc_priv *psoc_priv;

	ACTION_OUI_ENTER();

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	action_oui_destroy(psoc_priv);
exit:
	ACTION_OUI_EXIT();
}

bool wlan_action_oui_search(struct wlan_objmgr_psoc *psoc,
			    struct action_oui_search_attr *attr,
			    enum action_oui_id action_id)
{
	struct action_oui_psoc_priv *psoc_priv;
	bool found = false;

	if (!psoc || !attr) {
		action_oui_err("Invalid psoc or search attrs");
		goto exit;
	}

	if (action_id >= ACTION_OUI_MAXIMUM_ID) {
		action_oui_err("Invalid action_oui id: %u", action_id);
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	found = action_oui_search(psoc_priv, attr, action_id);

exit:
	return found;
}

QDF_STATUS
wlan_action_oui_cleanup(struct action_oui_psoc_priv *psoc_priv,
			enum action_oui_id action_id)
{
	struct action_oui_priv *oui_priv;
	struct action_oui_extension_priv *ext_priv;
	qdf_list_t *ext_list;
	QDF_STATUS status;
	qdf_list_node_t *node = NULL;

	if (action_id >= ACTION_OUI_MAXIMUM_ID)
		return QDF_STATUS_E_INVAL;

	oui_priv = psoc_priv->oui_priv[action_id];
	if (!oui_priv)
		return QDF_STATUS_SUCCESS;

	ext_list = &oui_priv->extension_list;
	qdf_mutex_acquire(&oui_priv->extension_lock);
	while (!qdf_list_empty(ext_list)) {
		status = qdf_list_remove_front(ext_list, &node);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			action_oui_err("Invalid delete in action: %u",
				       oui_priv->id);
			break;
		}
		ext_priv = qdf_container_of(
				node,
				struct action_oui_extension_priv,
				item);
		qdf_mem_free(ext_priv);
		ext_priv = NULL;
		if (psoc_priv->total_extensions)
			psoc_priv->total_extensions--;
		else
			action_oui_err("unexpected total_extensions 0");

		if (action_id >= ACTION_OUI_HOST_ONLY) {
			if (!psoc_priv->host_only_extensions)
				action_oui_err("unexpected total host extensions");
			else
				psoc_priv->host_only_extensions--;
		}
	}
	qdf_mutex_release(&oui_priv->extension_lock);

	return QDF_STATUS_SUCCESS;
}

bool wlan_action_oui_is_empty(struct wlan_objmgr_psoc *psoc,
			      enum action_oui_id action_id)
{
	struct action_oui_psoc_priv *psoc_priv;
	bool empty = true;

	if (!psoc) {
		action_oui_err("Invalid psoc");
		goto exit;
	}

	if (action_id >= ACTION_OUI_MAXIMUM_ID) {
		action_oui_err("Invalid action_oui id: %u", action_id);
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	empty = action_oui_is_empty(psoc_priv, action_id);

exit:
	return empty;
}

