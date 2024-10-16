/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#ifndef _WMI_UNIFIED_11BE_TLV_H_
#define _WMI_UNIFIED_11BE_TLV_H_

#ifdef WLAN_FEATURE_11BE_MLO
/**
 *  vdev_create_mlo_params_size() - Get MLO params size in vdev create
 *  @param: pointer to vdev create request param
 *  Return: size of MLO params in vdev create
 */
size_t vdev_create_mlo_params_size(struct vdev_create_params *param);
/**
 *  vdev_create_add_mlo_params() - Add MLO params in vdev create cmd
 *  @buf_ptr: pointer to vdev create buffer.
 *  @param: pointer to vdev create request param
 *
 *  Return: pointer to new offset of vdev create buffer
 */
uint8_t *vdev_create_add_mlo_params(uint8_t *buf_ptr,
				    struct vdev_create_params *param);
/**
 *  vdev_start_mlo_params_size() - Get MLO params size in vdev start
 *  @req: Vdev start request params
 *
 *  Return: size of MLO params in vdev start
 */
size_t vdev_start_mlo_params_size(struct vdev_start_params *req);
/**
 *  vdev_start_add_mlo_params() - Add MLO params in vdev start cmd
 *  @buf_ptr: pointer to vdev start buffer.
 *  @req: pointer to vdev create request param
 *
 *  Return: pointer to new offset of cmd buffer
 */
uint8_t *vdev_start_add_mlo_params(uint8_t *buf_ptr,
				   struct vdev_start_params *req);
/**
 *  vdev_start_add_ml_partner_links() - Add MLO partner links in vdev start cmd
 *  @buf_ptr: pointer to vdev start cmd buffer.
 *  @req: pointer to vdev start request param
 *
 *  Return: pointer to new offset of cmd buffer
 */
uint8_t *vdev_start_add_ml_partner_links(uint8_t *buf_ptr,
					 struct vdev_start_params *req);
/**
 * bcn_tmpl_mlo_param_size() - Get ML param size in beacon template
 * @param: Pointer to beacon template param
 *
 * Return: size of ML params in beacon template
 */
size_t bcn_tmpl_mlo_param_size(struct beacon_tmpl_params *param);

/**
 * bcn_tmpl_add_ml_partner_links - Add MLO partner links in beacon template
 *                                 command
 * @buf_ptr: pointer to beacon cmd buffer.
 * @param: pointer to beacon template params
 *
 * Return: pointer to new offset of cmd buffer
 */
uint8_t *bcn_tmpl_add_ml_partner_links(uint8_t *buf_ptr,
				       struct beacon_tmpl_params *param);

/**
 * bcn_tmpl_ml_info_size() - Get ML info size in beacon template
 * @param: Pointer to beacon template param
 *
 * Return: size of ML info in beacon template
 */
size_t bcn_tmpl_ml_info_size(struct beacon_tmpl_params *param);

/**
 * bcn_tmpl_add_ml_info() - Add MLO info to update Critical Update info in
 *                                 beacon template command
 * @buf_ptr: pointer to beacon cmd buffer.
 * @param: pointer to beacon template params
 *
 * Return: pointer to new offset of cmd buffer
 */
uint8_t *bcn_tmpl_add_ml_info(uint8_t *buf_ptr,
			      struct beacon_tmpl_params *param);
/**
 * prb_resp_tmpl_ml_info_size() - Get ML info size in 20TU probe resp template
 * @param: Pointer to 20TU probe response template param
 *
 * Return: size of ML info in 20TU probe response template
 */
size_t prb_resp_tmpl_ml_info_size(struct wmi_probe_resp_params *param);

/**
 * prb_resp_tmpl_add_ml_info() - Add MLO info to update Critical Update info in
 *                             20TU probe response template command
 * @buf_ptr: pointer to 20TU probe response cmd buffer.
 * @param: pointer to 20TU probe response template params
 *
 * Return: pointer to new offset of cmd buffer
 */
uint8_t *prb_resp_tmpl_add_ml_info(uint8_t *buf_ptr,
				   struct wmi_probe_resp_params *param);
/**
 *  peer_create_add_mlo_params() - Add MLO params in peer create cmd
 *  @buf_ptr: pointer to peer create cmd buffer.
 *  @req: pointer to peer create request param
 *
 *  Return: pointer to new offset of cmd buffer
 */
uint8_t *peer_create_add_mlo_params(uint8_t *buf_ptr,
				    struct peer_create_params *req);
/**
 *  peer_create_mlo_params_size() - Get ML params size in peer create
 *  @req: pointer to peer create request param
 *
 *  Return: size of ML params in peer create cmd
 */
size_t peer_create_mlo_params_size(struct peer_create_params *req);
/**
 *  peer_assoc_mlo_params_size() - Get ML params size in peer assoc
 *  @req: pointer to peer assoc request param
 *
 *  Return: size of ML params in peer assoc cmd
 */
size_t peer_assoc_mlo_params_size(struct peer_assoc_params *req);
/**
 *  peer_assoc_add_mlo_params() - Add MLO params in peer assoc cmd
 *  @buf_ptr: pointer to peer assoc cmd buffer.
 *  @req: pointer to peer assoc request param
 *
 *  Return: pointer to new offset of cmd buffer
 */
uint8_t *peer_assoc_add_mlo_params(uint8_t *buf_ptr,
				   struct peer_assoc_params *req);
/**
 *  peer_assoc_add_ml_partner_links() - Add MLO partner links in peer assoc cmd
 *  @buf_ptr: pointer to peer assoc cmd buffer.
 *  @req: pointer to peer assoc request param
 *
 *  Return: pointer to new offset of cmd buffer
 */
