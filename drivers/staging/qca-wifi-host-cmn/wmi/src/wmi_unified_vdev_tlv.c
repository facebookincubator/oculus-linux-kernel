/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <osdep.h>
#include <qdf_module.h>
#include <qdf_list.h>
#include <qdf_platform.h>
#include <wmi_unified_vdev_tlv.h>
#include <wlan_vdev_mgr_tgt_if_tx_defs.h>

static QDF_STATUS
send_vdev_config_ratemask_cmd_tlv(struct wmi_unified *wmi_handle,
				  struct config_ratemask_params *param)
{
	wmi_vdev_config_ratemask_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}
	cmd = (wmi_vdev_config_ratemask_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_config_ratemask_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_vdev_config_ratemask_cmd_fixed_param));
	cmd->vdev_id = param->vdev_id;
	cmd->type = param->type;
	cmd->mask_lower32 = param->lower32;
	cmd->mask_higher32 = param->higher32;
	cmd->mask_lower32_2 = param->lower32_2;
	cmd->mask_higher32_2 = param->higher32_2;

	wmi_debug("vdev_id %d type %d lower32 0x%x lower32_2 0x%x",
		  cmd->vdev_id, cmd->type, cmd->mask_lower32,
		  cmd->mask_lower32_2);
	wmi_debug("higher32 0x%x higher32_2 0x%x", cmd->mask_higher32,
		  cmd->mask_higher32_2);
	wmi_mtrace(WMI_VDEV_RATEMASK_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_VDEV_RATEMASK_CMDID)) {
		wmi_err("Setting vdev ratemask failed");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
send_beacon_send_cmd_tlv(struct wmi_unified *wmi_handle,
			 struct beacon_params *param)
{
	QDF_STATUS ret;
	wmi_bcn_send_from_host_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	qdf_dma_addr_t dma_addr;
	uint32_t dtim_flag = 0;

	wmi_buf = wmi_buf_alloc(wmi_handle, sizeof(*cmd));
	if (!wmi_buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	if (param->is_dtim_count_zero) {
		dtim_flag |= WMI_BCN_SEND_DTIM_ZERO;
		if (param->is_bitctl_reqd) {
			/* deliver CAB traffic in next DTIM beacon */
			dtim_flag |= WMI_BCN_SEND_DTIM_BITCTL_SET;
		}
	}
	cmd = (wmi_bcn_send_from_host_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_bcn_send_from_host_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
				(wmi_bcn_send_from_host_cmd_fixed_param));
	cmd->vdev_id = param->vdev_id;
	cmd->data_len = qdf_nbuf_len(param->wbuf);
	cmd->frame_ctrl = param->frame_ctrl;
	cmd->dtim_flag = dtim_flag;
	dma_addr = qdf_nbuf_get_frag_paddr(param->wbuf, 0);
	cmd->frag_ptr_lo = qdf_get_lower_32_bits(dma_addr);
#if defined(HTT_PADDR64)
	cmd->frag_ptr_hi = qdf_get_upper_32_bits(dma_addr) & 0x1F;
#endif
	cmd->bcn_antenna = param->bcn_txant;

	wmi_mtrace(WMI_PDEV_SEND_BCN_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, wmi_buf, sizeof(*cmd),
				   WMI_PDEV_SEND_BCN_CMDID);
	if (ret != QDF_STATUS_SUCCESS) {
		wmi_err("Failed to send bcn: %d", ret);
		wmi_buf_free(wmi_buf);
	}

	return ret;
}

static QDF_STATUS
extract_tbttoffset_num_vdevs_tlv(struct wmi_unified *wmi_handle, void *evt_buf,
				 uint32_t *num_vdevs)
{
	WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *param_buf;
	wmi_tbtt_offset_event_fixed_param *tbtt_offset_event;
	uint32_t vdev_map;

	param_buf = (WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid tbtt update ext event buffer");
		return QDF_STATUS_E_INVAL;
	}
	tbtt_offset_event = param_buf->fixed_param;
	vdev_map = tbtt_offset_event->vdev_map;
	*num_vdevs = wmi_vdev_map_to_num_vdevs(vdev_map);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
send_peer_filter_set_tx_cmd_tlv(struct wmi_unified *wmi_handle,
				uint8_t macaddr[],
				struct set_tx_peer_filter *param)
{
	wmi_peer_tx_filter_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_FAILURE;

	cmd = (wmi_peer_tx_filter_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_tx_filter_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_peer_tx_filter_cmd_fixed_param));

	cmd->vdev_id = param->vdev_id;
	cmd->action = param->action;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(macaddr, &cmd->addr);

	wmi_mtrace(WMI_PEER_TX_FILTER_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PEER_TX_FILTER_CMDID)) {
		wmi_err("Failed to set neighbour rx param");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
send_vdev_set_neighbour_rx_cmd_tlv(struct wmi_unified *wmi_handle,
				   uint8_t macaddr[QDF_MAC_ADDR_SIZE],
				   struct set_neighbour_rx_params *param)
{
	wmi_vdev_filter_nrp_config_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}
	cmd = (wmi_vdev_filter_nrp_config_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_filter_nrp_config_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_vdev_filter_nrp_config_cmd_fixed_param));
	cmd->vdev_id = param->vdev_id;
	cmd->bssid_idx = param->idx;
	cmd->action = param->action;
	cmd->type = param->type;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(macaddr, &cmd->addr);
	cmd->flag = 0;

	wmi_mtrace(WMI_VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID)) {
		wmi_err("Failed to set neighbour rx param");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
extract_vdev_start_resp_tlv(struct wmi_unified *wmi_handle, void *evt_buf,
			    struct vdev_start_response *vdev_rsp)
{
	WMI_VDEV_START_RESP_EVENTID_param_tlvs *param_buf;
	wmi_vdev_start_response_event_fixed_param *ev;

	param_buf = (WMI_VDEV_START_RESP_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid start response event buffer");
		return QDF_STATUS_E_INVAL;
	}

	ev = param_buf->fixed_param;
	if (!ev) {
		wmi_err("Invalid start response event buffer");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_zero(vdev_rsp, sizeof(*vdev_rsp));

	vdev_rsp->vdev_id = ev->vdev_id;
	vdev_rsp->requestor_id = ev->requestor_id;
	switch (ev->resp_type) {
	case WMI_VDEV_START_RESP_EVENT:
		vdev_rsp->resp_type = WMI_HOST_VDEV_START_RESP_EVENT;
		break;
	case WMI_VDEV_RESTART_RESP_EVENT:
		vdev_rsp->resp_type = WMI_HOST_VDEV_RESTART_RESP_EVENT;
		break;
	default:
		wmi_err("Invalid start response event buffer");
		break;
	};
	vdev_rsp->status = ev->status;
	vdev_rsp->chain_mask = ev->chain_mask;
	vdev_rsp->smps_mode = ev->smps_mode;
	vdev_rsp->mac_id = ev->mac_id;
	vdev_rsp->cfgd_tx_streams = ev->cfgd_tx_streams;
	vdev_rsp->cfgd_rx_streams = ev->cfgd_rx_streams;
	vdev_rsp->max_allowed_tx_power = ev->max_allowed_tx_power;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
extract_vdev_delete_resp_tlv(struct wmi_unified *wmi_handle, void *evt_buf,
			     struct vdev_delete_response *delete_rsp)
{
	WMI_VDEV_DELETE_RESP_EVENTID_param_tlvs *param_buf;
	wmi_vdev_delete_resp_event_fixed_param *ev;

	param_buf = (WMI_VDEV_DELETE_RESP_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid vdev delete response event buffer");
		return QDF_STATUS_E_INVAL;
	}

	ev = param_buf->fixed_param;
	if (!ev) {
		wmi_err("Invalid vdev delete response event");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_zero(delete_rsp, sizeof(*delete_rsp));
	delete_rsp->vdev_id = ev->vdev_id;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS extract_vdev_peer_delete_all_response_event_tlv(
		wmi_unified_t wmi_hdl,
		void *evt_buf,
		struct peer_delete_all_response *param)
{
	WMI_VDEV_DELETE_ALL_PEER_RESP_EVENTID_param_tlvs *param_buf;
	wmi_vdev_delete_all_peer_resp_event_fixed_param *ev;

	param_buf = (WMI_VDEV_DELETE_ALL_PEER_RESP_EVENTID_param_tlvs *)evt_buf;

	ev = (wmi_vdev_delete_all_peer_resp_event_fixed_param *)
							param_buf->fixed_param;
	if (!ev) {
		wmi_err("Invalid peer_delete all response");
		return QDF_STATUS_E_FAILURE;
	}

	param->vdev_id = ev->vdev_id;
	param->status = ev->status;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
extract_vdev_stopped_param_tlv(struct wmi_unified *wmi_handle,
			       void *evt_buf, uint32_t *vdev_id)
{
	WMI_VDEV_STOPPED_EVENTID_param_tlvs *param_buf;
	wmi_vdev_stopped_event_fixed_param *resp_event;

	param_buf = (WMI_VDEV_STOPPED_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid event buffer");
		return QDF_STATUS_E_INVAL;
	}
	resp_event = param_buf->fixed_param;
	*vdev_id = resp_event->vdev_id;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS extract_ext_tbttoffset_num_vdevs_tlv(
						wmi_unified_t wmi_hdl,
						void *evt_buf,
						uint32_t *num_vdevs)
{
	WMI_TBTTOFFSET_EXT_UPDATE_EVENTID_param_tlvs *param_buf;
	wmi_tbtt_offset_ext_event_fixed_param *tbtt_offset_ext_event;

	param_buf = (WMI_TBTTOFFSET_EXT_UPDATE_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid tbtt update ext event buffer");
		return QDF_STATUS_E_INVAL;
	}
	tbtt_offset_ext_event = param_buf->fixed_param;

	*num_vdevs = tbtt_offset_ext_event->num_vdevs;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS extract_tbttoffset_update_params_tlv(
					wmi_unified_t wmi_hdl,
					void *evt_buf, uint8_t idx,
					struct tbttoffset_params *tbtt_param)
{
	WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *param_buf;
	wmi_tbtt_offset_event_fixed_param *tbtt_offset_event;
	uint32_t vdev_map;

	param_buf = (WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid tbtt update event buffer");
		return QDF_STATUS_E_INVAL;
	}

	tbtt_offset_event = param_buf->fixed_param;
	vdev_map = tbtt_offset_event->vdev_map;
	tbtt_param->vdev_id = wmi_vdev_map_to_vdev_id(vdev_map, idx);
	if (tbtt_param->vdev_id == WLAN_INVALID_VDEV_ID)
		return QDF_STATUS_E_INVAL;
	tbtt_param->tbttoffset =
		param_buf->tbttoffset_list[tbtt_param->vdev_id];
	if (param_buf->tbtt_qtime_low_us_list)
		tbtt_param->vdev_tbtt_qtime_lo =
		    param_buf->tbtt_qtime_low_us_list[tbtt_param->vdev_id];
	if (param_buf->tbtt_qtime_high_us_list)
		tbtt_param->vdev_tbtt_qtime_hi =
		    param_buf->tbtt_qtime_high_us_list[tbtt_param->vdev_id];

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS extract_ext_tbttoffset_update_params_tlv(
					wmi_unified_t wmi_hdl,
					void *evt_buf, uint8_t idx,
					struct tbttoffset_params *tbtt_param)
{
	WMI_TBTTOFFSET_EXT_UPDATE_EVENTID_param_tlvs *param_buf;
	wmi_tbtt_offset_info *tbtt_offset_info;

	param_buf = (WMI_TBTTOFFSET_EXT_UPDATE_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid tbtt update event buffer");
		return QDF_STATUS_E_INVAL;
	}
	tbtt_offset_info = &param_buf->tbtt_offset_info[idx];

	tbtt_param->vdev_id = tbtt_offset_info->vdev_id;
	tbtt_param->tbttoffset = tbtt_offset_info->tbttoffset;
	tbtt_param->vdev_tbtt_qtime_lo = tbtt_offset_info->tbtt_qtime_low_us;
	tbtt_param->vdev_tbtt_qtime_hi = tbtt_offset_info->tbtt_qtime_high_us;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS extract_muedca_params_tlv(wmi_unified_t wmi_hdl,
					    void *evt_buf,
					    struct muedca_params *muedca_param_list)
{
	WMI_MUEDCA_PARAMS_CONFIG_EVENTID_param_tlvs *param_buf;
	wmi_muedca_params_config_event_fixed_param *muedca_param;
	int i;

	param_buf = (WMI_MUEDCA_PARAMS_CONFIG_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid muedca evt buffer");
		return QDF_STATUS_E_INVAL;
	}
	muedca_param = param_buf->fixed_param;

	muedca_param_list->pdev_id = wmi_hdl->ops->
		convert_target_pdev_id_to_host(wmi_hdl,
					       muedca_param->pdev_id);
	for (i = 0; i < WMI_AC_MAX; i++) {
		muedca_param_list->muedca_aifsn[i] = muedca_param->aifsn[i] &
						      WMI_MUEDCA_PARAM_MASK;
		muedca_param_list->muedca_ecwmin[i] = muedca_param->ecwmin[i] &
						      WMI_MUEDCA_PARAM_MASK;
		muedca_param_list->muedca_ecwmax[i] = muedca_param->ecwmax[i] &
						      WMI_MUEDCA_PARAM_MASK;
		muedca_param_list->muedca_timer[i] = muedca_param->muedca_expiration_time[i] &
						      WMI_MUEDCA_PARAM_MASK;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_SR
static QDF_STATUS
vdev_param_sr_prohibit_send_tlv(struct wmi_unified *wmi_handle,
				struct sr_prohibit_param *param)
{
	wmi_vdev_param_enable_sr_prohibit_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}
	cmd = (wmi_vdev_param_enable_sr_prohibit_fixed_param *)
	      wmi_buf_data(buf);
	WMITLV_SET_HDR
		(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_param_enable_sr_prohibit_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
		(wmi_vdev_param_enable_sr_prohibit_fixed_param));
	cmd->vdev_id = param->vdev_id;
	cmd->tidmap = 0;
	cmd->prohibit_enable = param->sr_he_siga_val15_allowed;
	wmi_debug("SR Prohibit enabled: %d", cmd->prohibit_enable);

	wmi_mtrace(WMI_VDEV_PARAM_ENABLE_SR_PROHIBIT_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_VDEV_PARAM_ENABLE_SR_PROHIBIT_CMDID)) {
		wmi_err("Failed to set neighbour rx param");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static void wmi_vdev_attach_sr_cmds_tlv(struct wmi_ops *wmi_ops)
{
	if (!wmi_ops)
		return;

	wmi_ops->vdev_param_sr_prohibit_send =
				vdev_param_sr_prohibit_send_tlv;
}
#else
static inline void wmi_vdev_attach_sr_cmds_tlv(struct wmi_ops *wmi_ops)
{
}
#endif

static void
set_peer_tx_vlan_config(wmi_peer_config_vlan_cmd_fixed_param *cmd,
			struct peer_vlan_config_param *cfg)
{
	WMI_VLAN_TX_SET(cmd->peer_vlan_config_mask, cfg->tx_cmd);

	/* Setting insert_or_strip bit for Tx */
	WMI_TX_INSERT_OR_STRIP_SET(cmd->peer_vlan_config_mask,
				   cfg->tx_strip_insert);

	if (cfg->tx_strip_insert_inner && cfg->tx_strip_insert) {
	/* Setting the strip_insert_vlan_inner bit fo Tx */
		WMI_TX_STRIP_INSERT_VLAN_INNER_SET(cmd->peer_vlan_config_mask,
						   cfg->tx_strip_insert_inner);
	/* If Insert inner tag bit is set, then fill inner_tci */
		WMI_TX_INSERT_VLAN_INNER_TCI_SET(cmd->insert_vlan_tci,
						 cfg->insert_vlan_inner_tci);
	}

	if (cfg->tx_strip_insert_outer && cfg->tx_strip_insert) {
		/* Setting the strip_insert_vlan_outer bit for Tx */
		WMI_TX_STRIP_INSERT_VLAN_OUTER_SET(cmd->peer_vlan_config_mask,
						   cfg->tx_strip_insert_outer);
		/* If Insert outer tag bit is set, then fill outer_tci */
		WMI_TX_INSERT_VLAN_OUTER_TCI_SET(cmd->insert_vlan_tci,
						 cfg->insert_vlan_outer_tci);
	}
}

static void
wmi_set_peer_vlan_config(wmi_peer_config_vlan_cmd_fixed_param *cmd,
			 struct peer_vlan_config_param *param)
{
	/* Tx command - Check if cmd is Tx then configure Tx cmd */
	if (param->tx_cmd)
		set_peer_tx_vlan_config(cmd, param);

	/* Rx command - Check if cmd is Rx then configure Rx cmd */
	if (param->rx_cmd) {
		WMI_VLAN_RX_SET(cmd->peer_vlan_config_mask, param->rx_cmd);

		/* Setting the strip_vlan_c_tag_decap bit in RX */
		WMI_RX_STRIP_VLAN_C_TAG_SET(cmd->peer_vlan_config_mask,
					    param->rx_strip_c_tag);

		/* Setting the strip_vlan_s_tag_decap bit in RX */
		WMI_RX_STRIP_VLAN_S_TAG_SET(cmd->peer_vlan_config_mask,
					    param->rx_strip_s_tag);

		/* Setting the insert_vlan_c_tag_decap bit in RX */
		WMI_RX_INSERT_VLAN_C_TAG_SET(cmd->peer_vlan_config_mask,
					     param->rx_insert_c_tag);

		/* Setting the insert_vlan_s_tag_decap bit in RX */
		WMI_RX_INSERT_VLAN_S_TAG_SET(cmd->peer_vlan_config_mask,
					     param->rx_insert_s_tag);
	}
}

static QDF_STATUS
send_peer_vlan_config_cmd_tlv(wmi_unified_t wmi,
			      uint8_t peer_addr[QDF_MAC_ADDR_SIZE],
			      struct peer_vlan_config_param *param)
{
	wmi_peer_config_vlan_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_peer_config_vlan_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_config_vlan_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_peer_config_vlan_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);

	/* vdev id */
	cmd->vdev_id = param->vdev_id;

	wmi_set_peer_vlan_config(cmd, param);

	if (wmi_unified_cmd_send(wmi, buf, len, WMI_PEER_CONFIG_VLAN_CMDID)) {
		wmi_err("Failed to send peer hw vlan acceleration command");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void wmi_vdev_attach_tlv(struct wmi_unified *wmi_handle)
{
	struct wmi_ops *wmi_ops;

	if (!wmi_handle) {
		wmi_err("null wmi handle");
		return;
	}

	wmi_ops = wmi_handle->ops;
	wmi_ops->extract_vdev_delete_resp = extract_vdev_delete_resp_tlv;
	wmi_ops->extract_vdev_stopped_param = extract_vdev_stopped_param_tlv;
	wmi_ops->extract_vdev_start_resp = extract_vdev_start_resp_tlv;
	wmi_ops->extract_vdev_peer_delete_all_response_event =
				extract_vdev_peer_delete_all_response_event_tlv;
	wmi_ops->extract_tbttoffset_num_vdevs =
				extract_tbttoffset_num_vdevs_tlv;
	wmi_ops->extract_tbttoffset_update_params =
				extract_tbttoffset_update_params_tlv;
	wmi_ops->extract_ext_tbttoffset_update_params =
				extract_ext_tbttoffset_update_params_tlv;
	wmi_ops->extract_ext_tbttoffset_num_vdevs =
				extract_ext_tbttoffset_num_vdevs_tlv;
	wmi_ops->extract_muedca_params_handler =
				extract_muedca_params_tlv;
	wmi_ops->send_vdev_set_neighbour_rx_cmd =
				send_vdev_set_neighbour_rx_cmd_tlv;
	wmi_ops->send_beacon_send_cmd = send_beacon_send_cmd_tlv;
	wmi_ops->send_vdev_config_ratemask_cmd =
				send_vdev_config_ratemask_cmd_tlv;
	wmi_ops->send_peer_filter_set_tx_cmd = send_peer_filter_set_tx_cmd_tlv;
	wmi_ops->send_peer_vlan_config_cmd = send_peer_vlan_config_cmd_tlv;
	wmi_vdev_attach_sr_cmds_tlv(wmi_ops);
}
