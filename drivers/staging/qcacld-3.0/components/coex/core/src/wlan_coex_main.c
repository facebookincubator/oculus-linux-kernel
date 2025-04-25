/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains definitions for coex core functions
 */

#include <wlan_coex_ucfg_api.h>
#include <wlan_coex_tgt_api.h>
#include <wlan_coex_main.h>

QDF_STATUS wlan_coex_psoc_created_notification(struct wlan_objmgr_psoc *psoc,
					       void *arg_list)
{
	struct coex_psoc_obj *psoc_obj;
	QDF_STATUS status;

	psoc_obj = qdf_mem_malloc(sizeof(*psoc_obj));
	if (!psoc_obj)
		return QDF_STATUS_E_NOMEM;

	psoc_obj->btc_chain_mode = WLAN_COEX_BTC_CHAIN_MODE_UNSETTLED;

	/* Attach scan private date to psoc */
	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						       WLAN_UMAC_COMP_COEX,
						       psoc_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		coex_err("Failed to attach psoc coex component");
		qdf_mem_free(psoc_obj);
	} else {
		coex_debug("Coex object attach to psoc successful");
	}

	return status;
}

QDF_STATUS wlan_coex_psoc_destroyed_notification(struct wlan_objmgr_psoc *psoc,
						 void *arg_list)
{
	void *psoc_obj;
	QDF_STATUS status;

	psoc_obj = wlan_psoc_get_coex_obj(psoc);
	if (!psoc_obj)
		return QDF_STATUS_E_FAILURE;

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_COEX,
						       psoc_obj);
	if (QDF_IS_STATUS_ERROR(status))
		coex_err("Failed to detach psoc coex component");

	qdf_mem_free(psoc_obj);

	return status;
}

QDF_STATUS
wlan_coex_psoc_init(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_coex_psoc_deinit(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_coex_config_send(struct wlan_objmgr_vdev *vdev,
				 struct coex_config_params *param)
{
	QDF_STATUS status;

	status = tgt_send_coex_config(vdev, param);
	if (QDF_IS_STATUS_ERROR(status))
		coex_err("failed to send coex config");

	return status;
}

QDF_STATUS wlan_coex_multi_config_send(struct wlan_objmgr_vdev *vdev,
				       struct coex_multi_config *param)
{
	struct wlan_objmgr_psoc *psoc;
	struct coex_config_params one_param;
	QDF_STATUS status, ret = QDF_STATUS_SUCCESS;
	uint32_t i;

	if (!vdev) {
		coex_err("Null vdev");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		coex_err("failed to get coex_obj");
		return QDF_STATUS_E_INVAL;
	}

	if (tgt_get_coex_multi_config_support(psoc))
		return tgt_send_coex_multi_config(vdev, param);

	for (i = 0; i < param->num_configs; i++) {
		one_param.vdev_id = param->vdev_id;
		one_param.config_type = param->cfg_items[i].config_type;
		one_param.config_arg1 = param->cfg_items[i].config_arg1;
		one_param.config_arg2 = param->cfg_items[i].config_arg2;
		one_param.config_arg3 = param->cfg_items[i].config_arg3;
		one_param.config_arg4 = param->cfg_items[i].config_arg4;
		one_param.config_arg5 = param->cfg_items[i].config_arg5;
		one_param.config_arg6 = param->cfg_items[i].config_arg6;
		status = tgt_send_coex_config(vdev, &one_param);
		if (QDF_IS_STATUS_ERROR(status)) {
			coex_err("fail to send one coex config");
			ret = status;
		}
	}

	return ret;
}

QDF_STATUS
wlan_coex_config_updated(struct wlan_objmgr_vdev *vdev, uint8_t type)
{
	struct wlan_objmgr_psoc *psoc;
	struct coex_psoc_obj *coex_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev) {
		coex_err("NULL vdev");
		return QDF_STATUS_E_INVAL;
	}

	if (type >= COEX_CONFIG_TYPE_MAX) {
		coex_err("config type out of range: %d", type);
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		coex_err("NULL psoc");
		return QDF_STATUS_E_INVAL;
	}

	coex_obj = wlan_psoc_get_coex_obj(psoc);
	if (!coex_obj)
		return QDF_STATUS_E_INVAL;

	if (coex_obj->coex_config_updated[type])
		status = coex_obj->coex_config_updated[type](vdev);

	return status;
}

QDF_STATUS
wlan_coex_psoc_set_btc_chain_mode(struct wlan_objmgr_psoc *psoc,
				  enum coex_btc_chain_mode val)
{
	struct coex_psoc_obj *coex_obj;

	coex_obj = wlan_psoc_get_coex_obj(psoc);
	if (!coex_obj)
		return QDF_STATUS_E_INVAL;

	coex_obj->btc_chain_mode = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_coex_psoc_get_btc_chain_mode(struct wlan_objmgr_psoc *psoc,
				  enum coex_btc_chain_mode *val)
{
	struct coex_psoc_obj *coex_obj;

	if (!val) {
		coex_err("invalid param for getting btc chain mode");
		return QDF_STATUS_E_INVAL;
	}

	coex_obj = wlan_psoc_get_coex_obj(psoc);
	if (!coex_obj)
		return QDF_STATUS_E_INVAL;

	*val = coex_obj->btc_chain_mode;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_DBAM_CONFIG
QDF_STATUS wlan_dbam_config_send(struct wlan_objmgr_vdev *vdev,
				 struct coex_dbam_config_params *param)
{
	QDF_STATUS status;

	status = tgt_send_dbam_config(vdev, param);
	if (QDF_IS_STATUS_ERROR(status))
		coex_err("failed to send dbam config");

	return status;
}

QDF_STATUS wlan_dbam_attach(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_dbam_tx_ops *dbam_tx_ops;

	if (!psoc) {
		coex_err("psoc is Null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	dbam_tx_ops = wlan_psoc_get_dbam_tx_ops(psoc);
	if (!dbam_tx_ops) {
		coex_err("dbam_tx_ops is Null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!dbam_tx_ops->dbam_event_attach) {
		coex_err("dbam_event_attach function is Null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return dbam_tx_ops->dbam_event_attach(psoc);
}

QDF_STATUS wlan_dbam_detach(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_dbam_tx_ops *dbam_tx_ops;

	if (!psoc) {
		coex_err("psoc is Null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	dbam_tx_ops = wlan_psoc_get_dbam_tx_ops(psoc);
	if (!dbam_tx_ops) {
		coex_err("dbam_tx_ops is Null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!dbam_tx_ops->dbam_event_detach) {
		coex_err("dbam_event_detach function is Null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return dbam_tx_ops->dbam_event_detach(psoc);
}
#endif
