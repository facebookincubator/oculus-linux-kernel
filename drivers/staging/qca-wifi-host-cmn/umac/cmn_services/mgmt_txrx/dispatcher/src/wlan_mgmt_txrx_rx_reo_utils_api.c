/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *  DOC: wlan_mgmt_txrx_rx_reo_utils_api.c
 *  This file contains mgmt rx re-ordering related public function definitions
 */

#include <wlan_mgmt_txrx_rx_reo_utils_api.h>
#include <wlan_mgmt_txrx_rx_reo_tgt_api.h>
#include "../../core/src/wlan_mgmt_txrx_rx_reo_i.h"
#include <cfg_ucfg_api.h>
#include <wlan_mgmt_txrx_tgt_api.h>
#include<wlan_mgmt_txrx_rx_reo_tgt_api.h>
#include <wlan_mlo_mgr_cmn.h>
#include <wlan_mlo_mgr_setup.h>

QDF_STATUS
wlan_mgmt_rx_reo_deinit(void)
{
	uint8_t ml_grp;
	uint8_t total_mlo_grps = WLAN_MAX_MLO_GROUPS;

	if (total_mlo_grps > WLAN_MAX_MLO_GROUPS)
		return QDF_STATUS_E_INVAL;

	for (ml_grp = 0; ml_grp < total_mlo_grps; ml_grp++)
		if (mgmt_rx_reo_deinit_context(ml_grp))
			return QDF_STATUS_E_INVAL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mgmt_rx_reo_init(void)
{
	uint8_t ml_grp;
	uint8_t total_mlo_grps = WLAN_MAX_MLO_GROUPS;

	if (total_mlo_grps > WLAN_MAX_MLO_GROUPS)
		return QDF_STATUS_E_INVAL;

	for (ml_grp = 0; ml_grp < total_mlo_grps; ml_grp++)
		if (mgmt_rx_reo_init_context(ml_grp))
			return QDF_STATUS_E_INVAL;

	return QDF_STATUS_SUCCESS;
}

#ifndef WLAN_MGMT_RX_REO_SIM_SUPPORT
QDF_STATUS wlan_mgmt_txrx_process_rx_frame(
			struct wlan_objmgr_pdev *pdev,
			qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params)
{
	return tgt_mgmt_txrx_process_rx_frame(pdev, buf, mgmt_rx_params);
}

QDF_STATUS
wlan_mgmt_rx_reo_get_snapshot_info
			(struct wlan_objmgr_pdev *pdev,
			 enum mgmt_rx_reo_shared_snapshot_id id,
			 struct mgmt_rx_reo_snapshot_info *snapshot_info)
{
	return tgt_mgmt_rx_reo_get_snapshot_info(pdev, id, snapshot_info);
}

/**
 * wlan_get_mlo_link_id_from_pdev() - Helper API to get the MLO HW link id
 * from the pdev object.
 * @pdev: Pointer to pdev object
 *
 * Return: On success returns the MLO HW link id corresponding to the pdev
 * object. On failure returns -EINVAL
 */
int8_t
wlan_get_mlo_link_id_from_pdev(struct wlan_objmgr_pdev *pdev)
{
	uint16_t hw_link_id;

	hw_link_id = wlan_mlo_get_pdev_hw_link_id(pdev);

	if (hw_link_id == INVALID_HW_LINK_ID) {
		mgmt_rx_reo_err("Invalid HW link id for the pdev");
		return -EINVAL;
	}

	return hw_link_id;
}

qdf_export_symbol(wlan_get_mlo_link_id_from_pdev);

int8_t
wlan_get_mlo_grp_id_from_pdev(struct wlan_objmgr_pdev *pdev)
{
	uint8_t ml_grp_id;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc) {
		mgmt_rx_reo_err("PSOC is NUll");
		return -EINVAL;
	}

	ml_grp_id = wlan_mlo_get_psoc_group_id(psoc);

	if (ml_grp_id >= WLAN_MAX_MLO_GROUPS) {
		mgmt_rx_reo_debug("Invalid MLO Group ID for the pdev");
		return -EINVAL;
	}

	return ml_grp_id;
}

qdf_export_symbol(wlan_get_mlo_grp_id_from_pdev);

/**
 * wlan_get_pdev_from_mlo_link_id() - Helper API to get the pdev
 * object from the MLO HW link id.
 * @mlo_link_id: MLO HW link id
 * @ml_grp_id: MLO Group id which it belongs to
 * @refdbgid: Reference debug id
 *
 * Return: On success returns the pdev object from the MLO HW link_id.
 * On failure returns NULL.
 */
