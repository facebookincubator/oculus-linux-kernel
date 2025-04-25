/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wlan_ll_sap_api.h"
#include <../../core/src/wlan_ll_lt_sap_bearer_switch.h>
#include <../../core/src/wlan_ll_lt_sap_main.h>
#include "wlan_cm_api.h"
#include "wlan_policy_mgr_ll_sap.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_reg_services_api.h"
#include "wlan_dfs_utils_api.h"
#include "wlan_mlme_main.h"

#ifdef WLAN_FEATURE_BEARER_SWITCH
wlan_bs_req_id
wlan_ll_lt_sap_bearer_switch_get_id(struct wlan_objmgr_psoc *psoc)
{
	return ll_lt_sap_bearer_switch_get_id(psoc);
}

QDF_STATUS
wlan_ll_lt_sap_switch_bearer_to_ble(struct wlan_objmgr_psoc *psoc,
				struct wlan_bearer_switch_request *bs_request)
{
	return ll_lt_sap_switch_bearer_to_ble(psoc, bs_request);
}

static void
connect_start_bearer_switch_requester_cb(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id,
					 wlan_bs_req_id request_id,
					 QDF_STATUS status, uint32_t req_value,
					 void *request_params)
{
	wlan_cm_id cm_id = req_value;

	wlan_cm_bearer_switch_resp(psoc, vdev_id, cm_id, status);
}

QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_start(struct wlan_objmgr_psoc *psoc,
					       qdf_list_t *scan_list,
					       uint8_t vdev_id,
					       wlan_cm_id cm_id)
{
	struct scan_cache_node *scan_node = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	uint32_t ch_freq = 0;
	struct scan_cache_entry *entry;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_bearer_switch_request bs_request = {0};
	qdf_freq_t ll_lt_sap_freq;
	bool is_bearer_switch_required = false;
	QDF_STATUS status = QDF_STATUS_E_ALREADY;
	uint8_t ll_lt_sap_vdev_id;

	ll_lt_sap_vdev_id = wlan_policy_mgr_get_ll_lt_sap_vdev_id(psoc);
	/* LL_LT SAP is not present, bearer switch is not required */
	if (ll_lt_sap_vdev_id == WLAN_INVALID_VDEV_ID)
		return status;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, ll_lt_sap_vdev_id,
						    WLAN_LL_SAP_ID);
	if (!vdev)
		return status;

	if (!scan_list || !qdf_list_size(scan_list))
		goto rel_ref;

	ll_lt_sap_freq = policy_mgr_get_ll_lt_sap_freq(psoc);
	qdf_list_peek_front(scan_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(scan_list, cur_node, &next_node);

		scan_node = qdf_container_of(cur_node, struct scan_cache_node,
					     node);
		entry = scan_node->entry;
		ch_freq = entry->channel.chan_freq;

		/*
		 * Switch the bearer in case of SCC/MCC for LL_LT SAP
		 */
		if (policy_mgr_2_freq_always_on_same_mac(psoc, ch_freq,
							 ll_lt_sap_freq)) {
			ll_sap_debug("Scan list has BSS of freq %d on same mac with ll_lt sap %d",
				     ch_freq, ll_lt_sap_freq);
			is_bearer_switch_required = true;
			break;
		}

		ch_freq = 0;
		cur_node = next_node;
		next_node = NULL;
	}

	if (!is_bearer_switch_required)
		goto rel_ref;

	bs_request.vdev_id = vdev_id;
	bs_request.request_id = ll_lt_sap_bearer_switch_get_id(psoc);
	bs_request.req_type = WLAN_BS_REQ_TO_NON_WLAN;
	bs_request.source = BEARER_SWITCH_REQ_CONNECT;
	bs_request.requester_cb = connect_start_bearer_switch_requester_cb;
	bs_request.arg_value = cm_id;

	status = ll_lt_sap_switch_bearer_to_ble(psoc, &bs_request);

	if (QDF_IS_STATUS_ERROR(status))
		status = QDF_STATUS_E_ALREADY;
rel_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LL_SAP_ID);

	return status;
}

static void connect_complete_bearer_switch_requester_cb(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id,
						wlan_bs_req_id request_id,
						QDF_STATUS status,
						uint32_t req_value,
						void *request_params)
{
	/* Drop this response as no action is required */
}

