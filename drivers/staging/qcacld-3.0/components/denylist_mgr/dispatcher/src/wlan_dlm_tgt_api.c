/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Implements public API for denylist manager to interact with target/WMI
 */

#include "wlan_dlm_tgt_api.h"

#if defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS
tgt_dlm_send_reject_list_to_fw(struct wlan_objmgr_pdev *pdev,
			       struct reject_ap_params *reject_params)
{
	struct wlan_dlm_tx_ops *dlm_tx_ops;
	struct dlm_pdev_priv_obj *dlm_priv;

	dlm_priv = dlm_get_pdev_obj(pdev);

	if (!dlm_priv) {
		dlm_err("dlm_priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	dlm_tx_ops = &dlm_priv->dlm_tx_ops;
	if (!dlm_tx_ops) {
		dlm_err("dlm_tx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (dlm_tx_ops->dlm_send_reject_ap_list)
		return dlm_tx_ops->dlm_send_reject_ap_list(pdev, reject_params);
	dlm_err("Tx ops not registered, failed to send reject list to FW");

	return QDF_STATUS_E_FAILURE;
}
#endif