struct wlan_objmgr_pdev *
wlan_get_pdev_from_mlo_link_id(uint8_t mlo_link_id, uint8_t ml_grp_id,
			       wlan_objmgr_ref_dbgid refdbgid)
{
	return wlan_mlo_get_pdev_by_hw_link_id(
			mlo_link_id, ml_grp_id, refdbgid);
}

qdf_export_symbol(wlan_get_pdev_from_mlo_link_id);
#else
QDF_STATUS wlan_mgmt_txrx_process_rx_frame(
			struct wlan_objmgr_pdev *pdev,
			qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params)
{
	QDF_STATUS status;

	/* Call the legacy handler to actually process and deliver frames */
	status = mgmt_rx_reo_sim_process_rx_frame(pdev, buf, mgmt_rx_params);

	/**
	 * Free up the mgmt rx params.
	 * nbuf shouldn't be freed here as it is taken care by
	 * rx_frame_legacy_handler.
	 */
	free_mgmt_rx_event_params(mgmt_rx_params);

	return status;
}

QDF_STATUS
wlan_mgmt_rx_reo_get_snapshot_info
			(struct wlan_objmgr_pdev *pdev,
			 enum mgmt_rx_reo_shared_snapshot_id id,
			 struct mgmt_rx_reo_snapshot_info *snapshot_info)
{
	QDF_STATUS status;

	status = mgmt_rx_reo_sim_get_snapshot_address(pdev, id,
						      &snapshot_info->address);
	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_err("Failed to get snapshot address for ID = %d",
				id);
		return status;
	}

	snapshot_info->version = 1;

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_get_mlo_link_id_from_pdev() - Helper API to get the MLO HW link id
 * from the pdev object.
 * @pdev: Pointer to pdev object
 *
 * Return: On success returns the MLO HW link id corresponding to the pdev
 * object. On failure returns -1.
 */
int8_t
wlan_get_mlo_link_id_from_pdev(struct wlan_objmgr_pdev *pdev)
{
	return mgmt_rx_reo_sim_get_mlo_link_id_from_pdev(pdev);
}

qdf_export_symbol(wlan_get_mlo_link_id_from_pdev);

/**
 * wlan_get_pdev_from_mlo_link_id() - Helper API to get the pdev
 * object from the MLO HW link id.
 * @mlo_link_id: MLO HW link id
 * @ml_grp_id: MLO Group id which it belongs to
 * @refdbgid: Reference debug id
 *
 * Return: On success returns the pdev object from the MLO HW link_id.
 * On failure returns NULL.
 */
struct wlan_objmgr_pdev *
wlan_get_pdev_from_mlo_link_id(uint8_t mlo_link_id, uint8_t ml_grp_id,
			       wlan_objmgr_ref_dbgid refdbgid)
{
	return mgmt_rx_reo_sim_get_pdev_from_mlo_link_id(
			mlo_link_id, ml_grp_id, refdbgid);
}

qdf_export_symbol(wlan_get_pdev_from_mlo_link_id);
#endif /* WLAN_MGMT_RX_REO_SIM_SUPPORT */

QDF_STATUS
wlan_mgmt_rx_reo_validate_mlo_link_info(struct wlan_objmgr_psoc *psoc)
{
	return mgmt_rx_reo_validate_mlo_link_info(psoc);
}

QDF_STATUS
wlan_mgmt_rx_reo_pdev_obj_create_notification(
			struct wlan_objmgr_pdev *pdev,
			struct mgmt_txrx_priv_pdev_context *mgmt_txrx_pdev_ctx)
{
	return mgmt_rx_reo_pdev_obj_create_notification(pdev,
							mgmt_txrx_pdev_ctx);
}

QDF_STATUS
wlan_mgmt_rx_reo_pdev_obj_destroy_notification(
			struct wlan_objmgr_pdev *pdev,
			struct mgmt_txrx_priv_pdev_context *mgmt_txrx_pdev_ctx)
{
	return mgmt_rx_reo_pdev_obj_destroy_notification(pdev,
							 mgmt_txrx_pdev_ctx);
}

QDF_STATUS
wlan_mgmt_rx_reo_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc)
{
	return mgmt_rx_reo_psoc_obj_create_notification(psoc);
}

QDF_STATUS
wlan_mgmt_rx_reo_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc)
{
	return mgmt_rx_reo_psoc_obj_destroy_notification(psoc);
}

QDF_STATUS
wlan_mgmt_rx_reo_attach(struct wlan_objmgr_pdev *pdev)
{
	return mgmt_rx_reo_attach(pdev);
}

qdf_export_symbol(wlan_mgmt_rx_reo_attach);

