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

#include <osdep.h>
#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_api.h"
#ifdef WLAN_MLO_MULTI_CHIP
#include "wmi_unified_11be_setup_api.h"
#endif
#include "wmi_unified_11be_tlv.h"

size_t vdev_create_mlo_params_size(struct vdev_create_params *param)
{
	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)param->mlo_mac))
		return WMI_TLV_HDR_SIZE;

	return sizeof(wmi_vdev_create_mlo_params) + WMI_TLV_HDR_SIZE;
}

uint8_t *vdev_create_add_mlo_params(uint8_t *buf_ptr,
				    struct vdev_create_params *param)
{
	wmi_vdev_create_mlo_params *mlo_params;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)param->mlo_mac)) {
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
		return buf_ptr + WMI_TLV_HDR_SIZE;
	}

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_vdev_create_mlo_params));
	buf_ptr += sizeof(uint32_t);

	mlo_params = (wmi_vdev_create_mlo_params *)buf_ptr;
	WMITLV_SET_HDR(&mlo_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_create_mlo_params,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_create_mlo_params));

	WMI_CHAR_ARRAY_TO_MAC_ADDR(param->mlo_mac, &mlo_params->mld_macaddr);

	wmi_debug("MLD Addr = "QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(param->mlo_mac));
	return buf_ptr + sizeof(wmi_vdev_create_mlo_params);
}

size_t vdev_start_mlo_params_size(struct vdev_start_params *req)
{
	size_t vdev_start_mlo_size;

	vdev_start_mlo_size = sizeof(wmi_vdev_start_mlo_params) +
			      WMI_TLV_HDR_SIZE +
			      (req->mlo_partner.num_links *
			      sizeof(wmi_partner_link_params)) +
			      WMI_TLV_HDR_SIZE;

	return vdev_start_mlo_size;
}

#ifdef WLAN_MCAST_MLO
static void vdev_start_add_mlo_mcast_params(uint32_t *mlo_flags,
					    struct vdev_start_params *req)
{
	WMI_MLO_FLAGS_SET_MCAST_VDEV(*mlo_flags,
				     req->mlo_flags.mlo_mcast_vdev);
}
#else
#define vdev_start_add_mlo_mcast_params(mlo_flags, req)
#endif

uint8_t *vdev_start_add_mlo_params(uint8_t *buf_ptr,
				   struct vdev_start_params *req)
{
	wmi_vdev_start_mlo_params *mlo_params;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_vdev_start_mlo_params));
	buf_ptr += sizeof(uint32_t);

	mlo_params = (wmi_vdev_start_mlo_params *)buf_ptr;
	WMITLV_SET_HDR(&mlo_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_start_mlo_params,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_start_mlo_params));

	mlo_params->mlo_flags.mlo_flags = 0;
	WMI_MLO_FLAGS_SET_ENABLED(mlo_params->mlo_flags.mlo_flags,
				  req->mlo_flags.mlo_enabled);
	WMI_MLO_FLAGS_SET_ASSOC_LINK(mlo_params->mlo_flags.mlo_flags,
				     req->mlo_flags.mlo_assoc_link);
	WMI_MLO_FLAGS_SET_LINK_ADD(mlo_params->mlo_flags.mlo_flags,
				   req->mlo_flags.mlo_link_add);
	mlo_params->mlo_flags.emlsr_support = req->mlo_flags.emlsr_support;

	vdev_start_add_mlo_mcast_params(&mlo_params->mlo_flags.mlo_flags,
					req);

	return buf_ptr + sizeof(wmi_vdev_start_mlo_params);
}

uint8_t *vdev_start_add_ml_partner_links(uint8_t *buf_ptr,
					 struct vdev_start_params *req)
{
	wmi_partner_link_params *ml_partner_link;
	struct mlo_vdev_start_partner_links *req_partner;
	uint8_t i;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		      (req->mlo_partner.num_links *
		      sizeof(wmi_partner_link_params)));
	buf_ptr += sizeof(uint32_t);

	req_partner = &req->mlo_partner;
	ml_partner_link = (wmi_partner_link_params *)buf_ptr;
	for (i = 0; i < req->mlo_partner.num_links; i++) {
		WMITLV_SET_HDR(&ml_partner_link->tlv_header,
			       WMITLV_TAG_STRUC_wmi_partner_link_params,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_partner_link_params));
		ml_partner_link->vdev_id = req_partner->partner_info[i].vdev_id;
		ml_partner_link->hw_link_id =
				req_partner->partner_info[i].hw_mld_link_id;
		WMI_CHAR_ARRAY_TO_MAC_ADDR(req_partner->partner_info[i].mac_addr,
					   &ml_partner_link->vdev_macaddr);
		ml_partner_link++;
	}

	return buf_ptr +
		(req->mlo_partner.num_links *
		 sizeof(wmi_partner_link_params));
}

size_t bcn_tmpl_mlo_param_size(struct beacon_tmpl_params *param)
{
	return WMI_TLV_HDR_SIZE;
}

uint8_t *bcn_tmpl_add_ml_partner_links(uint8_t *buf_ptr,
				       struct beacon_tmpl_params *param)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

size_t bcn_tmpl_ml_info_size(struct beacon_tmpl_params *param)
{
	return (WMI_TLV_HDR_SIZE + sizeof(wmi_bcn_tmpl_ml_info));
}

uint8_t *bcn_tmpl_add_ml_info(uint8_t *buf_ptr,
			      struct beacon_tmpl_params *param)
{
	wmi_bcn_tmpl_ml_info *ml_info;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_bcn_tmpl_ml_info));
	buf_ptr += WMI_TLV_HDR_SIZE;

	ml_info = (wmi_bcn_tmpl_ml_info *)buf_ptr;

	WMITLV_SET_HDR(&ml_info->tlv_header,
		       WMITLV_TAG_STRUC_wmi_bcn_tmpl_ml_info,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_bcn_tmpl_ml_info));

	ml_info->hw_link_id = param->cu_ml_info.hw_link_id;
	ml_info->cu_vdev_map_cat1_lo = param->cu_ml_info.cu_vdev_map_cat1_lo;
	ml_info->cu_vdev_map_cat1_hi = param->cu_ml_info.cu_vdev_map_cat1_hi;
	ml_info->cu_vdev_map_cat2_lo = param->cu_ml_info.cu_vdev_map_cat2_lo;
	ml_info->cu_vdev_map_cat2_hi = param->cu_ml_info.cu_vdev_map_cat2_hi;

	return buf_ptr + sizeof(wmi_bcn_tmpl_ml_info);
}

size_t prb_resp_tmpl_ml_info_size(struct wmi_probe_resp_params *param)
{
	return (WMI_TLV_HDR_SIZE + sizeof(wmi_prb_resp_tmpl_ml_info));
}

uint8_t *prb_resp_tmpl_add_ml_info(uint8_t *buf_ptr,
				   struct wmi_probe_resp_params *param)
{
	wmi_prb_resp_tmpl_ml_info *ml_info;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_prb_resp_tmpl_ml_info));
	buf_ptr += WMI_TLV_HDR_SIZE;

	ml_info = (wmi_prb_resp_tmpl_ml_info *)buf_ptr;

	WMITLV_SET_HDR(&ml_info->tlv_header,
		       WMITLV_TAG_STRUC_wmi_prb_resp_tmpl_ml_info,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_prb_resp_tmpl_ml_info));

	ml_info->hw_link_id = param->cu_ml_info.hw_link_id;
	ml_info->cu_vdev_map_cat1_lo = param->cu_ml_info.cu_vdev_map_cat1_lo;
	ml_info->cu_vdev_map_cat1_hi = param->cu_ml_info.cu_vdev_map_cat1_hi;
	ml_info->cu_vdev_map_cat2_lo = param->cu_ml_info.cu_vdev_map_cat2_lo;
	ml_info->cu_vdev_map_cat2_hi = param->cu_ml_info.cu_vdev_map_cat2_hi;

	return buf_ptr + sizeof(wmi_prb_resp_tmpl_ml_info);
}

