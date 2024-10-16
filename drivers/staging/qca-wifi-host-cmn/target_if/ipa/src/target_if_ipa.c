/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021, 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Target interface file for disa component to
 * Implement api's which shall be used by ipa component
 * in target if internally.
 */

#include <target_if.h>
#include <qdf_status.h>
#include <wmi_unified_api.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include <target_if_ipa.h>
#include <wlan_objmgr_psoc_obj.h>

#ifdef IPA_OFFLOAD
/**
 * target_if_ipa_uc_offload_control_req() - send IPA offload control to FW
 * @psoc: pointer to PSOC object
 * @req: IPA UC offload enable/disable control param
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_ipa_uc_offload_control_req(struct wlan_objmgr_psoc *psoc,
			struct ipa_uc_offload_control_params *req)
{
	return wmi_unified_ipa_offload_control_cmd(
			get_wmi_unified_hdl_from_psoc(psoc), req);
}

/**
 * @req: IPA intra bss enable/disable control param
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_ipa_intrabss_control_req(struct wlan_objmgr_psoc *psoc,
				   struct ipa_intrabss_control_params *req)
{
	struct vdev_set_params param = {0};
	wmi_unified_t wmi_handle;

	wmi_handle = (wmi_unified_t)get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	param.vdev_id = req->vdev_id;
	param.param_id = wmi_vdev_param_intra_bss_fwd;
	param.param_value = req->enable;

	return wmi_unified_vdev_set_param_send(wmi_handle, &param);
}

QDF_STATUS
target_if_ipa_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_ipa_tx_ops *ipa_ops;

	if (!tx_ops) {
		target_if_err("tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	ipa_ops = &tx_ops->ipa_ops;

	ipa_ops->ipa_uc_offload_control_req =
			target_if_ipa_uc_offload_control_req;
	ipa_ops->ipa_intrabss_control_req = target_if_ipa_intrabss_control_req;

	return QDF_STATUS_SUCCESS;
}
#endif