QDF_STATUS
wlan_mgmt_rx_reo_detach(struct wlan_objmgr_pdev *pdev)
{
	return mgmt_rx_reo_detach(pdev);
}

qdf_export_symbol(wlan_mgmt_rx_reo_detach);

uint16_t
wlan_mgmt_rx_reo_get_pkt_ctr_delta_thresh(struct wlan_objmgr_psoc *psoc)
{
	return cfg_get(psoc, CFG_MGMT_RX_REO_PKT_CTR_DELTA_THRESH);
}

uint16_t
wlan_mgmt_rx_reo_get_ingress_frame_debug_list_size(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		mgmt_rx_reo_err("psoc is NULL!");
		return 0;
	}

	return cfg_get(psoc, CFG_MGMT_RX_REO_INGRESS_FRAME_DEBUG_LIST_SIZE);
}

uint16_t
wlan_mgmt_rx_reo_get_egress_frame_debug_list_size(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		mgmt_rx_reo_err("psoc is NULL!");
		return 0;
	}

	return cfg_get(psoc, CFG_MGMT_RX_REO_EGRESS_FRAME_DEBUG_LIST_SIZE);
}

#ifndef WLAN_MGMT_RX_REO_SIM_SUPPORT
bool
wlan_mgmt_rx_reo_is_feature_enabled_at_psoc(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		mgmt_rx_reo_err("psoc is NULL!");
		return false;
	}

	if (!cfg_get(psoc, CFG_MGMT_RX_REO_ENABLE))
		return false;

	return wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_MGMT_RX_REO_CAPABLE);
}

qdf_export_symbol(wlan_mgmt_rx_reo_is_feature_enabled_at_psoc);

bool
wlan_mgmt_rx_reo_is_feature_enabled_at_pdev(struct wlan_objmgr_pdev *pdev)
{
	if (!pdev) {
		mgmt_rx_reo_err("pdev is NULL!");
		return false;
	}

	return wlan_mgmt_rx_reo_is_feature_enabled_at_psoc(
			wlan_pdev_get_psoc(pdev));
}

qdf_export_symbol(wlan_mgmt_rx_reo_is_feature_enabled_at_pdev);
#else
bool
wlan_mgmt_rx_reo_is_feature_enabled_at_psoc(struct wlan_objmgr_psoc *psoc)
{
	return true;
}

qdf_export_symbol(wlan_mgmt_rx_reo_is_feature_enabled_at_psoc);

bool
wlan_mgmt_rx_reo_is_feature_enabled_at_pdev(struct wlan_objmgr_pdev *pdev)
{
	return true;
}

qdf_export_symbol(wlan_mgmt_rx_reo_is_feature_enabled_at_pdev);

QDF_STATUS
wlan_mgmt_rx_reo_sim_start(uint8_t ml_grp_id)
{
	return mgmt_rx_reo_sim_start(ml_grp_id);
}

qdf_export_symbol(wlan_mgmt_rx_reo_sim_start);

QDF_STATUS
wlan_mgmt_rx_reo_sim_stop(uint8_t ml_grp_id)
{
	return mgmt_rx_reo_sim_stop(ml_grp_id);
}

qdf_export_symbol(wlan_mgmt_rx_reo_sim_stop);
#endif /* WLAN_MGMT_RX_REO_SIM_SUPPORT */

#ifdef WLAN_MLO_MULTI_CHIP
bool
wlan_mgmt_rx_reo_is_simulation_in_progress(uint8_t ml_grp_id)
{
	return mgmt_rx_reo_is_simulation_in_progress(ml_grp_id);
}

#else
bool
wlan_mgmt_rx_reo_is_simulation_in_progress(uint8_t ml_grp_id)
{
	return false;
}
#endif

QDF_STATUS
wlan_mgmt_rx_reo_print_ingress_frame_stats(uint8_t ml_grp_id)
{
	return mgmt_rx_reo_print_ingress_frame_stats(ml_grp_id);
}

QDF_STATUS
wlan_mgmt_rx_reo_print_ingress_frame_info(uint8_t ml_grp_id,
					  uint16_t num_frames)
{
	return mgmt_rx_reo_print_ingress_frame_info(ml_grp_id, num_frames);
}

QDF_STATUS
wlan_mgmt_rx_reo_print_egress_frame_stats(uint8_t ml_grp_id)
{
	return mgmt_rx_reo_print_egress_frame_stats(ml_grp_id);
}

QDF_STATUS
wlan_mgmt_rx_reo_print_egress_frame_info(uint8_t ml_grp_id, uint16_t num_frames)
{
	return mgmt_rx_reo_print_egress_frame_info(ml_grp_id, num_frames);
}