size_t peer_create_mlo_params_size(struct peer_create_params *req)
{
	return sizeof(wmi_peer_create_mlo_params) + WMI_TLV_HDR_SIZE;
}

uint8_t *peer_create_add_mlo_params(uint8_t *buf_ptr,
				    struct peer_create_params *req)
{
	wmi_peer_create_mlo_params *mlo_params;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_peer_create_mlo_params));
	buf_ptr += sizeof(uint32_t);

	mlo_params = (wmi_peer_create_mlo_params *)buf_ptr;
	WMITLV_SET_HDR(&mlo_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_create_mlo_params,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_peer_create_mlo_params));

	mlo_params->mlo_flags.mlo_flags = 0;
	WMI_MLO_FLAGS_SET_ENABLED(mlo_params->mlo_flags.mlo_flags,
				  req->mlo_enabled);

	return buf_ptr + sizeof(wmi_peer_create_mlo_params);
}

size_t peer_assoc_mlo_params_size(struct peer_assoc_params *req)
{
	size_t peer_assoc_mlo_size = sizeof(wmi_peer_assoc_mlo_params) +
			WMI_TLV_HDR_SIZE +
			(req->ml_links.num_links *
			sizeof(wmi_peer_assoc_mlo_partner_link_params)) +
			WMI_TLV_HDR_SIZE;

	return peer_assoc_mlo_size;
}

uint8_t *peer_assoc_add_mlo_params(uint8_t *buf_ptr,
				   struct peer_assoc_params *req)
{
	wmi_peer_assoc_mlo_params *mlo_params;

	/* Add WMI peer assoc mlo params */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_peer_assoc_mlo_params));
	buf_ptr += sizeof(uint32_t);

	mlo_params = (wmi_peer_assoc_mlo_params *)buf_ptr;
	WMITLV_SET_HDR(&mlo_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_assoc_mlo_params,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_peer_assoc_mlo_params));

	mlo_params->mlo_flags.mlo_flags = 0;
	WMI_MLO_FLAGS_SET_ENABLED(mlo_params->mlo_flags.mlo_flags,
				  req->mlo_params.mlo_enabled);
	WMI_MLO_FLAGS_SET_ASSOC_LINK(mlo_params->mlo_flags.mlo_flags,
				     req->mlo_params.mlo_assoc_link);
	WMI_MLO_FLAGS_SET_PRIMARY_UMAC(mlo_params->mlo_flags.mlo_flags,
				       req->mlo_params.mlo_primary_umac);
	WMI_MLO_FLAGS_SET_LINK_INDEX_VALID(mlo_params->mlo_flags.mlo_flags,
					   req->mlo_params.mlo_logical_link_index_valid);
	WMI_MLO_FLAGS_SET_PEER_ID_VALID(mlo_params->mlo_flags.mlo_flags,
					req->mlo_params.mlo_peer_id_valid);
	mlo_params->mlo_flags.emlsr_support = req->mlo_params.emlsr_support;

	mlo_params->mlo_flags.mlo_force_link_inactive =
			req->mlo_params.mlo_force_link_inactive;

	WMI_CHAR_ARRAY_TO_MAC_ADDR(req->mlo_params.mld_mac,
				   &mlo_params->mld_macaddr);
	mlo_params->logical_link_index = req->mlo_params.logical_link_index;
	mlo_params->mld_peer_id = req->mlo_params.ml_peer_id;

	mlo_params->ieee_link_id = req->mlo_params.ieee_link_id;
	mlo_params->emlsr_trans_timeout_us = req->mlo_params.trans_timeout_us;
	mlo_params->emlsr_trans_delay_us = req->mlo_params.emlsr_trans_delay_us;
	mlo_params->emlsr_padding_delay_us = req->mlo_params.emlsr_pad_delay_us;

	mlo_params->msd_dur_us = req->mlo_params.medium_sync_duration;
	mlo_params->msd_ofdm_ed_thr =
			req->mlo_params.medium_sync_ofdm_ed_thresh;
	mlo_params->msd_max_num_txops =
			req->mlo_params.medium_sync_max_txop_num;

	return buf_ptr + sizeof(wmi_peer_assoc_mlo_params);
}

uint8_t *peer_assoc_add_ml_partner_links(uint8_t *buf_ptr,
					 struct peer_assoc_params *req)
{
	wmi_peer_assoc_mlo_partner_link_params *ml_partner_link;
	struct ml_partner_info *partner_info;
	uint8_t i;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       (req->ml_links.num_links *
		       sizeof(wmi_peer_assoc_mlo_partner_link_params)));
	buf_ptr += sizeof(uint32_t);

	ml_partner_link = (wmi_peer_assoc_mlo_partner_link_params *)buf_ptr;
	partner_info = req->ml_links.partner_info;
	for (i = 0; i < req->ml_links.num_links; i++) {
		WMITLV_SET_HDR(&ml_partner_link->tlv_header,
			       WMITLV_TAG_STRUC_wmi_peer_assoc_mlo_partner_link_params,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_peer_assoc_mlo_partner_link_params));
		ml_partner_link->vdev_id = partner_info[i].vdev_id;
		ml_partner_link->hw_mld_link_id = partner_info[i].hw_mld_link_id;
		ml_partner_link++;
	}

	return buf_ptr +
	       (req->ml_links.num_links *
		sizeof(wmi_peer_assoc_mlo_partner_link_params));
}

size_t peer_delete_mlo_params_size(struct peer_delete_cmd_params *req)
{
	if (!req->hw_link_id_bitmap)
		return WMI_TLV_HDR_SIZE;

	return sizeof(wmi_peer_delete_mlo_params) + WMI_TLV_HDR_SIZE;
}

uint8_t *peer_delete_add_mlo_params(uint8_t *buf_ptr,
				    struct peer_delete_cmd_params *req)
{
	wmi_peer_delete_mlo_params *mlo_params;

	if (!req->hw_link_id_bitmap) {
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
		return buf_ptr + WMI_TLV_HDR_SIZE;
	}

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_peer_delete_mlo_params));
	buf_ptr += sizeof(uint32_t);

	mlo_params = (wmi_peer_delete_mlo_params *)buf_ptr;
	WMITLV_SET_HDR(&mlo_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_delete_mlo_params,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_peer_delete_mlo_params));
	mlo_params->mlo_hw_link_id_bitmap = req->hw_link_id_bitmap;
	return buf_ptr + sizeof(wmi_peer_delete_mlo_params);
}

/**
 * force_mode_host_to_fw() - translate force mode for MLO link set active
 *  command
 * @host_mode: force mode defined by host
 * @fw_mode: buffer to store force mode defined by FW
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_INVAL otherwise
 */
