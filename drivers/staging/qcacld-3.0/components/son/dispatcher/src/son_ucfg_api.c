/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <son_ucfg_api.h>
#include <wlan_mlme_main.h>
#include <init_deinit_lmac.h>

qdf_freq_t
ucfg_son_get_operation_chan_freq_vdev_id(struct wlan_objmgr_pdev *pdev,
					 uint8_t vdev_id)
{
	return wlan_get_operation_chan_freq_vdev_id(pdev, vdev_id);
}

void ucfg_son_get_min_and_max_power(struct wlan_objmgr_psoc *psoc,
				    int8_t *max_tx_power,
				    int8_t *min_tx_power)
{
	struct wlan_psoc_target_capability_info *target_cap =
					lmac_get_target_cap(psoc);

	*max_tx_power = 0;
	*min_tx_power = 0;

	if (target_cap) {
		*max_tx_power = target_cap->hw_max_tx_power;
		*min_tx_power = target_cap->hw_min_tx_power;
	}
}

bool ucfg_son_is_cac_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return QDF_IS_STATUS_SUCCESS(wlan_vdev_is_dfs_cac_wait(vdev));
}

uint32_t ucfg_son_get_sta_count(struct wlan_objmgr_vdev *vdev)
{
	uint32_t sta_count = wlan_vdev_get_peer_count(vdev);

	/* Don't include self peer in the count */
	if (sta_count > 0)
		--sta_count;

	return sta_count;
}

uint32_t ucfg_son_get_chan_flag(struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq, bool flag_160,
				struct ch_params *chan_params)
{
	return wlan_son_get_chan_flag(pdev, freq, flag_160,
				      chan_params);
}

QDF_STATUS ucfg_son_set_peer_kickout_allow(struct wlan_objmgr_vdev *vdev,
					   struct wlan_objmgr_peer *peer,
					   bool kickout_allow)
{
	return wlan_son_peer_set_kickout_allow(vdev, peer, kickout_allow);
}

QDF_STATUS ucfg_son_register_deliver_opmode_cb(struct wlan_objmgr_psoc *psoc,
					       mlme_deliver_cb cb)
{
	return wlan_son_register_mlme_deliver_cb(psoc, cb,
					SON_MLME_DELIVER_CB_TYPE_OPMODE);
}

QDF_STATUS ucfg_son_register_deliver_smps_cb(struct wlan_objmgr_psoc *psoc,
					     mlme_deliver_cb cb)
{
	return wlan_son_register_mlme_deliver_cb(psoc, cb,
					SON_MLME_DELIVER_CB_TYPE_SMPS);
}

int ucfg_son_cbs_init(void)
{
	return wlan_son_cbs_init();
}

int ucfg_son_cbs_deinit(void)
{
	return wlan_son_cbs_deinit();
}

int ucfg_son_set_cbs(struct wlan_objmgr_vdev *vdev,
		     bool enable)
{
	return wlan_son_set_cbs(vdev, enable);
}

int ucfg_son_set_cbs_wait_time(struct wlan_objmgr_vdev *vdev,
			       uint32_t val)
{
	return wlan_son_set_cbs_wait_time(vdev, val);
}

int ucfg_son_set_cbs_dwell_split_time(struct wlan_objmgr_vdev *vdev,
				      uint32_t val)
{
	return wlan_son_set_cbs_dwell_split_time(vdev, val);
}

int ucfg_son_disable_cbs(struct wlan_objmgr_vdev *vdev)
{
	return wlan_son_cbs_disable(vdev);
}

uint8_t ucfg_son_get_tx_power(struct element_info assoc_req_ies)
{
	return wlan_son_get_node_tx_power(assoc_req_ies);
}

QDF_STATUS ucfg_son_get_peer_rrm_info(struct element_info assoc_req_ies,
				      uint8_t *rrmcaps,
				      bool *is_beacon_meas_supported)
{
	return wlan_son_get_peer_rrm_info(assoc_req_ies, rrmcaps,
					  is_beacon_meas_supported);
}

QDF_STATUS
ucfg_son_vdev_get_supported_txrx_streams(struct wlan_objmgr_vdev *vdev,
					 uint32_t *num_tx_streams,
					 uint32_t *num_rx_streams)
{
	return wlan_son_vdev_get_supported_txrx_streams(vdev,
							num_tx_streams,
							num_rx_streams);
}

QDF_STATUS ucfg_son_get_vht_cap(struct wlan_objmgr_psoc *psoc,
				int32_t *vht_caps)
{
	struct wlan_psoc_target_capability_info *target_cap =
					lmac_get_target_cap(psoc);

	if (!target_cap)
		return QDF_STATUS_E_NULL_VALUE;
	*vht_caps = target_cap->vht_cap_info;
	return QDF_STATUS_SUCCESS;
}
