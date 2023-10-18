/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

/*
 * DOC: wlan_wfa_tgt_if_tx_api.c
 *
 * Implementation for the Common WFA config interfaces.
 */

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_wfa_tgt_if_tx_api.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_vdev_mgr_tgt_if_tx_api.h"
#include "wma.h"

static inline struct wlan_wfa_cmd_tx_ops *
wlan_wfatest_get_tx_ops_from_vdev(struct wlan_objmgr_vdev *vdev)
{
	mlme_psoc_ext_t *mlme_priv;
	struct wlan_wfa_cmd_tx_ops *tx_ops;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		mlme_legacy_err("psoc object is NULL");
		return NULL;
	}

	mlme_priv = wlan_psoc_mlme_get_ext_hdl(psoc);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return NULL;
	}

	tx_ops = &mlme_priv->wfa_testcmd.tx_ops;

	return tx_ops;
}

static QDF_STATUS
wlan_wfa_set_test_feature_flags(struct wlan_objmgr_psoc *psoc,
			       enum wlan_wfa_test_feature_flags feature,
			       uint8_t value)
{
	mlme_psoc_ext_t *mlme_priv;

	if (!psoc) {
		mlme_legacy_err("psoc object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_psoc_mlme_get_ext_hdl(psoc);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	switch (feature) {
	case WFA_TEST_IGNORE_RSNXE:
		if (value)
			mlme_priv->wfa_testcmd.flags |= WFA_TEST_IGNORE_RSNXE;
		else
			mlme_priv->wfa_testcmd.flags &= ~WFA_TEST_IGNORE_RSNXE;
		break;
	default:
		mlme_legacy_debug("Invalid feature flag: 0x%x", feature);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

bool wlan_wfa_get_test_feature_flags(struct wlan_objmgr_psoc *psoc,
				     enum wlan_wfa_test_feature_flags feature)
{
	mlme_psoc_ext_t *mlme_priv;
	bool set = false;

	if (!psoc) {
		mlme_legacy_err("psoc object is NULL");
		return set;
	}

	mlme_priv = wlan_psoc_mlme_get_ext_hdl(psoc);
	if (!mlme_priv) {
		mlme_legacy_err("psoc legacy private object is NULL");
		return set;
	}

	switch (feature) {
	case WFA_TEST_IGNORE_RSNXE:
		set = !!(mlme_priv->wfa_testcmd.flags & WFA_TEST_IGNORE_RSNXE);
		if (set)
			mlme_legacy_debug("IGNORE_RSNXE is set");
		break;
	default:
		mlme_legacy_debug("Invalid feature flag: 0x%x", feature);
		break;
	}

	return set;
}

QDF_STATUS
wlan_send_wfatest_cmd(struct wlan_objmgr_vdev *vdev,
		      struct set_wfatest_params *wmi_wfatest)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_wfa_cmd_tx_ops *tx_ops;
	struct vdev_mlme_obj *mlme_obj;
	struct config_fils_params param = {0};

	if (!vdev || !wmi_wfatest) {
		mlme_legacy_err("vdev or test params is NULL");
		return status;
	}

	if (wmi_wfatest->cmd == WFA_FILS_DISCV_FRAMES) {
		mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
		if (!mlme_obj) {
			wma_err("failed to get mlme_obj");
			return QDF_STATUS_E_INVAL;
		}

		param.vdev_id = wmi_wfatest->vdev_id;
		if (wmi_wfatest->value)
			param.fd_period = DEFAULT_FILS_DISCOVERY_PERIOD;

		return tgt_vdev_mgr_fils_enable_send(mlme_obj, &param);
	} else if (wmi_wfatest->cmd == WFA_IGNORE_H2E_RSNXE) {
		return wlan_wfa_set_test_feature_flags(wlan_vdev_get_psoc(vdev),
						       WFA_TEST_IGNORE_RSNXE,
						       wmi_wfatest->value);
	}

	tx_ops = wlan_wfatest_get_tx_ops_from_vdev(vdev);
	if (!tx_ops || !tx_ops->send_wfa_test_cmd) {
		mlme_legacy_err("Failed to send WFA test cmd");
		return QDF_STATUS_E_FAILURE;
	}

	return tx_ops->send_wfa_test_cmd(vdev, wmi_wfatest);
}