static inline QDF_STATUS
force_mode_host_to_fw(enum mlo_link_force_mode host_mode,
		      WMI_MLO_LINK_FORCE_MODE *fw_mode)
{
	switch (host_mode) {
	case MLO_LINK_FORCE_MODE_ACTIVE:
		*fw_mode = WMI_MLO_LINK_FORCE_ACTIVE;
		break;
	case MLO_LINK_FORCE_MODE_INACTIVE:
		*fw_mode = WMI_MLO_LINK_FORCE_INACTIVE;
		break;
	case MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE:
		*fw_mode = WMI_MLO_LINK_FORCE_ACTIVE_INACTIVE;
		break;
	case MLO_LINK_FORCE_MODE_ACTIVE_NUM:
		*fw_mode = WMI_MLO_LINK_FORCE_ACTIVE_LINK_NUM;
		break;
	case MLO_LINK_FORCE_MODE_INACTIVE_NUM:
		*fw_mode = WMI_MLO_LINK_FORCE_INACTIVE_LINK_NUM;
		break;
	case MLO_LINK_FORCE_MODE_NO_FORCE:
		*fw_mode = WMI_MLO_LINK_NO_FORCE;
		break;
	default:
		wmi_err("Invalid force mode: %d", host_mode);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * force_reason_host_to_fw() - translate force reason for MLO link set active
 *  command
 * @host_reason: force reason defined by host
 * @fw_reason: buffer to store force reason defined by FW
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_INVAL otherwise
 */
static inline QDF_STATUS
force_reason_host_to_fw(enum mlo_link_force_reason host_reason,
			WMI_MLO_LINK_FORCE_REASON *fw_reason)
{
	switch (host_reason) {
	case MLO_LINK_FORCE_REASON_CONNECT:
		*fw_reason = WMI_MLO_LINK_FORCE_REASON_NEW_CONNECT;
		break;
	case MLO_LINK_FORCE_REASON_DISCONNECT:
		*fw_reason =  WMI_MLO_LINK_FORCE_REASON_NEW_DISCONNECT;
		break;
	case MLO_LINK_FORCE_REASON_LINK_REMOVAL:
		*fw_reason =  WMI_MLO_LINK_FORCE_REASON_LINK_REMOVAL;
		break;
	default:
		wmi_err("Invalid force reason: %d", host_reason);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_mlo_link_set_active_cmd_tlv() - send mlo link set active command
 * @wmi_handle: wmi handle
 * @param: Pointer to mlo link set active param
 *
 * Return: QDF_STATUS_SUCCESS for success or QDF_STATUS_E_* for error
 */
static QDF_STATUS
send_mlo_link_set_active_cmd_tlv(wmi_unified_t wmi_handle,
				 struct mlo_link_set_active_param *param)
{
	QDF_STATUS status;
	wmi_mlo_link_set_active_cmd_fixed_param *cmd;
	wmi_mlo_set_active_link_number_param *link_num_param;
	uint32_t *vdev_bitmap;
	uint32_t num_link_num_param = 0, num_vdev_bitmap = 0, tlv_len;
	uint32_t num_inactive_vdev_bitmap = 0;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t len;
	int i;
	WMITLV_TAG_ID tag_id;
	WMI_MLO_LINK_FORCE_MODE force_mode;
	WMI_MLO_LINK_FORCE_REASON force_reason;

	if (!param->num_vdev_bitmap && !param->num_link_entry) {
		wmi_err("No entry is provided vdev bit map %d link entry %d",
			param->num_vdev_bitmap,
			param->num_link_entry);
		return QDF_STATUS_E_INVAL;
	}

	status = force_mode_host_to_fw(param->force_mode, &force_mode);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	status = force_reason_host_to_fw(param->reason, &force_reason);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	switch (force_mode) {
	case WMI_MLO_LINK_FORCE_ACTIVE_LINK_NUM:
	case WMI_MLO_LINK_FORCE_INACTIVE_LINK_NUM:
		num_link_num_param = param->num_link_entry;
		fallthrough;
	case WMI_MLO_LINK_FORCE_ACTIVE:
	case WMI_MLO_LINK_FORCE_INACTIVE:
	case WMI_MLO_LINK_NO_FORCE:
		num_vdev_bitmap = param->num_vdev_bitmap;
		break;
	case WMI_MLO_LINK_FORCE_ACTIVE_INACTIVE:
		num_vdev_bitmap = param->num_vdev_bitmap;
		num_inactive_vdev_bitmap = param->num_inactive_vdev_bitmap;
		break;
	default:
		wmi_err("Invalid force reason: %d", force_mode);
		return QDF_STATUS_E_INVAL;
	}

	len = sizeof(*cmd) +
	      WMI_TLV_HDR_SIZE + sizeof(*link_num_param) * num_link_num_param +
	      WMI_TLV_HDR_SIZE + sizeof(*vdev_bitmap) * num_vdev_bitmap;
	if (force_mode == WMI_MLO_LINK_FORCE_ACTIVE_INACTIVE)
		len += WMI_TLV_HDR_SIZE +
		sizeof(*vdev_bitmap) * num_inactive_vdev_bitmap;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_mlo_link_set_active_cmd_fixed_param *)buf_ptr;
	tlv_len = WMITLV_GET_STRUCT_TLVLEN
			(wmi_mlo_link_set_active_cmd_fixed_param);

	tag_id = WMITLV_TAG_STRUC_wmi_mlo_link_set_active_cmd_fixed_param;
	WMITLV_SET_HDR(&cmd->tlv_header, tag_id, tlv_len);
	cmd->force_mode = force_mode;
	cmd->reason = force_reason;
	wmi_debug("mode %d reason %d num_link_num_param %d num_vdev_bitmap %d inactive %d",
		  cmd->force_mode, cmd->reason, num_link_num_param,
		  num_vdev_bitmap, num_inactive_vdev_bitmap);
	buf_ptr += sizeof(*cmd);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(*link_num_param) * num_link_num_param);
	buf_ptr += WMI_TLV_HDR_SIZE;

	if (num_link_num_param) {
		link_num_param =
			(wmi_mlo_set_active_link_number_param *)buf_ptr;
		tlv_len = WMITLV_GET_STRUCT_TLVLEN
				(wmi_mlo_set_active_link_number_param);
		for (i = 0; i < num_link_num_param; i++) {
			WMITLV_SET_HDR(&link_num_param->tlv_header, 0, tlv_len);
			link_num_param->num_of_link =
				param->link_num[i].num_of_link;
			link_num_param->vdev_type =
				param->link_num[i].vdev_type;
			link_num_param->vdev_subtype =
				param->link_num[i].vdev_subtype;
			link_num_param->home_freq =
				param->link_num[i].home_freq;
			wmi_debug("entry[%d]: num_of_link %d vdev type %d subtype %d freq %d",
				  i, link_num_param->num_of_link,
				  link_num_param->vdev_type,
				  link_num_param->vdev_subtype,
				  link_num_param->home_freq);
			link_num_param++;
		}

		buf_ptr += sizeof(*link_num_param) * num_link_num_param;
	}

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       sizeof(*vdev_bitmap) * num_vdev_bitmap);
	buf_ptr += WMI_TLV_HDR_SIZE;

	if (num_vdev_bitmap) {
		vdev_bitmap = (A_UINT32 *)(buf_ptr);
		for (i = 0; i < num_vdev_bitmap; i++) {
			vdev_bitmap[i] = param->vdev_bitmap[i];
			wmi_debug("entry[%d]: vdev_id_bitmap 0x%x ",
				  i, vdev_bitmap[i]);
		}

		buf_ptr += sizeof(*vdev_bitmap) * num_vdev_bitmap;
	}
	if (force_mode == WMI_MLO_LINK_FORCE_ACTIVE_INACTIVE) {
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
			       sizeof(*vdev_bitmap) *
			       num_inactive_vdev_bitmap);
		buf_ptr += WMI_TLV_HDR_SIZE;

		if (num_inactive_vdev_bitmap) {
			vdev_bitmap = (A_UINT32 *)(buf_ptr);
			for (i = 0; i < num_inactive_vdev_bitmap; i++) {
				vdev_bitmap[i] =
					param->inactive_vdev_bitmap[i];
				wmi_debug("entry[%d]: inactive_vdev_id_bitmap 0x%x ",
					  i, vdev_bitmap[i]);
			}

			buf_ptr += sizeof(*vdev_bitmap) *
				num_inactive_vdev_bitmap;
		}
	}

	wmi_mtrace(WMI_MLO_LINK_SET_ACTIVE_CMDID, 0, cmd->force_mode);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_MLO_LINK_SET_ACTIVE_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("Failed to send MLO link set active command to FW: %d",
			status);
		wmi_buf_free(buf);
	}

	return status;
}

