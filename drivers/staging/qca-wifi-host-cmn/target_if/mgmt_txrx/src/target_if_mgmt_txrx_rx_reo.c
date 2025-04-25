/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

/**
 *  DOC: target_if_mgmt_txrx_rx_reo.c
 *  This file contains definitions of management rx re-ordering related APIs.
 */

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <qdf_status.h>
#include <target_if.h>
#include <wlan_mgmt_txrx_rx_reo_public_structs.h>
#include <target_if_mgmt_txrx_rx_reo.h>
#include <wlan_lmac_if_api.h>
#include <init_deinit_lmac.h>
#include <wlan_mlo_mgr_setup.h>
#include <qdf_platform.h>

/**
 * target_if_mgmt_rx_reo_fw_consumed_event_handler() - WMI event handler to
 * process MGMT Rx FW consumed event handler
 * @scn: Pointer to scn object
 * @data: Pointer to event buffer
 * @datalen: Length of event buffer
 *
 * Return: 0 for success, else failure
 */
static int
target_if_mgmt_rx_reo_fw_consumed_event_handler(
	ol_scn_t scn, uint8_t *data, uint32_t datalen)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wmi_unified *wmi_handle;
	QDF_STATUS status;
	struct mgmt_rx_reo_params params;
	struct wlan_lmac_if_mgmt_rx_reo_rx_ops *mgmt_rx_reo_rx_ops;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		mgmt_rx_reo_err("null psoc");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mgmt_rx_reo_err("wmi_handle is NULL");
		return -EINVAL;
	}

	status = wmi_extract_mgmt_rx_fw_consumed(wmi_handle, data, &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_err("Failed to extract mgmt rx params");
		return -EINVAL;
	}

	mgmt_rx_reo_rx_ops = target_if_mgmt_rx_reo_get_rx_ops(psoc);
	if (!mgmt_rx_reo_rx_ops) {
		mgmt_rx_reo_err("rx_ops of MGMT Rx REO module is NULL");
		return -EINVAL;
	}

	if (!mgmt_rx_reo_rx_ops->fw_consumed_event_handler) {
		mgmt_rx_reo_err("FW consumed event handler is NULL");
		return -EINVAL;
	}

	/* Take the pdev reference */
	pdev = wlan_objmgr_get_pdev_by_id(psoc, params.pdev_id,
					  WLAN_MGMT_SB_ID);
	if (!pdev) {
		mgmt_rx_reo_err("Couldn't get pdev for pdev_id: %d"
				"on psoc: %pK", params.pdev_id, psoc);
		return -EINVAL;
	}

	status = mgmt_rx_reo_rx_ops->fw_consumed_event_handler(pdev, &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_warn_rl("FW consumed event handling failed");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_MGMT_SB_ID);
		return -EINVAL;
	}

	wlan_objmgr_pdev_release_ref(pdev, WLAN_MGMT_SB_ID);
	return 0;
}

void target_if_mgmt_rx_reo_release_frames(void *arg)
{
	ol_scn_t scn = arg;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_mgmt_rx_reo_rx_ops *mgmt_rx_reo_rx_ops;
	QDF_STATUS status;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		mgmt_rx_reo_err("null psoc");
		return;
	}

	mgmt_rx_reo_rx_ops = target_if_mgmt_rx_reo_get_rx_ops(psoc);
	if (!mgmt_rx_reo_rx_ops) {
		mgmt_rx_reo_err("rx_ops of MGMT Rx REO module is NULL");
		return;
	}

	status = mgmt_rx_reo_rx_ops->release_frames(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_err("Failed to release entries, ret = %d", status);
		return;
	}
}

QDF_STATUS
target_if_mgmt_rx_reo_register_event_handlers(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;
	QDF_STATUS status;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mgmt_rx_reo_err("Invalid WMI handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_register_event_handler(
			wmi_handle,
			wmi_mgmt_rx_fw_consumed_eventid,
			target_if_mgmt_rx_reo_fw_consumed_event_handler,
			WMI_RX_UMAC_CTX);

	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_err("Register Rx FW consumed event cb errcode %d",
				status);
		if (status ==  QDF_STATUS_E_NOSUPPORT)
			status = QDF_STATUS_SUCCESS;
	}

	return status;
}

