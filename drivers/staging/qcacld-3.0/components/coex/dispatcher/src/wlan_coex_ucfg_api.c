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
 * DOC: contains coex north bound interface definitions
 */

#include <wlan_coex_main.h>
#include <wlan_coex_ucfg_api.h>
#include "wmi_unified.h"
#include "wlan_coex_public_structs.h"
#include "wlan_coex_tgt_api.h"

QDF_STATUS
ucfg_coex_register_cfg_updated_handler(struct wlan_objmgr_psoc *psoc,
				       enum coex_config_type type,
				       update_coex_cb handler)
{
	struct coex_psoc_obj *coex_obj;

	if (type >= COEX_CONFIG_TYPE_MAX) {
		coex_err("invalid coex type: %d", type);
		return QDF_STATUS_E_INVAL;
	}

	coex_obj = wlan_psoc_get_coex_obj(psoc);
	if (!coex_obj)
		return QDF_STATUS_E_INVAL;

	coex_obj->coex_config_updated[type] = handler;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_coex_psoc_set_btc_chain_mode(struct wlan_objmgr_psoc *psoc,
				  enum coex_btc_chain_mode val)
{
	return wlan_coex_psoc_set_btc_chain_mode(psoc, val);
}

QDF_STATUS
ucfg_coex_psoc_get_btc_chain_mode(struct wlan_objmgr_psoc *psoc,
				  enum coex_btc_chain_mode *val)
{
	return wlan_coex_psoc_get_btc_chain_mode(psoc, val);
}

QDF_STATUS
ucfg_coex_send_btc_chain_mode(struct wlan_objmgr_vdev *vdev,
			      enum coex_btc_chain_mode mode)
{
	struct coex_config_params param = {0};

	if (mode > WLAN_COEX_BTC_CHAIN_MODE_HYBRID)
		return QDF_STATUS_E_INVAL;

	param.vdev_id = wlan_vdev_get_id(vdev);
	param.config_type = WMI_COEX_CONFIG_BTCOEX_SEPARATE_CHAIN_MODE;
	param.config_arg1 = mode;

	coex_debug("send btc chain mode %d for vdev %d", mode, param.vdev_id);

	return wlan_coex_config_send(vdev, &param);
}

QDF_STATUS
ucfg_coex_send_multi_config(struct wlan_objmgr_vdev *vdev,
			    struct coex_multi_config *param)
{
	return wlan_coex_multi_config_send(vdev, param);
}

#ifdef WLAN_FEATURE_DBAM_CONFIG
QDF_STATUS
ucfg_coex_send_dbam_config(struct wlan_objmgr_vdev *vdev,
			   struct coex_dbam_config_params *param,
			   void (*clbk)(void *ctx,
			   enum coex_dbam_comp_status *rsp),
			   void *context)
{
	struct wlan_objmgr_psoc *psoc;
	struct coex_psoc_obj *coex_obj;
	struct wlan_coex_callback *cbk;

	if (!vdev) {
		coex_err("Null vdev");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		coex_err("psoc is null");
		return QDF_STATUS_E_INVAL;
	}

	coex_obj = wlan_psoc_get_coex_obj(psoc);
	if (!coex_obj) {
		coex_err("failed to get coex_obj");
		return QDF_STATUS_E_INVAL;
	}

	cbk = &coex_obj->cb;
	cbk->set_dbam_config_cb = clbk;
	cbk->set_dbam_config_ctx = context;

	coex_debug("send dbam config mode %d for vdev_id %d",
		   param->dbam_mode, param->vdev_id);

	return wlan_dbam_config_send(vdev, param);
}
#endif

#define COEX_CONFIG_ENABLE_CONT_INFO 12

QDF_STATUS
ucfg_coex_send_logging_config(struct wlan_objmgr_psoc *psoc,
			      uint32_t *apps_args)
{
	struct coex_config_params param = {0};
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	if (apps_args[0] != COEX_CONFIG_ENABLE_CONT_INFO) {
		coex_err("invalid cmd %d", apps_args[0]);
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc, QDF_STA_MODE,
							WLAN_COEX_ID);

	if (!vdev) {
		coex_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	param.vdev_id = wlan_vdev_get_id(vdev);
	param.config_type = WMI_COEX_CONFIG_ENABLE_CONT_INFO;
	param.config_arg1 = apps_args[1];
	param.config_arg2 = apps_args[2];
	param.config_arg3 = apps_args[3];
	param.config_arg4 = apps_args[4];
	param.config_arg5 = apps_args[5];
	param.config_arg6 = apps_args[6];

	coex_debug("send logging_config arg: %d for vdev %d", apps_args,
		   param.vdev_id);

	status = wlan_coex_config_send(vdev, &param);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_COEX_ID);

	return status;
}