/**
 * extract_mlo_link_set_active_resp_tlv() - extract mlo link set active resp
 *  from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @resp: Pointer to hold mlo link set active resp
 *
 * Return: QDF_STATUS_SUCCESS for success or QDF_STATUS_E_* for error
 */
static QDF_STATUS
extract_mlo_link_set_active_resp_tlv(wmi_unified_t wmi_handle, void *evt_buf,
				     struct mlo_link_set_active_resp *resp)
{
	wmi_mlo_link_set_active_resp_event_fixed_param *evt;
	WMI_MLO_LINK_SET_ACTIVE_RESP_EVENTID_param_tlvs *param_buf;
	uint32_t entry_num, *bitmap;
	int i;

	param_buf = evt_buf;
	if (!param_buf || !resp) {
		wmi_err("Invalid param");
		return QDF_STATUS_E_INVAL;
	}

	evt = param_buf->fixed_param;
	resp->status = evt->status;
	wmi_debug("status: %u", resp->status);

	bitmap = param_buf->force_active_vdev_bitmap;
	entry_num = qdf_min(param_buf->num_force_active_vdev_bitmap,
			    (uint32_t)MLO_VDEV_BITMAP_SZ);
	resp->active_sz = entry_num;
	for (i = 0; i < entry_num; i++) {
		resp->active[i] = bitmap[i];
		wmi_debug("active[%d]: 0x%x", i, resp->active[i]);
	}