QDF_STATUS
target_if_mgmt_rx_reo_unregister_event_handlers(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;
	QDF_STATUS status;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mgmt_rx_reo_err("Invalid WMI handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_unregister_event_handler(
			wmi_handle,
			wmi_mgmt_rx_fw_consumed_eventid);

	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_err("Unregister Rx FW consumed event cb errcode %d",
				status);
		if (status ==  QDF_STATUS_E_NOSUPPORT)
			status = QDF_STATUS_SUCCESS;
	}

	return status;
}

/**
 * target_if_mgmt_rx_reo_get_num_active_hw_links() - Get number of active MLO HW
 * links
 * @psoc: Pointer to psoc object
 * @num_active_hw_links: pointer to number of active MLO HW links
 *
 * Get number of active MLO HW links from the MLO global shared memory arena.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_mgmt_rx_reo_get_num_active_hw_links(struct wlan_objmgr_psoc *psoc,
					      int8_t *num_active_hw_links)
{
	struct wlan_lmac_if_mgmt_rx_reo_low_level_ops *low_level_ops;
	uint8_t grp_id;

	if (!psoc) {
		mgmt_rx_reo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!num_active_hw_links) {
		mgmt_rx_reo_err("Pointer to num_active_hw_links is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_psoc_get_grp_id(psoc, &grp_id)) {
		mgmt_rx_reo_err("Failed to get valid MLO Group id");
		return QDF_STATUS_E_INVAL;
	}

	low_level_ops = target_if_get_mgmt_rx_reo_low_level_ops(psoc);

	if (!low_level_ops) {
		mgmt_rx_reo_err("Low level ops of MGMT Rx REO is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!low_level_ops->implemented) {
		mgmt_rx_reo_err("Low level ops not implemented");
		return QDF_STATUS_E_INVAL;
	}

	*num_active_hw_links = low_level_ops->get_num_links(grp_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_mgmt_rx_reo_get_valid_hw_link_bitmap() - Get valid MLO HW link
 * bitmap
 * @psoc: Pointer to psoc object
 * @valid_hw_link_bitmap: Pointer to valid MLO HW link bitmap
 *
 * Get valid MLO HW link bitmap from the MLO global shared memory arena.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_mgmt_rx_reo_get_valid_hw_link_bitmap(struct wlan_objmgr_psoc *psoc,
					       uint16_t *valid_hw_link_bitmap)
{
	struct wlan_lmac_if_mgmt_rx_reo_low_level_ops *low_level_ops;
	uint8_t grp_id;

	if (!psoc) {
		mgmt_rx_reo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!valid_hw_link_bitmap) {
		mgmt_rx_reo_err("Pointer to valid_hw_link_bitmap is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_psoc_get_grp_id(psoc, &grp_id)) {
		mgmt_rx_reo_err("Failed to get valid MLO Group id");
		return QDF_STATUS_E_INVAL;
	}

	low_level_ops = target_if_get_mgmt_rx_reo_low_level_ops(psoc);

	if (!low_level_ops) {
		mgmt_rx_reo_err("Low level ops of MGMT Rx REO is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!low_level_ops->implemented) {
		mgmt_rx_reo_err("Low level ops not implemented");
		return QDF_STATUS_E_INVAL;
	}

	*valid_hw_link_bitmap = low_level_ops->get_valid_link_bitmap(grp_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_mgmt_rx_reo_read_snapshot_raw() - Read raw value of management
 * rx-reorder snapshot
 * @pdev: pointer to pdev object
 * @snapshot_address: snapshot address
 * @mgmt_rx_reo_snapshot_low: Pointer to lower 32 bits of snapshot value
 * @mgmt_rx_reo_snapshot_high: Pointer to higher 32 bits of snapshot value
 * @snapshot_version: snapshot version
 * @raw_snapshot: Raw snapshot data
 *
 * Read raw value of management rx-reorder snapshots.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_mgmt_rx_reo_read_snapshot_raw
			(struct wlan_objmgr_pdev *pdev,
			 struct mgmt_rx_reo_shared_snapshot *snapshot_address,
			 uint32_t *mgmt_rx_reo_snapshot_low,
			 uint32_t *mgmt_rx_reo_snapshot_high,
			 uint8_t snapshot_version,
			 struct mgmt_rx_reo_shared_snapshot *raw_snapshot)
{
	uint32_t prev_snapshot_low;
	uint32_t prev_snapshot_high;
	uint32_t cur_snapshot_low;
	uint32_t cur_snapshot_high;
	uint8_t retry_count = 0;

	if (snapshot_version == 1) {
		*mgmt_rx_reo_snapshot_low =
		    qdf_le32_to_cpu(snapshot_address->mgmt_rx_reo_snapshot_low);
		*mgmt_rx_reo_snapshot_high =
		   qdf_le32_to_cpu(snapshot_address->mgmt_rx_reo_snapshot_high);
		raw_snapshot->mgmt_rx_reo_snapshot_low =
						*mgmt_rx_reo_snapshot_low;
		raw_snapshot->mgmt_rx_reo_snapshot_high =
						*mgmt_rx_reo_snapshot_high;
		return QDF_STATUS_SUCCESS;
	}

	prev_snapshot_low =
		qdf_le32_to_cpu(snapshot_address->mgmt_rx_reo_snapshot_low);
	prev_snapshot_high =
		qdf_le32_to_cpu(snapshot_address->mgmt_rx_reo_snapshot_high);
	raw_snapshot->mgmt_rx_reo_snapshot_low = prev_snapshot_low;
	raw_snapshot->mgmt_rx_reo_snapshot_high = prev_snapshot_high;

	for (; retry_count < (MGMT_RX_REO_SNAPSHOT_B2B_READ_SWAR_RETRY_LIMIT - 1);
	     retry_count++) {
		cur_snapshot_low =
		    qdf_le32_to_cpu(snapshot_address->mgmt_rx_reo_snapshot_low);
		cur_snapshot_high =
		   qdf_le32_to_cpu(snapshot_address->mgmt_rx_reo_snapshot_high);

		raw_snapshot[retry_count + 1].mgmt_rx_reo_snapshot_low =
							cur_snapshot_low;
		raw_snapshot[retry_count + 1].mgmt_rx_reo_snapshot_high =
							cur_snapshot_high;

		if (prev_snapshot_low == cur_snapshot_low &&
		    prev_snapshot_high == cur_snapshot_high)
			break;

		prev_snapshot_low = cur_snapshot_low;
		prev_snapshot_high = cur_snapshot_high;
	}

	if (retry_count ==
	    (MGMT_RX_REO_SNAPSHOT_B2B_READ_SWAR_RETRY_LIMIT - 1)) {
		enum qdf_hang_reason reason;

		reason = QDF_MGMT_RX_REO_INCONSISTENT_SNAPSHOT;
		mgmt_rx_reo_err("Triggering self recovery, inconsistent SS");
		qdf_trigger_self_recovery(wlan_pdev_get_psoc(pdev), reason);
	}

	*mgmt_rx_reo_snapshot_low = cur_snapshot_low;
	*mgmt_rx_reo_snapshot_high = cur_snapshot_high;

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_mgmt_rx_reo_read_snapshot() - Read management rx-reorder snapshot
 * @pdev: pdev pointer
 * @snapshot_info: Snapshot info
 * @id: Snapshot ID
 * @snapshot_value: Pointer to snapshot value
 * @raw_snapshot: Raw snapshot data
 *
 * Read management rx-reorder snapshots from target.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_mgmt_rx_reo_read_snapshot(
			struct wlan_objmgr_pdev *pdev,
			struct mgmt_rx_reo_snapshot_info *snapshot_info,
			enum mgmt_rx_reo_shared_snapshot_id id,
			struct mgmt_rx_reo_snapshot_params *snapshot_value,
			struct mgmt_rx_reo_shared_snapshot (*raw_snapshot)
			[MGMT_RX_REO_SNAPSHOT_B2B_READ_SWAR_RETRY_LIMIT])
{
	bool snapshot_valid;
	uint16_t mgmt_pkt_ctr;
	uint32_t global_timestamp;
	uint32_t mgmt_rx_reo_snapshot_low;
	uint32_t mgmt_rx_reo_snapshot_high;
	uint8_t retry_count;
	QDF_STATUS status;
	struct wlan_lmac_if_mgmt_rx_reo_low_level_ops *low_level_ops;
	struct mgmt_rx_reo_shared_snapshot *snapshot_address;
	uint8_t snapshot_version;

	if (!snapshot_info) {
		mgmt_rx_reo_err("Mgmt Rx REO snapshot info null");
		return QDF_STATUS_E_INVAL;
	}

	snapshot_address = snapshot_info->address;
	if (!snapshot_address) {
		mgmt_rx_reo_err("Mgmt Rx REO snapshot address null");
		return QDF_STATUS_E_INVAL;
	}

	snapshot_version = snapshot_info->version;

	if (!snapshot_value) {
		mgmt_rx_reo_err("Mgmt Rx REO snapshot null");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_zero(snapshot_value, sizeof(*snapshot_value));

	low_level_ops = target_if_get_mgmt_rx_reo_low_level_ops(
				wlan_pdev_get_psoc(pdev));

	if (!low_level_ops) {
		mgmt_rx_reo_err("Low level ops of MGMT Rx REO is null");
		return QDF_STATUS_E_FAILURE;
	}

	/* Make sure that function pointers are populated */
	if (!low_level_ops->implemented) {
		mgmt_rx_reo_err("Low level ops not implemented");
		return QDF_STATUS_E_INVAL;
	}

	switch (id) {
	case MGMT_RX_REO_SHARED_SNAPSHOT_MAC_HW:
	case MGMT_RX_REO_SHARED_SNAPSHOT_FW_CONSUMED:
	case MGMT_RX_REO_SHARED_SNAPSHOT_FW_FORWARDED:
		retry_count = 0;
		for (; retry_count < MGMT_RX_REO_SNAPSHOT_READ_RETRY_LIMIT;
		     retry_count++) {
			status = target_if_mgmt_rx_reo_read_snapshot_raw
					(pdev, snapshot_address,
					 &mgmt_rx_reo_snapshot_low,
					 &mgmt_rx_reo_snapshot_high,
					 snapshot_version,
					 raw_snapshot[retry_count]);

			if (QDF_IS_STATUS_ERROR(status)) {
				mgmt_rx_reo_err("Failed to read snapshot %d",
						id);
				return QDF_STATUS_E_FAILURE;
			}

			snapshot_valid = low_level_ops->snapshot_is_valid(
						mgmt_rx_reo_snapshot_low,
						snapshot_version);

			if (!snapshot_valid) {
				mgmt_rx_reo_info("Invalid REO snapshot value");
				snapshot_value->valid = false;
				snapshot_value->mgmt_pkt_ctr =
					low_level_ops->snapshot_get_mgmt_pkt_ctr
					(mgmt_rx_reo_snapshot_low,
					 snapshot_version);
				snapshot_value->global_timestamp =
				low_level_ops->snapshot_get_global_timestamp
					(mgmt_rx_reo_snapshot_low,
					 mgmt_rx_reo_snapshot_high,
					 snapshot_version);
				snapshot_value->retry_count = retry_count + 1;
				return QDF_STATUS_SUCCESS;
			}

			if (low_level_ops->snapshot_is_consistent
						(mgmt_rx_reo_snapshot_low,
						 mgmt_rx_reo_snapshot_high,
						 snapshot_version)) {
				global_timestamp =
				    low_level_ops->snapshot_get_global_timestamp
						(mgmt_rx_reo_snapshot_low,
						 mgmt_rx_reo_snapshot_high,
						 snapshot_version);
				mgmt_pkt_ctr =
					low_level_ops->snapshot_get_mgmt_pkt_ctr
						(mgmt_rx_reo_snapshot_low,
						 snapshot_version);
				break;
			}
			mgmt_rx_reo_info("Inconsistent snapshot %d, version=%u, low=0x%x, high=0x%x, retry=%u",
					 id, snapshot_version,
					 mgmt_rx_reo_snapshot_low,
					 mgmt_rx_reo_snapshot_high,
					 retry_count);
		}

		if (retry_count == MGMT_RX_REO_SNAPSHOT_READ_RETRY_LIMIT) {
			enum qdf_hang_reason reason;

			mgmt_rx_reo_err("Read retry limit, id = %d, ver = %u",
					id, snapshot_version);
			snapshot_value->valid = false;
			snapshot_value->mgmt_pkt_ctr = 0xFFFF;
			snapshot_value->global_timestamp = 0xFFFFFFFF;
			snapshot_value->retry_count = retry_count;
			reason = QDF_MGMT_RX_REO_INCONSISTENT_SNAPSHOT;
			mgmt_rx_reo_err("Triggering self recovery, retry fail");
			qdf_trigger_self_recovery(wlan_pdev_get_psoc(pdev),
						  reason);
			return QDF_STATUS_E_FAILURE;
		}

		snapshot_value->valid = true;
		snapshot_value->mgmt_pkt_ctr = mgmt_pkt_ctr;
		snapshot_value->global_timestamp = global_timestamp;
		snapshot_value->retry_count = retry_count + 1;
		status = QDF_STATUS_SUCCESS;
		break;

	default:
		mgmt_rx_reo_err("Invalid snapshot id %d", id);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

/**
 * target_if_mgmt_rx_reo_get_snapshot_info() - Get information related to
 * management rx-reorder snapshot
 * @pdev: Pointer to pdev object
 * @id: Snapshot ID
 * @snapshot_info: Pointer to snapshot info
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_mgmt_rx_reo_get_snapshot_info
			(struct wlan_objmgr_pdev *pdev,
			 enum mgmt_rx_reo_shared_snapshot_id id,
			 struct mgmt_rx_reo_snapshot_info *snapshot_info)
{
	struct wlan_lmac_if_mgmt_rx_reo_low_level_ops *low_level_ops;
	int8_t link_id;
	int8_t snapshot_version;
	uint8_t grp_id;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		mgmt_rx_reo_err("pdev is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		mgmt_rx_reo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (id >= MGMT_RX_REO_SHARED_SNAPSHOT_MAX) {
		mgmt_rx_reo_err("Mgmt RX REO snapshot id invalid %d", id);
		return QDF_STATUS_E_INVAL;
	}

	if (!snapshot_info) {
		mgmt_rx_reo_err("Ref to mgmt RX REO snapshot info is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_psoc_get_grp_id(psoc, &grp_id)) {
		mgmt_rx_reo_err("Failed to get valid MLO Group id");
		return QDF_STATUS_E_INVAL;
	}

	low_level_ops = target_if_get_mgmt_rx_reo_low_level_ops(psoc);

	if (!low_level_ops) {
		mgmt_rx_reo_err("Low level ops of MGMT Rx REO is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (!low_level_ops->implemented) {
		mgmt_rx_reo_err("Low level ops not implemented");
		return QDF_STATUS_E_INVAL;
	}

	link_id = wlan_get_mlo_link_id_from_pdev(pdev);
	if (link_id < 0) {
		mgmt_rx_reo_err("Invalid link id %d", link_id);
		return QDF_STATUS_E_INVAL;
	}

	snapshot_info->address =
			low_level_ops->get_snapshot_address(grp_id,
							    link_id, id);

	snapshot_version = low_level_ops->get_snapshot_version(grp_id, id);
	if (snapshot_version < 0) {
		mgmt_rx_reo_err("Invalid snapshot version %d MLO Group id %d",
				snapshot_version, grp_id);
		return QDF_STATUS_E_INVAL;
	}

	snapshot_info->version = snapshot_version;

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_mgmt_rx_reo_filter_config() - Configure MGMT Rx REO filter
 * @pdev: Pointer to pdev objmgr
 * @filter: Pointer to MGMT Rx REO filter
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
target_if_mgmt_rx_reo_filter_config(
	struct wlan_objmgr_pdev *pdev,
	struct mgmt_rx_reo_filter *filter)
{
	QDF_STATUS status;
	struct wmi_unified *wmi_handle;
	uint8_t pdev_id;

	wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!wmi_handle) {
		mgmt_rx_reo_err("Invalid WMI handle");
		return QDF_STATUS_E_INVAL;
	}
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	status = wmi_unified_mgmt_rx_reo_filter_config_cmd(wmi_handle, pdev_id,
							   filter);
	if (QDF_IS_STATUS_ERROR(status))
		mgmt_rx_reo_err("Unable to send MGMT Rx REO Filter config cmd");

	return status;
}

QDF_STATUS
target_if_mgmt_rx_reo_extract_reo_params(
	wmi_unified_t wmi_handle, void *evt_buf,
	struct mgmt_rx_event_params *params)
{
	struct wlan_objmgr_psoc *psoc;

	if (!wmi_handle) {
		mgmt_rx_reo_err("wmi_handle is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	psoc = target_if_get_psoc_from_scn_hdl(wmi_handle->scn_handle);
	if (!psoc) {
		mgmt_rx_reo_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* If REO feature is not enabled in FW, no need to extract REO params */
	if (!wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_MGMT_RX_REO_CAPABLE))
		return QDF_STATUS_SUCCESS;

	if (!params) {
		mgmt_rx_reo_err("MGMT Rx event parameters is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return wmi_extract_mgmt_rx_reo_params(wmi_handle, evt_buf,
					      params->reo_params);
}