uint8_t *peer_assoc_add_ml_partner_links(uint8_t *buf_ptr,
					 struct peer_assoc_params *req);
/**
 * peer_assoc_t2lm_params_size() - Get T2LM param size in peer assoc
 * @req: pointer to peer create request param
 *
 *  Return: size of ML params in peer create cmd
 */
size_t peer_assoc_t2lm_params_size(struct peer_assoc_params *req);
/**
 *  peer_assoc_add_tid_to_link_map() - Add TID-to-link mapping in peer assoc cmd
 *  @buf_ptr: pointer to peer assoc cmd buffer.
 *  @req: pointer to peer assoc request param
 *
 *  Return: pointer to new offset of cmd buffer
 */
uint8_t *peer_assoc_add_tid_to_link_map(uint8_t *buf_ptr,
					struct peer_assoc_params *req);

/**
 *  peer_delete_mlo_params_size() - Get MLO params size in pdev delete
 *  @req: peer delete request params
 *
 *  Return: size of MLO params in vdev start
 */
size_t peer_delete_mlo_params_size(struct peer_delete_cmd_params *req);

/**
 *  peer_delete_add_mlo_params() - Add MLO params in peer delete cmd
 *  @buf_ptr: pointer to peer delete cmd  buffer.
 *  @req: pointer to peer delete request param
 *
 *  Return: pointer to new offset of cmd buffer
 */
uint8_t *peer_delete_add_mlo_params(uint8_t *buf_ptr,
				    struct peer_delete_cmd_params *req);

/** wmi_11be_tlv_attach_tlv - Attach 11be relaated callbacks
 *  @wmi_handle: WMI handle
 */
void wmi_11be_attach_tlv(wmi_unified_t wmi_handle);

/**
 * extract_mgmt_rx_mlo_link_removal_tlv_count() - Extract the number of link
 * removal TLVs from MGMT Rx event
 * @num_link_removal_tlvs: Number of link removal TLVs
 * @hdr: MGMT Rx event parameters to be populated
 *
 * Return: None
 */
static inline void
extract_mgmt_rx_mlo_link_removal_tlv_count(
	int num_link_removal_tlvs,
	struct mgmt_rx_event_params *hdr)
{
	hdr->num_link_removal_info = num_link_removal_tlvs;
}
#else
static uint8_t *vdev_create_add_mlo_params(uint8_t *buf_ptr,
					   struct vdev_create_params *param)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t vdev_create_mlo_params_size(struct vdev_create_params *param)
{
	return WMI_TLV_HDR_SIZE;
}

static uint8_t *vdev_start_add_mlo_params(uint8_t *buf_ptr,
					  struct vdev_start_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t vdev_start_mlo_params_size(struct vdev_start_params *req)
{
	return WMI_TLV_HDR_SIZE;
}

static uint8_t *vdev_start_add_ml_partner_links(uint8_t *buf_ptr,
						struct vdev_start_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t bcn_tmpl_mlo_param_size(struct beacon_tmpl_params *param)
{
	return WMI_TLV_HDR_SIZE;
}

static uint8_t *bcn_tmpl_add_ml_partner_links(uint8_t *buf_ptr,
					      struct beacon_tmpl_params *param)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t bcn_tmpl_ml_info_size(struct beacon_tmpl_params *param)
{
	return WMI_TLV_HDR_SIZE;
}

static uint8_t *bcn_tmpl_add_ml_info(uint8_t *buf_ptr,
				     struct beacon_tmpl_params *param)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t prb_resp_tmpl_ml_info_size(struct wmi_probe_resp_params *param)
{
	return WMI_TLV_HDR_SIZE;
}

static uint8_t *prb_resp_tmpl_add_ml_info(uint8_t *buf_ptr,
					  struct wmi_probe_resp_params *param)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static uint8_t *peer_create_add_mlo_params(uint8_t *buf_ptr,
					  struct peer_create_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t peer_create_mlo_params_size(struct peer_create_params *req)
{
	return WMI_TLV_HDR_SIZE;
}

static size_t peer_assoc_mlo_params_size(struct peer_assoc_params *req)
{
	size_t peer_assoc_mlo_size =
			WMI_TLV_HDR_SIZE +
			WMI_TLV_HDR_SIZE;

	return peer_assoc_mlo_size;
}

static uint8_t *peer_assoc_add_mlo_params(uint8_t *buf_ptr,
					  struct peer_assoc_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static uint8_t *peer_assoc_add_ml_partner_links(uint8_t *buf_ptr,
						struct peer_assoc_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t peer_assoc_t2lm_params_size(struct peer_assoc_params *req)
{
	return WMI_TLV_HDR_SIZE;
}

static uint8_t *peer_assoc_add_tid_to_link_map(uint8_t *buf_ptr,
					       struct peer_assoc_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t peer_delete_mlo_params_size(struct peer_delete_cmd_params *req)
{
	return WMI_TLV_HDR_SIZE;
}

static uint8_t *peer_delete_add_mlo_params(uint8_t *buf_ptr,
					   struct peer_delete_cmd_params *req)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static void wmi_11be_attach_tlv(wmi_unified_t wmi_handle)
{ }

static inline void
extract_mgmt_rx_mlo_link_removal_tlv_count(
	int num_link_removal_tlvs,
	struct mgmt_rx_event_params *hdr)
{
}
#endif /*WLAN_FEATURE_11BE_MLO*/
#endif /*_WMI_UNIFIED_11BE_TLV_H_*/