	bitmap = param_buf->force_inactive_vdev_bitmap;
	entry_num = qdf_min(param_buf->num_force_inactive_vdev_bitmap,
			    (uint32_t)MLO_VDEV_BITMAP_SZ);
	resp->inactive_sz = entry_num;
	for (i = 0; i < entry_num; i++) {
		resp->inactive[i] = bitmap[i];
		wmi_debug("inactive[%d]: 0x%x", i, resp->inactive[i]);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_mlo_link_removal_cmd_tlv() - Send WMI command for MLO link removal
 * @wmi_handle: wmi handle
 * @params: MLO link removal command parameters
 *
 * Return: QDF_STATUS_SUCCESS of operation
 */
static QDF_STATUS send_mlo_link_removal_cmd_tlv(
	wmi_unified_t wmi_handle,
	const struct mlo_link_removal_cmd_params *params)
{
	wmi_mlo_link_removal_cmd_fixed_param *fixed_params;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t buf_len = 0;
	uint32_t ie_len_aligned = 0;
	QDF_STATUS ret;

	if (!params) {
		wmi_err("command params is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ie_len_aligned = roundup(params->reconfig_ml_ie_size, sizeof(uint32_t));

	buf_len = sizeof(wmi_mlo_link_removal_cmd_fixed_param) +
		  WMI_TLV_HDR_SIZE + ie_len_aligned;

	buf = wmi_buf_alloc(wmi_handle, buf_len);
	if (!buf) {
		wmi_err("wmi buf alloc failed for link removal cmd: psoc (%pK) vdev(%u)",
			wmi_handle->soc->wmi_psoc, params->vdev_id);
		return QDF_STATUS_E_NOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);

	/* Populate fixed params TLV */
	fixed_params = (wmi_mlo_link_removal_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&fixed_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_mlo_link_removal_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			   wmi_mlo_link_removal_cmd_fixed_param));
	fixed_params->vdev_id = params->vdev_id;
	fixed_params->reconfig_ml_ie_num_bytes_valid =
		params->reconfig_ml_ie_size;
	buf_ptr += sizeof(*fixed_params);

	/* Populate the array of bytes TLV */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ie_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Populate ML reconfiguration element in raw bytes */
	qdf_mem_copy(buf_ptr, params->reconfig_ml_ie,
		     params->reconfig_ml_ie_size);

	wmi_mtrace(WMI_MLO_LINK_REMOVAL_CMDID, fixed_params->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, buf_len,
				   WMI_MLO_LINK_REMOVAL_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send MLO link removal cmd: psoc (%pK) vdev(%u)",
			wmi_handle->soc->wmi_psoc, params->vdev_id);
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * extract_mlo_link_removal_evt_fixed_param_tlv() - Extract fixed parameters TLV
 * from the MLO link removal WMI  event
 * @wmi_handle: wmi handle
 * @buf: pointer to event buffer
 * @params: MLO link removal event parameters
 *
 * Return: QDF_STATUS of operation
 */
static QDF_STATUS
extract_mlo_link_removal_evt_fixed_param_tlv(
	struct wmi_unified *wmi_handle,
	void *buf,
	struct mlo_link_removal_evt_params *params)
{
	WMI_MLO_LINK_REMOVAL_EVENTID_param_tlvs *param_buf = buf;
	wmi_mlo_link_removal_evt_fixed_param *ev;

	if (!param_buf) {
		wmi_err_rl("Param_buf is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!params) {
		wmi_err_rl("params is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ev = param_buf->fixed_param;
	params->vdev_id = ev->vdev_id;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_mlo_link_removal_tbtt_update_tlv() - Extract TBTT update TLV
 * from the MLO link removal WMI  event
 * @wmi_handle: wmi handle
 * @buf: pointer to event buffer
 * @tbtt_info: TBTT information to be populated
 *
 * Return: QDF_STATUS of operation
 */
static QDF_STATUS
extract_mlo_link_removal_tbtt_update_tlv(
	struct wmi_unified *wmi_handle,
	void *buf,
	struct mlo_link_removal_tbtt_info *tbtt_info)
{
	WMI_MLO_LINK_REMOVAL_EVENTID_param_tlvs *param_buf = buf;
	wmi_mlo_link_removal_tbtt_update *tlv;

	if (!param_buf) {
		wmi_err_rl("Param_buf is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!tbtt_info) {
		wmi_err_rl("Writable argument is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	tlv = param_buf->tbtt_update;

	tbtt_info->tbtt_count = tlv->tbtt_count;
	tbtt_info->tsf = ((uint64_t)tlv->tsf_high << 32) | tlv->tsf_low;
	tbtt_info->qtimer_reading =
		((uint64_t)tlv->qtimer_ts_high << 32) | tlv->qtimer_ts_low;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_mgmt_rx_mlo_link_removal_info_tlv() - Extract MLO link removal info
 * from MGMT Rx event
 * @wmi_handle: wmi handle
 * @buf: event buffer
 * @link_removal_info: link removal information array to be populated
 * @num_link_removal_info: Number of elements in @link_removal_info
 *
 * Return: QDF_STATUS of operation
 */
static QDF_STATUS
extract_mgmt_rx_mlo_link_removal_info_tlv(
	struct wmi_unified *wmi_handle,
	void *buf,
	struct mgmt_rx_mlo_link_removal_info *link_removal_info,
	int num_link_removal_info)
{
	WMI_MGMT_RX_EVENTID_param_tlvs *param_buf = buf;
	wmi_mlo_link_removal_tbtt_count *tlv_arr;
	int tlv_idx = 0;
	struct mgmt_rx_mlo_link_removal_info *info;

	if (!param_buf) {
		wmi_err_rl("Param_buf is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!link_removal_info) {
		wmi_err_rl("Writable argument is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (num_link_removal_info != param_buf->num_link_removal_tbtt_count) {
		wmi_err_rl("link_removal_info array size (%d) is not equal to"
			   "number of corresponding TLVs(%d) present in event",
			   num_link_removal_info,
			   param_buf->num_link_removal_tbtt_count);
		return QDF_STATUS_E_RANGE;
	}

	tlv_arr = param_buf->link_removal_tbtt_count;
	for (; tlv_idx < param_buf->num_link_removal_tbtt_count; tlv_idx++) {
		info = &link_removal_info[tlv_idx];

		info->hw_link_id = WMI_MLO_LINK_REMOVAL_GET_LINKID(
					tlv_arr[tlv_idx].tbtt_info);
		info->vdev_id = WMI_MLO_LINK_REMOVAL_GET_VDEVID(
					tlv_arr[tlv_idx].tbtt_info);
		info->tbtt_count = WMI_MLO_LINK_REMOVAL_GET_TBTT_COUNT(
					tlv_arr[tlv_idx].tbtt_info);
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE
size_t peer_assoc_t2lm_params_size(struct peer_assoc_params *req)
{
	size_t peer_assoc_t2lm_size = WMI_TLV_HDR_SIZE +
		(req->t2lm_params.num_dir * T2LM_MAX_NUM_TIDS *
		 (sizeof(wmi_peer_assoc_tid_to_link_map)));

	return peer_assoc_t2lm_size;
}

static void peer_assoc_populate_t2lm_tlv(wmi_peer_assoc_tid_to_link_map *cmd,
				  struct wlan_host_t2lm_of_tids *t2lm,
				  uint8_t tid_num)
{
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_assoc_tid_to_link_map,
		       WMITLV_GET_STRUCT_TLVLEN(
				   wmi_peer_assoc_tid_to_link_map));

	/* Populate TID number */
	WMI_TID_TO_LINK_MAP_TID_NUM_SET(cmd->tid_to_link_map_info, tid_num);

	/* Populate the direction */
	WMI_TID_TO_LINK_MAP_DIR_SET(cmd->tid_to_link_map_info,
				    t2lm->direction);

	/* Populate the default link mapping value */
	WMI_TID_TO_LINK_MAP_DEFAULT_MAPPING_SET(
			cmd->tid_to_link_map_info,
			t2lm->default_link_mapping);

	/* Populate the T2LM provisioned links for the corresponding TID
	 * number.
	 */
	WMI_TID_TO_LINK_MAP_LINK_MASK_SET(
			cmd->tid_to_link_map_info,
			t2lm->t2lm_provisioned_links[tid_num]);

	wmi_debug("Add T2LM TLV: tid_to_link_map_info:%x",
		  cmd->tid_to_link_map_info);
}

uint8_t *peer_assoc_add_tid_to_link_map(uint8_t *buf_ptr,
					struct peer_assoc_params *req)
{
	struct wmi_host_tid_to_link_map_params *t2lm_params = &req->t2lm_params;
	wmi_peer_assoc_tid_to_link_map *cmd;
	uint8_t dir = 0;
	uint8_t tid_num = 0;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       (req->t2lm_params.num_dir * T2LM_MAX_NUM_TIDS *
		       sizeof(wmi_peer_assoc_tid_to_link_map)));
	buf_ptr += sizeof(uint32_t);

	for (dir = 0; dir < t2lm_params->num_dir; dir++) {
		wmi_debug("Add T2LM TLV for peer: " QDF_MAC_ADDR_FMT " direction:%d",
				QDF_MAC_ADDR_REF(t2lm_params->peer_macaddr),
				t2lm_params->t2lm_info[dir].direction);
		for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
			cmd = (wmi_peer_assoc_tid_to_link_map *)buf_ptr;
			peer_assoc_populate_t2lm_tlv(
					cmd, &t2lm_params->t2lm_info[dir],
					tid_num);
			buf_ptr += sizeof(wmi_peer_assoc_tid_to_link_map);
		}
	}

	return buf_ptr;
}

static QDF_STATUS
send_link_state_request_cmd_tlv(wmi_unified_t wmi_handle,
				struct wmi_host_link_state_params *params)
{
	wmi_mlo_vdev_get_link_info_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	uint32_t buf_len = 0;

	buf_len = sizeof(wmi_mlo_vdev_get_link_info_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, buf_len);
	if (!buf) {
		wmi_err("wmi buf alloc failed for vdev id %d while link state cmd send: ",
			params->vdev_id);
		return QDF_STATUS_E_NOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_mlo_vdev_get_link_info_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_mlo_vdev_get_link_info_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
		wmi_mlo_vdev_get_link_info_cmd_fixed_param));

	cmd->vdev_id = params->vdev_id;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(params->mld_mac, &cmd->mld_macaddr);
	buf_ptr += sizeof(wmi_mlo_vdev_get_link_info_cmd_fixed_param);
	wmi_mtrace(WMI_MLO_VDEV_GET_LINK_INFO_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, buf_len,
				   WMI_MLO_VDEV_GET_LINK_INFO_CMDID);
	if (ret) {
		wmi_err("Failed to send ml link state command to FW: %d vdev id %d",
			ret, cmd->vdev_id);
		wmi_buf_free(buf);
	}
	return ret;
}

static QDF_STATUS
extract_mlo_link_state_event_tlv(struct wmi_unified *wmi_handle,
				 void *buf,
				 struct  ml_link_state_info_event *params)
{
	WMI_MLO_VDEV_LINK_INFO_EVENTID_param_tlvs *param_buf;
	wmi_mlo_vdev_link_info_event_fixed_param *ev;
	wmi_mlo_vdev_link_info  *link_info = NULL;
	int num_info = 0;
	uint8_t *mld_addr;
	uint32_t num_link_info = 0;

	param_buf = (WMI_MLO_VDEV_LINK_INFO_EVENTID_param_tlvs *)buf;

	if (!param_buf) {
		wmi_err_rl("Param_buf is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ev = (wmi_mlo_vdev_link_info_event_fixed_param *)
	param_buf->fixed_param;
	link_info = (wmi_mlo_vdev_link_info *)param_buf->mlo_vdev_link_info;

	num_link_info = param_buf->num_mlo_vdev_link_info;
	params->status = ev->status;
	params->vdev_id = ev->vdev_id;
	params->hw_mode_index = ev->hw_mode_index;
	params->num_mlo_vdev_link_info = num_link_info;
	mld_addr = params->mldaddr.bytes;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&ev->mld_macaddr, mld_addr);

	if (params->num_mlo_vdev_link_info > WLAN_MLO_MAX_VDEVS) {
		wmi_err_rl("Invalid number of vdev link info");
		return QDF_STATUS_E_FAILURE;
	}

	for (num_info = 0; num_info < num_link_info; num_info++) {
		params->link_info[num_info].vdev_id =
		WMI_MLO_VDEV_LINK_INFO_GET_VDEVID(link_info->link_info);

		params->link_info[num_info].link_id =
		WMI_MLO_VDEV_LINK_INFO_GET_LINKID(link_info->link_info);

		params->link_info[num_info].link_status =
		WMI_MLO_VDEV_LINK_INFO_GET_LINK_STATUS(link_info->link_info);

		params->link_info[num_info].chan_freq =
		link_info->chan_freq;

		link_info++;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS send_mlo_peer_tid_to_link_map_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct wmi_host_tid_to_link_map_params *params)
{
	wmi_peer_tid_to_link_map_fixed_param *cmd;
	wmi_tid_to_link_map *t2lm;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	uint32_t buf_len = 0;
	uint8_t dir = 0;
	uint8_t tid_num = 0;

	buf_len = sizeof(wmi_peer_tid_to_link_map_fixed_param) +
		WMI_TLV_HDR_SIZE + (params->num_dir * T2LM_MAX_NUM_TIDS *
		 sizeof(wmi_tid_to_link_map));

	buf = wmi_buf_alloc(wmi_handle, buf_len);
	if (!buf) {
		wmi_err("wmi buf alloc failed for mlo_peer_mac: "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(params->peer_macaddr));
		return QDF_STATUS_E_NOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_peer_tid_to_link_map_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_tid_to_link_map_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			   wmi_peer_tid_to_link_map_fixed_param));

	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
			wmi_handle, params->pdev_id);

	WMI_CHAR_ARRAY_TO_MAC_ADDR(params->peer_macaddr, &cmd->link_macaddr);

	buf_ptr += sizeof(wmi_peer_tid_to_link_map_fixed_param);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       (params->num_dir * T2LM_MAX_NUM_TIDS *
		       sizeof(wmi_tid_to_link_map)));
	buf_ptr += sizeof(uint32_t);

	for (dir = 0; dir < params->num_dir; dir++) {
		wmi_debug("Add T2LM TLV for peer: " QDF_MAC_ADDR_FMT " direction:%d",
				QDF_MAC_ADDR_REF(params->peer_macaddr),
				params->t2lm_info[dir].direction);

		for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
			t2lm = (wmi_tid_to_link_map *)buf_ptr;

			WMITLV_SET_HDR(&t2lm->tlv_header,
				       WMITLV_TAG_STRUC_wmi_tid_to_link_map,
				       WMITLV_GET_STRUCT_TLVLEN(
					   wmi_tid_to_link_map));

			/* Populate TID number */
			WMI_TID_TO_LINK_MAP_TID_NUM_SET(
					t2lm->tid_to_link_map_info, tid_num);

			/* Populate the direction */
			WMI_TID_TO_LINK_MAP_DIR_SET(
					t2lm->tid_to_link_map_info,
					params->t2lm_info[dir].direction);

			/* Populate the default link mapping value */
			WMI_TID_TO_LINK_MAP_DEFAULT_MAPPING_SET(
					t2lm->tid_to_link_map_info,
					params->t2lm_info[dir].default_link_mapping);

			/* Populate the T2LM provisioned links for the
			 * corresponding TID number.
			 */
			WMI_TID_TO_LINK_MAP_LINK_MASK_SET(
					t2lm->tid_to_link_map_info,
					params->t2lm_info[dir].t2lm_provisioned_links[tid_num]);

			buf_ptr += sizeof(wmi_tid_to_link_map);

			wmi_debug("Add T2LM TLV: tid_to_link_map_info:%x",
				  t2lm->tid_to_link_map_info);
		}
	}

	wmi_mtrace(WMI_MLO_PEER_TID_TO_LINK_MAP_CMDID, cmd->pdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, buf_len,
				   WMI_MLO_PEER_TID_TO_LINK_MAP_CMDID);
	if (ret) {
		wmi_err("Failed to send T2LM command to FW: %d mlo_peer_mac: " QDF_MAC_ADDR_FMT,
				ret, QDF_MAC_ADDR_REF(params->peer_macaddr));
		wmi_buf_free(buf);
	}

	return ret;
}

static void update_t2lm_ie_info_params(
		wmi_mlo_ap_vdev_tid_to_link_map_ie_info * info,
		struct wlan_t2lm_info *params)
{
	WMI_MLO_VDEV_TID_TO_LINK_MAP_CTRL_DIR_SET(
			info->tid_to_link_map_ctrl,
			params->direction);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_CTRL_DEF_LINK_SET(
			info->tid_to_link_map_ctrl,
			params->default_link_mapping);

	info->map_switch_time = params->mapping_switch_time;
	WMI_MLO_VDEV_TID_TO_LINK_MAP_CTRL_SWT_TIME_SET(
			info->tid_to_link_map_ctrl,
			params->mapping_switch_time_present);

	info->expected_duration = params->expected_duration;
	WMI_MLO_VDEV_TID_TO_LINK_MAP_CTRL_DUR_TIME_SET(
			info->tid_to_link_map_ctrl,
			params->expected_duration_present);

	wmi_debug("tid_to_link_map_ctrl:%x map_switch_time:%d expected_duration:%d",
		  info->tid_to_link_map_ctrl, info->map_switch_time,
		  info->expected_duration);

	/* Do not fill link mapping values when default mapping is set to 1 */
	if (params->default_link_mapping)
		return;

	WMI_MLO_VDEV_TID_TO_LINK_MAP_CTRL_LINK_MAP_PRE_SET(
			info->tid_to_link_map_ctrl, 0xff);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_0_SET(
			info->ieee_tid_0_1_link_map,
			params->ieee_link_map_tid[0]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_1_SET(
			info->ieee_tid_0_1_link_map,
			params->ieee_link_map_tid[1]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_2_SET(
			info->ieee_tid_2_3_link_map,
			params->ieee_link_map_tid[2]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_3_SET(
			info->ieee_tid_2_3_link_map,
			params->ieee_link_map_tid[3]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_4_SET(
			info->ieee_tid_4_5_link_map,
			params->ieee_link_map_tid[4]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_5_SET(
			info->ieee_tid_4_5_link_map,
			params->ieee_link_map_tid[5]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_6_SET(
			info->ieee_tid_6_7_link_map,
			params->ieee_link_map_tid[6]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_IEEE_LINK_ID_7_SET(
			info->ieee_tid_6_7_link_map,
			params->ieee_link_map_tid[7]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_0_SET(
			info->hw_tid_0_1_link_map,
			params->hw_link_map_tid[0]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_1_SET(
			info->hw_tid_0_1_link_map,
			params->hw_link_map_tid[1]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_2_SET(
			info->hw_tid_2_3_link_map,
			params->hw_link_map_tid[2]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_3_SET(
			info->hw_tid_2_3_link_map,
			params->hw_link_map_tid[3]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_4_SET(
			info->hw_tid_4_5_link_map,
			params->hw_link_map_tid[4]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_5_SET(
			info->hw_tid_4_5_link_map,
			params->hw_link_map_tid[5]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_6_SET(
			info->hw_tid_6_7_link_map,
			params->hw_link_map_tid[6]);

	WMI_MLO_VDEV_TID_TO_LINK_MAP_HW_LINK_ID_7_SET(
			info->hw_tid_6_7_link_map,
			params->hw_link_map_tid[7]);

	wmi_debug("tid_to_link_map_ctrl:%x", info->tid_to_link_map_ctrl);
	wmi_debug("ieee_link_map: tid_0_1:%x tid_2_3:%x tid_4_5:%x tid_6_7:%x",
		  info->ieee_tid_0_1_link_map, info->ieee_tid_2_3_link_map,
		  info->ieee_tid_4_5_link_map, info->ieee_tid_6_7_link_map);
	wmi_debug("hw_link_map: tid_0_1:%x tid_2_3:%x tid_4_5:%x tid_6_7:%x",
		  info->hw_tid_0_1_link_map, info->hw_tid_2_3_link_map,
		  info->hw_tid_4_5_link_map, info->hw_tid_6_7_link_map);
}

static QDF_STATUS send_mlo_vdev_tid_to_link_map_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct wmi_host_tid_to_link_map_ap_params *params)
{
	wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param *cmd;
	wmi_mlo_ap_vdev_tid_to_link_map_ie_info *info;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	uint32_t buf_len = 0;
	uint32_t num_info = 0;

	if (params->num_t2lm_info > WLAN_MAX_T2LM_IE) {
		wmi_err("Failed to send T2LM command to FW for vdev id %d as t2lm info %d is greater than max %d",
			params->vdev_id,
			params->num_t2lm_info,
			WLAN_MAX_T2LM_IE);
		return QDF_STATUS_E_INVAL;
	}

	buf_len = sizeof(wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param) +
		WMI_TLV_HDR_SIZE + (params->num_t2lm_info *
		 sizeof(wmi_mlo_ap_vdev_tid_to_link_map_ie_info));

	buf = wmi_buf_alloc(wmi_handle, buf_len);
	if (!buf) {
		wmi_err("wmi buf alloc failed for vdev id %d while t2lm map cmd send: ",
			params->vdev_id);
		return QDF_STATUS_E_NOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(
	       &cmd->tlv_header,
	       WMITLV_TAG_STRUC_wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param,
	       WMITLV_GET_STRUCT_TLVLEN(
	       wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param));

	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
			wmi_handle, params->pdev_id);
	cmd->vdev_id = params->vdev_id;
	cmd->disabled_link_bitmap = params->disabled_link_bitmap;
	wmi_debug("pdev_id:%d vdev_id:%d disabled_link_bitmap:%x num_t2lm_info:%d",
		  cmd->pdev_id, cmd->vdev_id, cmd->disabled_link_bitmap,
		  params->num_t2lm_info);

	buf_ptr += sizeof(wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       (params->num_t2lm_info *
			sizeof(wmi_mlo_ap_vdev_tid_to_link_map_ie_info)));
	buf_ptr += sizeof(uint32_t);

	for (num_info = 0; num_info < params->num_t2lm_info; num_info++) {
		info = (wmi_mlo_ap_vdev_tid_to_link_map_ie_info *)buf_ptr;

		WMITLV_SET_HDR(
		       &info->tlv_header,
		       WMITLV_TAG_STRUC_wmi_mlo_ap_vdev_tid_to_link_map_ie_info,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_mlo_ap_vdev_tid_to_link_map_ie_info));
		update_t2lm_ie_info_params(info, &params->info[num_info]);
		buf_ptr += sizeof(wmi_mlo_ap_vdev_tid_to_link_map_ie_info);
	}

	wmi_mtrace(WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, buf_len,
				   WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_CMDID);
	if (ret) {
		wmi_err("Failed to send T2LM command to FW: %d vdev id %d",
			ret, cmd->vdev_id);
		wmi_buf_free(buf);
	}

	return ret;
}

static QDF_STATUS
extract_mlo_vdev_tid_to_link_map_event_tlv(
		struct wmi_unified *wmi_handle,
		uint8_t *buf,
		struct mlo_vdev_host_tid_to_link_map_resp *params)
{
	WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_EVENTID_param_tlvs *param_buf;
	wmi_mlo_ap_vdev_tid_to_link_map_evt_fixed_param *ev;

	param_buf = (WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_EVENTID_param_tlvs *)buf;
	if (!param_buf) {
		wmi_err_rl("Param_buf is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ev = (wmi_mlo_ap_vdev_tid_to_link_map_evt_fixed_param *)
		param_buf->fixed_param;

	params->vdev_id = ev->vdev_id;
	params->status  = ev->status_type;
	params->mapping_switch_tsf = ev->mapping_switch_tsf;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
extract_mlo_vdev_bcast_tid_to_link_map_event_tlv(
				struct wmi_unified *wmi_handle,
				void *buf,
				struct mlo_bcast_t2lm_info *bcast_info)
{
	WMI_MGMT_RX_EVENTID_param_tlvs *param_tlvs;
	wmi_mlo_bcast_t2lm_info *info;
	int i;

	param_tlvs = (WMI_MGMT_RX_EVENTID_param_tlvs *)buf;
	if (!param_tlvs) {
		wmi_err(" MGMT RX param_tlvs is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (param_tlvs->num_mlo_bcast_t2lm_info > MAX_AP_MLDS_PER_LINK) {
		wmi_err("num_mlo_bcast_t2lm_info is greater than %d",
			MAX_AP_MLDS_PER_LINK);
		return QDF_STATUS_E_INVAL;
	}

	info = param_tlvs->mlo_bcast_t2lm_info;
	if (!info) {
		wmi_debug("mlo_bcast_t2lm_info is not applicable");
		return QDF_STATUS_SUCCESS;
	}

	bcast_info->num_vdevs = param_tlvs->num_mlo_bcast_t2lm_info;
	wmi_debug("num_vdevs:%d", bcast_info->num_vdevs);
	for (i = 0; i < param_tlvs->num_mlo_bcast_t2lm_info; i++) {
		bcast_info->vdev_id[i] =
			WMI_MLO_BROADCAST_TID_TO_LINK_MAP_INFO_VDEV_ID_GET(
					info->vdev_id_expec_dur);

		bcast_info->expected_duration[i] =
			WMI_MLO_BROADCAST_TID_TO_LINK_MAP_INFO_EXP_DUR_GET(
					info->vdev_id_expec_dur);
		wmi_debug("vdev_id:%d expected_duration:%d",
			  bcast_info->vdev_id[i],
			  bcast_info->expected_duration[i]);
	}

	return QDF_STATUS_SUCCESS;
}
#else
size_t peer_assoc_t2lm_params_size(struct peer_assoc_params *req)
{
	return WMI_TLV_HDR_SIZE;
}

uint8_t *peer_assoc_add_tid_to_link_map(uint8_t *buf_ptr,
					       struct peer_assoc_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}
#endif /* WLAN_FEATURE_11BE */

#ifdef WLAN_MLO_MULTI_CHIP
QDF_STATUS mlo_setup_cmd_send_tlv(struct wmi_unified *wmi_handle,
				  struct wmi_mlo_setup_params *param)
{
	QDF_STATUS ret;
	wmi_mlo_setup_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	uint8_t *buf_ptr;
	uint32_t *partner_links;
	uint8_t idx;

	if (param->num_valid_hw_links > MAX_LINK_IN_MLO)
		return QDF_STATUS_E_INVAL;

	len = sizeof(*cmd) +
		(param->num_valid_hw_links * sizeof(uint32_t)) +
		WMI_TLV_HDR_SIZE;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_mlo_setup_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_mlo_setup_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_mlo_setup_cmd_fixed_param));

	cmd->mld_group_id = param->mld_grp_id;
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);
	buf_ptr = (uint8_t *)cmd + sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (sizeof(uint32_t) * param->num_valid_hw_links));
	partner_links = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (idx = 0; idx < param->num_valid_hw_links; idx++)
		partner_links[idx] = param->partner_links[idx];

	wmi_mtrace(WMI_MLO_SETUP_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len, WMI_MLO_SETUP_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send MLO setup command ret = %d", ret);
		wmi_buf_free(buf);
	}

	return ret;
}

QDF_STATUS mlo_ready_cmd_send_tlv(struct wmi_unified *wmi_handle,
				  struct wmi_mlo_ready_params *param)
{
	QDF_STATUS ret;
	wmi_mlo_ready_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;

	len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_mlo_ready_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_mlo_ready_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_mlo_ready_cmd_fixed_param));

	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);

	wmi_mtrace(WMI_MLO_READY_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len, WMI_MLO_READY_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send MLO ready command ret = %d", ret);
		wmi_buf_free(buf);
	}

	return ret;
}

QDF_STATUS mlo_teardown_cmd_send_tlv(struct wmi_unified *wmi_handle,
				     struct wmi_mlo_teardown_params *param)
{
	QDF_STATUS ret;
	wmi_mlo_teardown_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;

	len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_mlo_teardown_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_mlo_teardown_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_mlo_teardown_fixed_param));

	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);
	switch (param->reason) {
	case WMI_MLO_TEARDOWN_REASON_SSR:
		cmd->reason_code = WMI_MLO_TEARDOWN_SSR_REASON;
		break;
	case WMI_MLO_TEARDOWN_REASON_DOWN:
	default:
		cmd->reason_code = WMI_MLO_TEARDOWN_SSR_REASON + 1;
		break;
	}

	wmi_mtrace(WMI_MLO_TEARDOWN_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_MLO_TEARDOWN_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send MLO Teardown command ret = %d", ret);
		wmi_buf_free(buf);
	}

	return ret;
}

QDF_STATUS
extract_mlo_setup_cmpl_event_tlv(struct wmi_unified *wmi_handle,
				 uint8_t *buf,
				 struct wmi_mlo_setup_complete_params *params)
{
	WMI_MLO_SETUP_COMPLETE_EVENTID_param_tlvs *param_buf;
	wmi_mlo_setup_complete_event_fixed_param *ev;

	param_buf = (WMI_MLO_SETUP_COMPLETE_EVENTID_param_tlvs *)buf;
	if (!param_buf) {
		wmi_err_rl("Param_buf is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	ev = (wmi_mlo_setup_complete_event_fixed_param *)param_buf->fixed_param;

	params->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
								wmi_handle,
								ev->pdev_id);
	if (!ev->status)
		params->status = WMI_MLO_SETUP_STATUS_SUCCESS;
	else
		params->status = WMI_MLO_SETUP_STATUS_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
extract_mlo_teardown_cmpl_event_tlv(struct wmi_unified *wmi_handle,
				    uint8_t *buf,
				    struct wmi_mlo_teardown_cmpl_params *params)
{
	WMI_MLO_TEARDOWN_COMPLETE_EVENTID_param_tlvs *param_buf;
	wmi_mlo_teardown_complete_fixed_param *ev;

	param_buf = (WMI_MLO_TEARDOWN_COMPLETE_EVENTID_param_tlvs *)buf;
	if (!param_buf) {
		wmi_err_rl("Param_buf is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	ev = (wmi_mlo_teardown_complete_fixed_param *)param_buf->fixed_param;

	params->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
								wmi_handle,
								ev->pdev_id);
	if (!ev->status)
		params->status = WMI_MLO_TEARDOWN_STATUS_SUCCESS;
	else
		params->status = WMI_MLO_TEARDOWN_STATUS_FAILURE;

	return QDF_STATUS_SUCCESS;
}

static void wmi_11be_attach_mlo_setup_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->mlo_setup_cmd_send = mlo_setup_cmd_send_tlv;
	ops->mlo_teardown_cmd_send = mlo_teardown_cmd_send_tlv;
	ops->mlo_ready_cmd_send = mlo_ready_cmd_send_tlv;
	ops->extract_mlo_setup_cmpl_event = extract_mlo_setup_cmpl_event_tlv;
	ops->extract_mlo_teardown_cmpl_event =
					extract_mlo_teardown_cmpl_event_tlv;
}

#else /*WLAN_MLO_MULTI_CHIP*/

static void wmi_11be_attach_mlo_setup_tlv(wmi_unified_t wmi_handle)
{}

#endif /*WLAN_MLO_MULTI_CHIP*/

/**
 * extract_mgmt_rx_ml_cu_params_tlv() - extract MGMT Critical Update params
 * from MGMT_RX_EVENT_ID
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @cu_params: Pointer to MGMT Critical update parameters
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static
QDF_STATUS extract_mgmt_rx_ml_cu_params_tlv(wmi_unified_t wmi_handle,
					    void *evt_buf,
					    struct mlo_mgmt_ml_info *cu_params)
{
	WMI_MGMT_RX_EVENTID_param_tlvs *param_tlvs;
	wmi_mgmt_ml_info *cu_params_tlv;
	wmi_mgmt_rx_hdr *ev_hdr;
	uint32_t num_bpcc_bufp;

	param_tlvs = evt_buf;
	if (!param_tlvs) {
		wmi_err(" MGMT RX param_tlvs is NULL");
		return QDF_STATUS_E_INVAL;
	}

	ev_hdr = param_tlvs->hdr;
	if (!ev_hdr) {
		wmi_err("Rx event is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!cu_params) {
		wmi_debug("MGMT Rx CU params is NULL");
		return QDF_STATUS_E_INVAL;
	}

	cu_params_tlv = param_tlvs->ml_info;
	if (!cu_params_tlv) {
		wmi_debug("mgmt_ml_info TLV is not sent by FW");
		return QDF_STATUS_E_INVAL;
	}

	cu_params->cu_vdev_map[0] =
		cu_params_tlv->cu_vdev_map_1 & CU_VDEV_MAP_MASK;
	cu_params->cu_vdev_map[1] =
		(cu_params_tlv->cu_vdev_map_1 >> 16) & CU_VDEV_MAP_MASK;
	cu_params->cu_vdev_map[2] =
		cu_params_tlv->cu_vdev_map_2 & CU_VDEV_MAP_MASK;
	cu_params->cu_vdev_map[3] =
		(cu_params_tlv->cu_vdev_map_2 >> 16) & CU_VDEV_MAP_MASK;
	cu_params->cu_vdev_map[4] =
		cu_params_tlv->cu_vdev_map_3 & CU_VDEV_MAP_MASK;
	cu_params->cu_vdev_map[5] =
		(cu_params_tlv->cu_vdev_map_3 >> 16) & CU_VDEV_MAP_MASK;

	/* At present MAX_LINKS_SUPPORTED are 6.
	 * cu_vdev_map_4 which required for links
	 * 7 and 8 is unused.
	 */
	num_bpcc_bufp = param_tlvs->num_bpcc_bufp;
	if (param_tlvs->num_bpcc_bufp > sizeof(cu_params->vdev_bpcc)) {
		wmi_err("Invalid num_bpcc_bufp:%u", num_bpcc_bufp);
		return QDF_STATUS_E_INVAL;
	}
	qdf_mem_copy(cu_params->vdev_bpcc, param_tlvs->bpcc_bufp,
		     num_bpcc_bufp);

	qdf_trace_hex_dump(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
			   param_tlvs->bpcc_bufp, num_bpcc_bufp);

	return QDF_STATUS_SUCCESS;
}

void wmi_11be_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	wmi_11be_attach_mlo_setup_tlv(wmi_handle);
	ops->extract_mlo_link_set_active_resp =
		extract_mlo_link_set_active_resp_tlv;
	ops->send_mlo_link_set_active_cmd =
		send_mlo_link_set_active_cmd_tlv;
#ifdef WLAN_FEATURE_11BE
	ops->send_mlo_peer_tid_to_link_map =
		send_mlo_peer_tid_to_link_map_cmd_tlv;
	ops->send_mlo_vdev_tid_to_link_map =
		send_mlo_vdev_tid_to_link_map_cmd_tlv;
	ops->send_mlo_link_state_request =
		send_link_state_request_cmd_tlv;
	ops->extract_mlo_vdev_tid_to_link_map_event =
		extract_mlo_vdev_tid_to_link_map_event_tlv;
	ops->extract_mlo_vdev_bcast_tid_to_link_map_event =
		extract_mlo_vdev_bcast_tid_to_link_map_event_tlv;
	ops->extract_mlo_link_state_event =
		extract_mlo_link_state_event_tlv;
#endif /* WLAN_FEATURE_11BE */
	ops->extract_mgmt_rx_ml_cu_params =
		extract_mgmt_rx_ml_cu_params_tlv;
	ops->send_mlo_link_removal_cmd = send_mlo_link_removal_cmd_tlv;
	ops->extract_mlo_link_removal_evt_fixed_param =
			extract_mlo_link_removal_evt_fixed_param_tlv;
	ops->extract_mlo_link_removal_tbtt_update =
			extract_mlo_link_removal_tbtt_update_tlv;
	ops->extract_mgmt_rx_mlo_link_removal_info =
			extract_mgmt_rx_mlo_link_removal_info_tlv;
}