/**
 * target_if_mgmt_rx_reo_schedule_delivery() - Schedule the delivery of
 * management frames of the given psoc
 * @psoc: Pointer to psoc object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_mgmt_rx_reo_schedule_delivery(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;
	QDF_STATUS status;
	HTC_ENDPOINT_ID wmi_endpoint_id;
	HTC_HANDLE htc_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mgmt_rx_reo_err("wmi_handle is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	htc_handle = lmac_get_htc_hdl(psoc);
	if (!htc_handle) {
		mgmt_rx_reo_err("HTC_handle is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wmi_endpoint_id = wmi_get_endpoint(wmi_handle);

	status = htc_enable_custom_cb(htc_handle, wmi_endpoint_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_err("Failed to schedule delivery");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_mgmt_rx_reo_cancel_scheduled_delivery() - Cancel the scheduled
 * delivery of management frames of the given psoc
 * @psoc: Pointer to psoc object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_mgmt_rx_reo_cancel_scheduled_delivery(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;
	QDF_STATUS status;
	HTC_ENDPOINT_ID wmi_endpoint_id;
	HTC_HANDLE htc_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mgmt_rx_reo_err("wmi_handle is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	htc_handle = lmac_get_htc_hdl(psoc);
	if (!htc_handle) {
		mgmt_rx_reo_err("HTC_handle is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wmi_endpoint_id = wmi_get_endpoint(wmi_handle);

	status = htc_disable_custom_cb(htc_handle, wmi_endpoint_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		mgmt_rx_reo_err("Failed to cancel scheduled delivery");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_mgmt_rx_reo_tx_ops_register(
			struct wlan_lmac_if_mgmt_txrx_tx_ops *mgmt_txrx_tx_ops)
{
	struct wlan_lmac_if_mgmt_rx_reo_tx_ops *mgmt_rx_reo_tx_ops;

	if (!mgmt_txrx_tx_ops) {
		mgmt_rx_reo_err("mgmt_txrx txops NULL");
		return QDF_STATUS_E_FAILURE;
	}
	mgmt_rx_reo_tx_ops = &mgmt_txrx_tx_ops->mgmt_rx_reo_tx_ops;
	mgmt_rx_reo_tx_ops->get_num_active_hw_links =
				target_if_mgmt_rx_reo_get_num_active_hw_links;
	mgmt_rx_reo_tx_ops->get_valid_hw_link_bitmap =
				target_if_mgmt_rx_reo_get_valid_hw_link_bitmap;
	mgmt_rx_reo_tx_ops->read_mgmt_rx_reo_snapshot =
				target_if_mgmt_rx_reo_read_snapshot;
	mgmt_rx_reo_tx_ops->get_mgmt_rx_reo_snapshot_info =
				target_if_mgmt_rx_reo_get_snapshot_info;
	mgmt_rx_reo_tx_ops->mgmt_rx_reo_filter_config =
					target_if_mgmt_rx_reo_filter_config;
	mgmt_rx_reo_tx_ops->schedule_delivery =
				target_if_mgmt_rx_reo_schedule_delivery;
	mgmt_rx_reo_tx_ops->cancel_scheduled_delivery =
				target_if_mgmt_rx_reo_cancel_scheduled_delivery;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_mgmt_rx_reo_host_drop_handler(struct wlan_objmgr_pdev *pdev,
					struct mgmt_rx_event_params *params)
{
	struct wlan_lmac_if_mgmt_rx_reo_rx_ops *mgmt_rx_reo_rx_ops;

	if (!pdev) {
		mgmt_rx_reo_err("pdev is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!params) {
		mgmt_rx_reo_err("mgmt rx event params are null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mgmt_rx_reo_rx_ops = target_if_mgmt_rx_reo_get_rx_ops(
					wlan_pdev_get_psoc(pdev));
	if (!mgmt_rx_reo_rx_ops) {
		mgmt_rx_reo_err("rx_ops of MGMT Rx REO module is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mgmt_rx_reo_rx_ops->host_drop_handler(pdev, params->reo_params);
}
