/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 * DOC: target_if_wifi_pos_rx_ops.c
 * This file defines the functions pertinent to wifi positioning component's
 * target if layer.
 */
#include "wifi_pos_utils_pub.h"
#include "wifi_pos_api.h"
#include "wifi_pos_pasn_api.h"

#include "wmi_unified_api.h"
#include "wlan_lmac_if_def.h"
#include "target_if_wifi_pos.h"
#include "target_if_wifi_pos_rx_ops.h"
#include "wifi_pos_utils_i.h"
#include "target_if.h"

static struct wlan_lmac_if_wifi_pos_rx_ops *
target_if_wifi_pos_get_rxops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	if (!psoc) {
		target_if_err("passed psoc is NULL");
		return NULL;
	}

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		target_if_err("rx_ops is NULL");
		return NULL;
	}

	return &rx_ops->wifi_pos_rx_ops;
}

int target_if_wifi_pos_oem_rsp_ev_handler(ol_scn_t scn,
					  uint8_t *data_buf,
					  uint32_t data_len)
{
	int ret;
	uint8_t ring_idx = 0;
	QDF_STATUS status;
	uint32_t cookie = 0;
	struct wmi_host_oem_indirect_data *indirect;
	struct oem_data_rsp oem_rsp = {0};
	struct wifi_pos_psoc_priv_obj *priv_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_wifi_pos_rx_ops *wifi_pos_rx_ops;
	struct wmi_oem_response_param oem_resp_param = {0};
	wmi_unified_t wmi_handle;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	wlan_objmgr_psoc_get_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	priv_obj = wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());
	if (!priv_obj) {
		target_if_err("priv_obj is null");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	wifi_pos_rx_ops = target_if_wifi_pos_get_rxops(psoc);
	if (!wifi_pos_rx_ops || !wifi_pos_rx_ops->oem_rsp_event_rx) {
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		target_if_err("lmac callbacks not registered");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	ret = wmi_extract_oem_response_param(wmi_handle,
					     data_buf,
					     &oem_resp_param);

	oem_rsp.rsp_len_1 = oem_resp_param.num_data1;
	oem_rsp.data_1    = oem_resp_param.data_1;

	if (oem_resp_param.num_data2) {
		oem_rsp.rsp_len_2 = oem_resp_param.num_data2;
		oem_rsp.data_2    = oem_resp_param.data_2;
	}

	indirect = &oem_resp_param.indirect_data;
	status = target_if_wifi_pos_get_indirect_data(priv_obj, indirect,
						      &oem_rsp, &cookie);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("get indirect data failed status: %d", status);
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		return QDF_STATUS_E_INVAL;
	}

	ret = wifi_pos_rx_ops->oem_rsp_event_rx(psoc, &oem_rsp);
	if (indirect)
		ring_idx = indirect->pdev_id - 1;
	status = target_if_wifi_pos_replenish_ring(priv_obj, ring_idx,
						   oem_rsp.vaddr, cookie);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("replenish failed status: %d", status);
		ret = QDF_STATUS_E_FAILURE;
	}

	wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);

	return ret;
}

int wifi_pos_oem_cap_ev_handler(ol_scn_t scn, uint8_t *buf, uint32_t len)
{
	/* TBD */
	return 0;
}

int wifi_pos_oem_meas_rpt_ev_handler(ol_scn_t scn, uint8_t *buf,
				     uint32_t len)
{
	/* TBD */
	return 0;
}

int wifi_pos_oem_err_rpt_ev_handler(ol_scn_t scn, uint8_t *buf,
				    uint32_t len)
{
	/* TBD */
	return 0;
}

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
static struct wlan_lmac_if_wifi_pos_rx_ops *
target_if_wifi_pos_get_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		wifi_pos_err("rx_ops is NULL");
		return NULL;
	}

	return &rx_ops->wifi_pos_rx_ops;
}

int target_if_wifi_pos_pasn_peer_create_ev_handler(ol_scn_t scn,
						   uint8_t *buf,
						   uint32_t len)
{
	wmi_unified_t wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_wifi_pos_rx_ops *rx_ops;
	struct wifi_pos_pasn_peer_data *data = NULL;
	QDF_STATUS status;

	data = qdf_mem_malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		qdf_mem_free(data);
		return -EINVAL;
	}

	wlan_objmgr_psoc_get_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);

	if (!wlan_psoc_nif_fw_ext2_cap_get(psoc,
					   WLAN_VDEV_DELETE_ALL_PEER_SUPPORT)) {
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		qdf_mem_free(data);
		target_if_debug("Firmware doesn't support Peer delete all");
		return -EPERM;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		qdf_mem_free(data);
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	status = wmi_extract_pasn_peer_create_req(wmi_handle, buf, data);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("Extract PASN peer create failed");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		qdf_mem_free(data);
		return -EINVAL;
	}

	rx_ops = target_if_wifi_pos_get_rx_ops(psoc);
	if (!rx_ops || !rx_ops->wifi_pos_ranging_peer_create_cb) {
		wifi_pos_err("%s is null",
			     !rx_ops ? "rx_ops" : "rx_ops_cb");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		qdf_mem_free(data);
		return -EINVAL;
	}

	rx_ops->wifi_pos_ranging_peer_create_cb(psoc, data->peer_info,
						data->vdev_id,
						data->num_peers);

	wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
	qdf_mem_free(data);

	return 0;
}

int target_if_wifi_pos_pasn_peer_delete_ev_handler(ol_scn_t scn,
						   uint8_t *buf,
						   uint32_t len)
{
	wmi_unified_t wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_wifi_pos_rx_ops *rx_ops;
	struct wifi_pos_pasn_peer_data *data = NULL;
	QDF_STATUS status;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	wlan_objmgr_psoc_get_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		target_if_err("wmi_handle is null");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	data = qdf_mem_malloc(sizeof(*data));
	if (!data) {
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		return QDF_STATUS_E_NOMEM;
	}

	status = wmi_extract_pasn_peer_delete_req(wmi_handle, buf, data);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("Extract PASN peer delete failed");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		qdf_mem_free(data);
		return QDF_STATUS_E_NULL_VALUE;
	}

	rx_ops = target_if_wifi_pos_get_rx_ops(psoc);
	if (!rx_ops || !rx_ops->wifi_pos_ranging_peer_delete_cb) {
		wifi_pos_err("%s is null",
			     !rx_ops ? "rx_ops" : "rx_ops_cb");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
		qdf_mem_free(data);
		return QDF_STATUS_E_NULL_VALUE;
	}

	rx_ops->wifi_pos_ranging_peer_delete_cb(psoc, data->peer_info,
						data->vdev_id,
						data->num_peers);

	wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_POS_TGT_IF_ID);
	qdf_mem_free(data);

	return 0;
}
#endif

