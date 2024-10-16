/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: contains interface prototypes for OS_IF layer
 */
#include <qdf_trace.h>
#include <spatial_reuse_ucfg_api.h>
#include <spatial_reuse_api.h>
#include <wlan_policy_mgr_api.h>

void ucfg_spatial_reuse_register_cb(struct wlan_objmgr_psoc *psoc,
				    sr_osif_event_cb cb)
{
	wlan_sr_register_callback(psoc, cb);
}

void ucfg_spatial_reuse_get_sr_config(struct wlan_objmgr_vdev *vdev,
				      uint8_t *sr_ctrl,
				      uint8_t *non_srg_max_pd_offset,
				      bool *he_spr_enabled)
{
	*sr_ctrl = wlan_vdev_mlme_get_sr_ctrl(vdev);
	*non_srg_max_pd_offset = wlan_vdev_mlme_get_non_srg_pd_offset(vdev);
	*he_spr_enabled = wlan_vdev_mlme_get_he_spr_enabled(vdev);
}

void ucfg_spatial_reuse_set_sr_config(struct wlan_objmgr_vdev *vdev,
				      uint8_t sr_ctrl,
				      uint8_t non_srg_max_pd_offset)
{
	wlan_vdev_mlme_set_sr_ctrl(vdev, sr_ctrl);
	wlan_vdev_mlme_set_non_srg_pd_offset(vdev, non_srg_max_pd_offset);
}

bool ucfg_spatial_reuse_is_sr_disabled_due_conc(struct wlan_objmgr_vdev *vdev)
{
	return wlan_vdev_mlme_is_sr_disable_due_conc(vdev);
}

void ucfg_spatial_reuse_set_sr_conc_stat(struct wlan_objmgr_vdev *vdev,
					 bool sr_conc_disabled)
{
	wlan_vdev_mlme_set_sr_disable_due_conc(vdev, sr_conc_disabled);
}

void ucfg_spatial_reuse_send_sr_config(struct wlan_objmgr_vdev *vdev,
				       bool enable)
{
	uint8_t sr_ctrl = 0;
	/* Disabled PD Threshold */
	uint8_t non_srg_max_pd_offset = 0x80;

	/* SR feature itself is disabled by user */
	if (!wlan_vdev_mlme_get_he_spr_enabled(vdev))
		return;
	/* SR is disabled due to conccurrency */
	if (ucfg_spatial_reuse_is_sr_disabled_due_conc(vdev))
		return;

	if (enable) {
		sr_ctrl = wlan_vdev_mlme_get_sr_ctrl(vdev);
		non_srg_max_pd_offset =
				wlan_vdev_mlme_get_non_srg_pd_offset(vdev);
		if (sr_ctrl && non_srg_max_pd_offset)
			wlan_spatial_reuse_config_set(vdev, sr_ctrl,
						      non_srg_max_pd_offset);
	} else {
		wlan_spatial_reuse_config_set(vdev, sr_ctrl,
					      non_srg_max_pd_offset);
	}
}

void ucfg_spatial_reuse_set_sr_enable(struct wlan_objmgr_vdev *vdev,
				      bool enable)
{
	 wlan_vdev_mlme_set_he_spr_enabled(vdev, enable);
}

QDF_STATUS ucfg_spatial_reuse_send_sr_prohibit(
					struct wlan_objmgr_vdev *vdev,
					bool enable_he_siga_val15_prohibit)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool sr_enabled = wlan_vdev_mlme_get_he_spr_enabled(vdev);
	bool sr_prohibited = wlan_vdev_mlme_is_sr_prohibit_en(vdev);
	uint8_t sr_ctrl = wlan_vdev_mlme_get_sr_ctrl(vdev);

	/* Enable PD prohibit only when it is allowed by the AP,
	 * Check if it is not enabled already, then only enable it
	 */
	if (sr_enabled && (sr_ctrl & WLAN_HE_SIGA_SR_VAL15_ALLOWED) &&
	    sr_prohibited != enable_he_siga_val15_prohibit) {
		status = wlan_spatial_reuse_he_siga_val15_allowed_set
					(vdev,
					 enable_he_siga_val15_prohibit);

		if (QDF_IS_STATUS_SUCCESS(status))
			wlan_vdev_mlme_set_sr_prohibit_en
					(vdev,
					 enable_he_siga_val15_prohibit);
	} else {
		mlme_debug("Prohibit command can not be sent sr_enabled %d, sr_ctrl %d , sr_prohibited %d",
			   sr_enabled,
			   sr_ctrl,
			   sr_prohibited);

		return QDF_STATUS_E_FAILURE;
	}
	return status;
}

QDF_STATUS
ucfg_spatial_reuse_setup_req(struct wlan_objmgr_vdev *vdev,
			     struct wlan_objmgr_pdev *pdev,
			     bool is_sr_enable, int32_t srg_pd_threshold,
			     int32_t non_srg_pd_threshold)
{
	return wlan_sr_setup_req(vdev, pdev, is_sr_enable,
				 srg_pd_threshold, non_srg_pd_threshold);
}

QDF_STATUS ucfg_spatial_reuse_operation_allowed(struct wlan_objmgr_psoc *psoc,
						struct wlan_objmgr_vdev *vdev)
{
	uint32_t conc_vdev_id;
	uint8_t vdev_id, mac_id;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev || !psoc)
		return QDF_STATUS_E_NULL_VALUE;

	vdev_id = wlan_vdev_get_id(vdev);
	if (!policy_mgr_get_connection_count(psoc)) {
		mlme_debug("No active vdev");
		return status;
	}
	status = policy_mgr_get_mac_id_by_session_id(psoc, vdev_id, &mac_id);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	conc_vdev_id = policy_mgr_get_conc_vdev_on_same_mac(psoc, vdev_id,
							    mac_id);
	if (conc_vdev_id != WLAN_INVALID_VDEV_ID &&
	    !policy_mgr_sr_same_mac_conc_enabled(psoc))
		return QDF_STATUS_E_NOSUPPORT;
	return status;
}