QDF_STATUS wlan_ll_sap_switch_bearer_on_sta_connect_complete(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id)
{
	struct wlan_bearer_switch_request bs_request = {0};
	QDF_STATUS status;

	bs_request.vdev_id = vdev_id;
	bs_request.request_id = ll_lt_sap_bearer_switch_get_id(psoc);
	bs_request.req_type = WLAN_BS_REQ_TO_WLAN;
	bs_request.source = BEARER_SWITCH_REQ_CONNECT;
	bs_request.requester_cb = connect_complete_bearer_switch_requester_cb;

	status = ll_lt_sap_switch_bearer_to_wlan(psoc, &bs_request);

	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_ALREADY;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlan_ll_lt_sap_get_freq_list(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_ll_lt_sap_freq_list *freq_list,
				uint8_t vdev_id)
{
	return ll_lt_sap_get_freq_list(psoc, freq_list, vdev_id);
}

qdf_freq_t wlan_ll_lt_sap_override_freq(struct wlan_objmgr_psoc *psoc,
					uint32_t vdev_id,
					qdf_freq_t chan_freq)
{
	qdf_freq_t freq;

	if (!policy_mgr_is_vdev_ll_lt_sap(psoc, vdev_id))
		return chan_freq;

	/*
	 * If already any concurrent interface is present on this frequency,
	 * select a different frequency to start ll_lt_sap
	 */
	if (!policy_mgr_get_connection_count_with_ch_freq(chan_freq))
		return chan_freq;

	freq = ll_lt_sap_get_valid_freq(psoc, vdev_id);

	ll_sap_debug("Vdev %d ll_lt_sap old freq %d new freq %d", vdev_id,
		     chan_freq, freq);

	return freq;
}

qdf_freq_t wlan_get_ll_lt_sap_restart_freq(struct wlan_objmgr_pdev *pdev,
					   qdf_freq_t chan_freq,
					   uint8_t vdev_id,
					   enum sap_csa_reason_code *csa_reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	qdf_freq_t restart_freq;

	if (wlan_reg_is_disable_in_secondary_list_for_freq(pdev, chan_freq) &&
	    !utils_dfs_is_freq_in_nol(pdev, chan_freq)) {
		*csa_reason = CSA_REASON_CHAN_DISABLED;
		goto get_new_ll_lt_sap_freq;
	} else if (wlan_reg_is_passive_for_freq(pdev, chan_freq))  {
		*csa_reason = CSA_REASON_CHAN_PASSIVE;
		goto get_new_ll_lt_sap_freq;
	} else if (!policy_mgr_is_sap_freq_allowed(psoc,
				wlan_get_opmode_from_vdev_id(pdev, vdev_id),
				chan_freq)) {
		*csa_reason = CSA_REASON_UNSAFE_CHANNEL;
		goto get_new_ll_lt_sap_freq;
	} else if (policy_mgr_is_ll_lt_sap_restart_required(psoc)) {
		*csa_reason = CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL;
		goto get_new_ll_lt_sap_freq;
	}

	return chan_freq;

get_new_ll_lt_sap_freq:
	restart_freq = ll_lt_sap_get_valid_freq(psoc, vdev_id);

	ll_sap_debug("vdev %d old freq %d restart freq %d CSA reason %d ",
		     vdev_id, chan_freq, restart_freq, *csa_reason);
	return restart_freq;
}

static void fw_bearer_switch_requester_cb(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id,
					  wlan_bs_req_id request_id,
					  QDF_STATUS status,
					  uint32_t req_value,
					  void *request_params)
{
	/*
	 * Drop this response as all the responses to the FW is always
	 * forwarded
	 */
}

QDF_STATUS
wlan_ll_sap_fw_bearer_switch_req(struct wlan_objmgr_psoc *psoc,
				 enum bearer_switch_req_type req_type)
{
	struct wlan_bearer_switch_request bs_request = {0};
	QDF_STATUS status;
	uint8_t ll_lt_sap_vdev_id;

	ll_lt_sap_vdev_id = wlan_policy_mgr_get_ll_lt_sap_vdev_id(psoc);

	bs_request.vdev_id = ll_lt_sap_vdev_id;
	bs_request.request_id = ll_lt_sap_bearer_switch_get_id(psoc);
	bs_request.req_type = req_type;
	bs_request.source = BEARER_SWITCH_REQ_FW;
	bs_request.requester_cb = fw_bearer_switch_requester_cb;

	if (req_type == WLAN_BS_REQ_TO_WLAN)
		status = ll_lt_sap_switch_bearer_to_wlan(psoc, &bs_request);

	else
		status = ll_lt_sap_switch_bearer_to_ble(psoc, &bs_request);

	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_ALREADY;

	return QDF_STATUS_SUCCESS;
}
