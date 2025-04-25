/*
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
 * DOC: target_if_dp_comp.c
 */

#include "target_if_dp_comp.h"
#include "target_if.h"
#include "qdf_status.h"
#include "wmi.h"
#include "wmi_unified_api.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_param.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_dp_public_struct.h"
#include "cdp_txrx_cmn.h"
#include "cdp_txrx_ops.h"
#include "wlan_dp_main.h"
#include <wlan_cm_api.h>

/**
 * target_if_dp_get_arp_stats_event_handler() - arp stats event handler
 * @scn: scn
 * @data: buffer with event
 * @datalen: buffer length
 *
 * Return: Return: 0 on success, failure code otherwise.
 */
static int
target_if_dp_get_arp_stats_event_handler(ol_scn_t scn, uint8_t *data,
					 uint32_t datalen)
{
	WMI_VDEV_GET_ARP_STAT_EVENTID_param_tlvs *param_buf;
	wmi_vdev_get_arp_stats_event_fixed_param *data_event;
	wmi_vdev_get_connectivity_check_stats *connect_stats_event;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_dp_psoc_nb_ops *nb_ops;
	uint8_t *buf_ptr;
	struct dp_rsp_stats rsp = {0};

	if (!scn || !data) {
		dp_err("scn: 0x%pK, data: 0x%pK", scn, data);
		return -EINVAL;
	}
	param_buf = (WMI_VDEV_GET_ARP_STAT_EVENTID_param_tlvs *)data;
	if (!param_buf) {
		dp_err("Invalid get arp stats event");
		return -EINVAL;
	}
	data_event = param_buf->fixed_param;
	if (!data_event) {
		dp_err("Invalid get arp stats data event");
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		dp_err("null psoc");
		return -EINVAL;
	}

	nb_ops = dp_intf_get_rx_ops(psoc);
	if (!nb_ops) {
		dp_err("null tx ops");
		return -EINVAL;
	}

	rsp.arp_req_enqueue = data_event->arp_req_enqueue;
	rsp.vdev_id = data_event->vdev_id;
	rsp.arp_req_tx_success = data_event->arp_req_tx_success;
	rsp.arp_req_tx_failure = data_event->arp_req_tx_failure;
	rsp.arp_rsp_recvd = data_event->arp_rsp_recvd;
	rsp.out_of_order_arp_rsp_drop_cnt =
		data_event->out_of_order_arp_rsp_drop_cnt;
	rsp.dad_detected = data_event->dad_detected;
	rsp.connect_status = data_event->connect_status;
	rsp.ba_session_establishment_status =
		data_event->ba_session_establishment_status;

	buf_ptr = (uint8_t *)data_event;
	buf_ptr = buf_ptr + sizeof(wmi_vdev_get_arp_stats_event_fixed_param) +
		  WMI_TLV_HDR_SIZE;
	connect_stats_event = (wmi_vdev_get_connectivity_check_stats *)buf_ptr;

	if (((connect_stats_event->tlv_header & 0xFFFF0000) >> 16 ==
	      WMITLV_TAG_STRUC_wmi_vdev_get_connectivity_check_stats)) {
		rsp.connect_stats_present = true;
		rsp.tcp_ack_recvd = connect_stats_event->tcp_ack_recvd;
		rsp.icmpv4_rsp_recvd = connect_stats_event->icmpv4_rsp_recvd;
		dp_debug("tcp_ack_recvd %d icmpv4_rsp_recvd %d",
			 connect_stats_event->tcp_ack_recvd,
			 connect_stats_event->icmpv4_rsp_recvd);
	}

	nb_ops->osif_dp_get_arp_stats_evt(psoc, &rsp);

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_dp_arp_stats_register_event_handler() - register event handler
 * @psoc: psoc handle
 *
 * Return: Return: 0 on success, failure code otherwise.
 */
static QDF_STATUS
target_if_dp_arp_stats_register_event_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;
	QDF_STATUS ret_val;

	if (!psoc) {
		dp_err("PSOC is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		dp_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	ret_val = wmi_unified_register_event_handler(wmi_handle,
				wmi_get_arp_stats_req_id,
				target_if_dp_get_arp_stats_event_handler,
				WMI_RX_WORK_CTX);
	if (QDF_IS_STATUS_ERROR(ret_val))
		dp_err("Failed to register event_handler");

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_dp_arp_stats_unregister_event_handler() - unregister event handler
 * @psoc: psoc handle
 *
 * Return: Return: 0 on success, failure code otherwise.
 */
static QDF_STATUS
target_if_dp_arp_stats_unregister_event_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	if (!psoc) {
		dp_err("PSOC is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		dp_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	wmi_unified_unregister_event_handler(wmi_handle,
					     wmi_get_arp_stats_req_id);
	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_dp_get_arp_req_stats() - send get arp stats request command to fw
 * @psoc: psoc handle
 * @req_buf: get arp stats request buffer
 *
 * Return: Return: 0 on success, failure code otherwise.
 */
static QDF_STATUS
target_if_dp_get_arp_req_stats(struct wlan_objmgr_psoc *psoc,
			       struct dp_get_arp_stats_params *req_buf)
{
	QDF_STATUS status;
	struct get_arp_stats *arp_stats;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc) {
		dp_err("PSOC is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		dp_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    req_buf->vdev_id,
						    WLAN_DP_ID);
	if (!vdev) {
		dp_err("Can't get vdev by vdev_id:%d", req_buf->vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	if (!wlan_cm_is_vdev_active(vdev)) {
		dp_debug("vdev id:%d is not started", req_buf->vdev_id);
		status = QDF_STATUS_E_INVAL;
		goto release_ref;
	}

	arp_stats = (struct get_arp_stats *)req_buf;
	status = wmi_unified_get_arp_stats_req(wmi_handle, arp_stats);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("failed to send get arp stats to FW");
release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_DP_ID);
	return status;
}

/**
 * target_if_dp_set_arp_req_stats() - send set arp stats request command to fw
 * @psoc: psoc handle
 * @req_buf: set srp stats request buffer
 *
 * Return: Return: 0 on success, failure code otherwise.
 */
static QDF_STATUS
target_if_dp_set_arp_req_stats(struct wlan_objmgr_psoc *psoc,
			       struct dp_set_arp_stats_params *req_buf)
{
	QDF_STATUS status;
	struct set_arp_stats *arp_stats;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc) {
		dp_err("PSOC is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		dp_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    req_buf->vdev_id,
						    WLAN_DP_ID);
	if (!vdev) {
		dp_err("Can't get vdev by vdev_id:%d", req_buf->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = wlan_vdev_is_up(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("vdev id:%d is not started", req_buf->vdev_id);
		status = QDF_STATUS_E_INVAL;
		goto release_ref;
	}
	arp_stats = (struct set_arp_stats *)req_buf;
	status = wmi_unified_set_arp_stats_req(wmi_handle, arp_stats);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("failed to set arp stats to FW");

release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_DP_ID);
	return status;
}

/**
 * target_if_dp_lro_config_cmd() - process the LRO config command
 * @psoc: Pointer to psoc handle
 * @dp_lro_cmd: Pointer to LRO configuration parameters
 *
 * This function sends down the LRO configuration parameters to
 * the firmware to enable LRO, sets the TCP flags and sets the
 * seed values for the toeplitz hash generation
 *
 * Return: QDF_STATUS_SUCCESS for success otherwise failure
 */
static QDF_STATUS
target_if_dp_lro_config_cmd(struct wlan_objmgr_psoc *psoc,
			    struct cdp_lro_hash_config *dp_lro_cmd)
{
	struct wmi_lro_config_cmd_t wmi_lro_cmd = {0};
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!dp_lro_cmd || !wmi_handle) {
		dp_err("wmi_handle or dp_lro_cmd is null");
		return QDF_STATUS_E_FAILURE;
	}

	wmi_lro_cmd.lro_enable = dp_lro_cmd->lro_enable;
	wmi_lro_cmd.tcp_flag = dp_lro_cmd->tcp_flag;
	wmi_lro_cmd.tcp_flag_mask = dp_lro_cmd->tcp_flag_mask;
	qdf_mem_copy(wmi_lro_cmd.toeplitz_hash_ipv4,
		     dp_lro_cmd->toeplitz_hash_ipv4,
		     LRO_IPV4_SEED_ARR_SZ * sizeof(uint32_t));
	qdf_mem_copy(wmi_lro_cmd.toeplitz_hash_ipv6,
		     dp_lro_cmd->toeplitz_hash_ipv6,
		     LRO_IPV6_SEED_ARR_SZ * sizeof(uint32_t));

	return wmi_unified_lro_config_cmd(wmi_handle, &wmi_lro_cmd);
}

/**
 * target_if_dp_send_dhcp_ind() - process set arp stats request command to fw
 * @vdev_id: vdev id
 * @dhcp_ind: DHCP indication.
 *
 * Return: 0 on success, failure code otherwise.
 */
static QDF_STATUS
target_if_dp_send_dhcp_ind(uint16_t vdev_id,
			   struct dp_dhcp_ind *dhcp_ind)
{
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	wmi_peer_set_param_cmd_fixed_param peer_set_param_fp = {0};
	QDF_STATUS status;

	psoc = wlan_objmgr_get_psoc_by_id(0, WLAN_PSOC_TARGET_IF_ID);
	if (!psoc) {
		dp_err("psoc null");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		dp_err("Unable to get wmi handle");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* fill in values */
	peer_set_param_fp.vdev_id = vdev_id;
	peer_set_param_fp.param_id = WMI_HOST_PEER_CRIT_PROTO_HINT_ENABLED;

	if (dhcp_ind->dhcp_start)
		peer_set_param_fp.param_value = 1;
	else
		peer_set_param_fp.param_value = 0;

	WMI_CHAR_ARRAY_TO_MAC_ADDR(dhcp_ind->peer_mac_addr.bytes,
				   &peer_set_param_fp.peer_macaddr);

	status = wmi_unified_process_dhcp_ind(wmi_handle,
					      &peer_set_param_fp);
	wlan_objmgr_psoc_release_ref(psoc, WLAN_PSOC_TARGET_IF_ID);

	return status;
}

void target_if_dp_register_tx_ops(struct wlan_dp_psoc_sb_ops *sb_ops)
{
	sb_ops->dp_arp_stats_register_event_handler =
		target_if_dp_arp_stats_register_event_handler;
	sb_ops->dp_arp_stats_unregister_event_handler =
		target_if_dp_arp_stats_unregister_event_handler;
	sb_ops->dp_get_arp_req_stats =
		target_if_dp_get_arp_req_stats;
	sb_ops->dp_set_arp_req_stats =
		target_if_dp_set_arp_req_stats;
	sb_ops->dp_lro_config_cmd = target_if_dp_lro_config_cmd;
	sb_ops->dp_send_dhcp_ind =
		target_if_dp_send_dhcp_ind;
}

void target_if_dp_register_rx_ops(struct wlan_dp_psoc_nb_ops *nb_ops)
{
}
