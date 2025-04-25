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
 *  DOC: wlan_mgmt_txrx_rx_re_tgt_api.c
 *  This file contains mgmt rx re-ordering tgt layer related function
 *  definitions
 */
#include <wlan_mgmt_txrx_rx_reo_tgt_api.h>
#include <wlan_mlo_mgr_cmn.h>
#include "../../core/src/wlan_mgmt_txrx_rx_reo_i.h"
#include <../../core/src/wlan_mgmt_txrx_main_i.h>

QDF_STATUS
tgt_mgmt_rx_reo_get_num_active_hw_links(struct wlan_objmgr_psoc *psoc,
					int8_t *num_active_hw_links)
{
	struct wlan_lmac_if_mgmt_rx_reo_tx_ops *mgmt_rx_reo_txops;

	mgmt_rx_reo_txops = wlan_psoc_get_mgmt_rx_reo_txops(psoc);
	if (!mgmt_rx_reo_txops) {
		mgmt_rx_reo_err("mgmt rx reo txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mgmt_rx_reo_txops->get_num_active_hw_links) {
		mgmt_rx_reo_err("get num active hw links txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mgmt_rx_reo_txops->get_num_active_hw_links(psoc,
							  num_active_hw_links);
}

QDF_STATUS
tgt_mgmt_rx_reo_get_valid_hw_link_bitmap(struct wlan_objmgr_psoc *psoc,
					 uint16_t *valid_hw_link_bitmap)
{
	struct wlan_lmac_if_mgmt_rx_reo_tx_ops *mgmt_rx_reo_txops;

	mgmt_rx_reo_txops = wlan_psoc_get_mgmt_rx_reo_txops(psoc);
	if (!mgmt_rx_reo_txops) {
		mgmt_rx_reo_err("mgmt rx reo txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mgmt_rx_reo_txops->get_valid_hw_link_bitmap) {
		mgmt_rx_reo_err("get valid hw link bitmap txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mgmt_rx_reo_txops->get_valid_hw_link_bitmap(psoc,
						valid_hw_link_bitmap);
}

QDF_STATUS
tgt_mgmt_rx_reo_read_snapshot(
			struct wlan_objmgr_pdev *pdev,
			struct mgmt_rx_reo_snapshot_info *snapshot_info,
			enum mgmt_rx_reo_shared_snapshot_id id,
			struct mgmt_rx_reo_snapshot_params *value,
			struct mgmt_rx_reo_shared_snapshot (*raw_snapshot)
			[MGMT_RX_REO_SNAPSHOT_B2B_READ_SWAR_RETRY_LIMIT])
{
	struct wlan_lmac_if_mgmt_rx_reo_tx_ops *mgmt_rx_reo_txops;

	mgmt_rx_reo_txops = wlan_pdev_get_mgmt_rx_reo_txops(pdev);
	if (!mgmt_rx_reo_txops) {
		mgmt_rx_reo_err("mgmt rx reo txops is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!mgmt_rx_reo_txops->read_mgmt_rx_reo_snapshot) {
		mgmt_rx_reo_err("mgmt rx reo read snapshot txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mgmt_rx_reo_txops->read_mgmt_rx_reo_snapshot(pdev, snapshot_info,
							    id, value,
							    raw_snapshot);
}

/**
 * tgt_mgmt_rx_reo_enter_algo_without_buffer() - Entry point to the MGMT Rx REO
 * algorithm when there is no frame buffer
 * @pdev: pdev for which this frame/event is intended
 * @reo_params: MGMT Rx REO parameters corresponding to this frame/event
 * @type: Type of the MGMT Rx REO frame/event descriptor
 *
 * Return: QDF_STATUS of operation
 */
static QDF_STATUS
tgt_mgmt_rx_reo_enter_algo_without_buffer(
				struct wlan_objmgr_pdev *pdev,
				struct mgmt_rx_reo_params *reo_params,
				enum mgmt_rx_reo_frame_descriptor_type type)
{
	struct mgmt_rx_event_params mgmt_rx_params = {0};
	struct mgmt_rx_reo_frame_descriptor desc = {0};
	bool is_frm_queued;
	QDF_STATUS status;
	int8_t link_id;
	uint8_t ml_grp_id;

	if (!pdev) {
		mgmt_rx_reo_err("pdev is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!wlan_mgmt_rx_reo_is_feature_enabled_at_pdev(pdev))
		return  QDF_STATUS_SUCCESS;

	if (!reo_params) {
		mgmt_rx_reo_err("mgmt rx reo params are null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	link_id = wlan_get_mlo_link_id_from_pdev(pdev);
	if (link_id < 0) {
		mgmt_rx_reo_err("Invalid link %d for the pdev", link_id);
		return QDF_STATUS_E_INVAL;
	}

	ml_grp_id = wlan_get_mlo_grp_id_from_pdev(pdev);
	if (ml_grp_id > WLAN_MAX_MLO_GROUPS) {
		mgmt_rx_reo_err("Invalid MLO Group  %d for the pdev",
				ml_grp_id);
		return QDF_STATUS_E_INVAL;
	}

	reo_params->link_id = link_id;
	reo_params->mlo_grp_id = ml_grp_id;
	mgmt_rx_params.reo_params = reo_params;

	desc.nbuf = NULL; /* No frame buffer */
	desc.rx_params = &mgmt_rx_params;
	desc.reo_params_copy = *mgmt_rx_params.reo_params;
	desc.type = type;
	desc.ingress_timestamp = qdf_get_log_timestamp();
	desc.ingress_list_size_rx = -1;
	desc.ingress_list_insertion_pos = -1;
	desc.egress_list_size_rx = -1;
	desc.egress_list_insertion_pos = -1;
	desc.frame_type = IEEE80211_FC0_TYPE_MGT;
	desc.frame_subtype = 0xFF;
	desc.reo_required = is_mgmt_rx_reo_required(pdev, &desc);
	desc.queued_list = MGMT_RX_REO_LIST_TYPE_INVALID;
	desc.drop = false;
	desc.drop_reason = MGMT_RX_REO_INGRESS_DROP_REASON_INVALID;

	/* Enter the REO algorithm */
	status = wlan_mgmt_rx_reo_algo_entry(pdev, &desc, &is_frm_queued);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (is_frm_queued) {
		mgmt_rx_reo_err("Frame is queued to reo list");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
tgt_mgmt_rx_reo_fw_consumed_event_handler(struct wlan_objmgr_pdev *pdev,
					  struct mgmt_rx_reo_params *params)
{
	return tgt_mgmt_rx_reo_enter_algo_without_buffer(
			pdev, params, MGMT_RX_REO_FRAME_DESC_FW_CONSUMED_FRAME);
}

QDF_STATUS
tgt_mgmt_rx_reo_host_drop_handler(struct wlan_objmgr_pdev *pdev,
				  struct mgmt_rx_reo_params *params)
{
	return tgt_mgmt_rx_reo_enter_algo_without_buffer(
			pdev, params, MGMT_RX_REO_FRAME_DESC_ERROR_FRAME);
}

/**
 * psoc_get_hw_link_id_bmap() - Helper API to get HW link ID bitmap of the
 * pdevs in a psoc
 * @psoc: Pointer to psoc object
 * @obj: Pointer to pdev object
 * @arg: pointer to void * argument
 *
 * Return: void
 */
static void
psoc_get_hw_link_id_bmap(struct wlan_objmgr_psoc *psoc, void *obj, void *arg)
{
	struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)obj;
	uint32_t *link_bitmap = (uint32_t *)arg;
	int8_t cur_link;

	cur_link = wlan_get_mlo_link_id_from_pdev(pdev);
	if (cur_link < 0 || cur_link >= MAX_MLO_LINKS) {
		mgmt_rx_reo_err("Invalid link = %d", cur_link);
		return;
	}

	*link_bitmap |= (1 << cur_link);
}

QDF_STATUS
tgt_mgmt_rx_reo_release_frames(struct wlan_objmgr_psoc *psoc)
{
	uint8_t mlo_grp_id;
	uint32_t link_bitmap = 0;

	mlo_grp_id = wlan_mlo_get_psoc_group_id(psoc);

	wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
				     psoc_get_hw_link_id_bmap,
				     &link_bitmap, false, WLAN_MGMT_RX_REO_ID);

	return wlan_mgmt_rx_reo_release_frames(mlo_grp_id, link_bitmap);
}

QDF_STATUS tgt_mgmt_rx_reo_filter_config(struct wlan_objmgr_pdev *pdev,
					 struct mgmt_rx_reo_filter *filter)
{
	struct wlan_lmac_if_mgmt_rx_reo_tx_ops *mgmt_rx_reo_txops;

	mgmt_rx_reo_txops = wlan_pdev_get_mgmt_rx_reo_txops(pdev);
	if (!mgmt_rx_reo_txops) {
		mgmt_rx_reo_err("MGMT Rx REO txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mgmt_rx_reo_txops->mgmt_rx_reo_filter_config) {
		mgmt_rx_reo_err("mgmt_rx_reo_filter_config is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mgmt_rx_reo_txops->mgmt_rx_reo_filter_config(pdev, filter);
}

QDF_STATUS
tgt_mgmt_rx_reo_get_snapshot_info
			(struct wlan_objmgr_pdev *pdev,
			 enum mgmt_rx_reo_shared_snapshot_id id,
			 struct mgmt_rx_reo_snapshot_info *snapshot_info)
{
	struct wlan_lmac_if_mgmt_rx_reo_tx_ops *mgmt_rx_reo_txops;

	mgmt_rx_reo_txops = wlan_pdev_get_mgmt_rx_reo_txops(pdev);
	if (!mgmt_rx_reo_txops) {
		mgmt_rx_reo_err("mgmt rx reo txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mgmt_rx_reo_txops->get_mgmt_rx_reo_snapshot_info) {
		mgmt_rx_reo_err("txops entry for get snapshot info is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mgmt_rx_reo_txops->get_mgmt_rx_reo_snapshot_info(pdev, id,
								snapshot_info);
}

bool
wlan_mgmt_rx_reo_check_simulation_in_progress(struct wlan_objmgr_pdev *pdev)
{
	uint8_t ml_grp_id;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	if (!wlan_mlo_get_psoc_capable(psoc))
		return false;

	ml_grp_id = wlan_get_mlo_grp_id_from_pdev(pdev);
	if (ml_grp_id > WLAN_MAX_MLO_GROUPS) {
		mgmt_rx_reo_err("INVALID ML Group ID for the PDEV");
		return false;
	}

	if (!wlan_mgmt_rx_reo_is_simulation_in_progress(ml_grp_id))
		return false;

	return true;
}

QDF_STATUS tgt_mgmt_rx_reo_frame_handler(
				struct wlan_objmgr_pdev *pdev,
				qdf_nbuf_t buf,
				struct mgmt_rx_event_params *mgmt_rx_params)
{
	QDF_STATUS status;
	struct mgmt_rx_reo_frame_descriptor desc = {0};
	bool is_queued;
	int8_t link_id;
	uint8_t ml_grp_id;
	uint8_t frame_type;
	uint8_t frame_subtype;
	struct ieee80211_frame *wh;

	if (!pdev) {
		mgmt_rx_reo_err("pdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto cleanup;
	}

	if (!wlan_mgmt_rx_reo_check_simulation_in_progress(pdev) && !buf) {
		mgmt_rx_reo_err("nbuf is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto cleanup;
	}

	if (!mgmt_rx_params) {
		mgmt_rx_reo_err("MGMT rx params is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto cleanup;
	}

	if (!wlan_mgmt_rx_reo_is_feature_enabled_at_pdev(pdev))
		return tgt_mgmt_txrx_process_rx_frame(pdev, buf,
						      mgmt_rx_params);

	if (!mgmt_rx_params->reo_params) {
		mgmt_rx_reo_err("MGMT rx REO params is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto cleanup;
	}

	link_id = wlan_get_mlo_link_id_from_pdev(pdev);
	if (link_id < 0) {
		mgmt_rx_reo_err("Invalid link %d for the pdev", link_id);
		status = QDF_STATUS_E_INVAL;
		goto cleanup;
	}

	ml_grp_id = wlan_get_mlo_grp_id_from_pdev(pdev);
	if (ml_grp_id > WLAN_MAX_MLO_GROUPS) {
		mgmt_rx_reo_err("Invalid MGMT rx reo Group id");
		status = QDF_STATUS_E_INVAL;
		goto cleanup;
	}

	mgmt_rx_params->reo_params->link_id = link_id;
	mgmt_rx_params->reo_params->mlo_grp_id = ml_grp_id;

	/* Populate frame descriptor */
	desc.type = MGMT_RX_REO_FRAME_DESC_HOST_CONSUMED_FRAME;
	desc.nbuf = buf;
	desc.rx_params = mgmt_rx_params;
	desc.reo_params_copy = *mgmt_rx_params->reo_params;
	desc.ingress_timestamp = qdf_get_log_timestamp();
	desc.ingress_list_size_rx = -1;
	desc.ingress_list_insertion_pos = -1;
	desc.egress_list_size_rx = -1;
	desc.egress_list_insertion_pos = -1;
	desc.queued_list = MGMT_RX_REO_LIST_TYPE_INVALID;
	desc.drop = false;
	desc.drop_reason = MGMT_RX_REO_INGRESS_DROP_REASON_INVALID;

	wh = (struct ieee80211_frame *)qdf_nbuf_data(buf);
	frame_type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	frame_subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	desc.frame_type = frame_type;
	desc.frame_subtype = frame_subtype;

	if (frame_type != IEEE80211_FC0_TYPE_MGT ||
	    !is_mgmt_rx_reo_required(pdev, &desc)) {
		desc.reo_required = false;
		status = wlan_mgmt_rx_reo_algo_entry(pdev, &desc, &is_queued);

		if (QDF_IS_STATUS_ERROR(status)) {
			mgmt_rx_reo_warn_rl("Failed to execute REO algorithm");
			goto cleanup;
		}

		if (is_queued) {
			mgmt_rx_reo_err("Frame is queued to reo list");
			return QDF_STATUS_E_FAILURE;
		}

		return tgt_mgmt_txrx_process_rx_frame(pdev, buf,
						      mgmt_rx_params);
	} else {
		desc.reo_required = true;
		status = wlan_mgmt_rx_reo_algo_entry(pdev, &desc, &is_queued);

		if (QDF_IS_STATUS_ERROR(status))
			mgmt_rx_reo_warn_rl("Failed to execute REO algorithm");

		/**
		 *  If frame is queued, we shouldn't free up params and
		 *  buf pointers.
		 */
		if (is_queued)
			return status;
	}
cleanup:
	qdf_nbuf_free(buf);
	free_mgmt_rx_event_params(mgmt_rx_params);

	return status;
}

QDF_STATUS
tgt_mgmt_rx_reo_schedule_delivery(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_mgmt_rx_reo_tx_ops *mgmt_rx_reo_txops;

	mgmt_rx_reo_txops = wlan_psoc_get_mgmt_rx_reo_txops(psoc);
	if (!mgmt_rx_reo_txops) {
		mgmt_rx_reo_err("MGMT Rx REO txops is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mgmt_rx_reo_txops->schedule_delivery) {
		mgmt_rx_reo_err("mgmt_rx_reo_schedule delivery is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mgmt_rx_reo_txops->schedule_delivery(psoc);
}
