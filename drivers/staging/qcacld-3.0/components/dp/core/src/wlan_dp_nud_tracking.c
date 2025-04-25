/*
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

/**
 * DOC: contains nud event tracking main function definitions
 */

#include "osif_sync.h"
#include "wlan_dp_main.h"
#include "wlan_dlm_ucfg_api.h"
#include "wlan_dp_cfg.h"
#include <cdp_txrx_misc.h>
#include "wlan_cm_roam_ucfg_api.h"
#include <wlan_cm_api.h>
#include "wlan_dp_nud_tracking.h"
#include "wlan_vdev_mgr_api.h"

#ifdef WLAN_NUD_TRACKING
/**
 * dp_txrx_get_tx_ack_count() - Get Tx Ack count
 * @dp_intf: Pointer to dp_intf
 *
 * Return: number of Tx ack count
 */
static uint32_t dp_txrx_get_tx_ack_count(struct wlan_dp_intf *dp_intf)
{
	struct cdp_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_dp_link *dp_link;
	struct wlan_dp_link *dp_link_next;
	uint32_t ack_count = 0;

	dp_for_each_link_held_safe(dp_intf, dp_link, dp_link_next) {
		ack_count += cdp_get_tx_ack_stats(soc, dp_link->link_id);
	}

	return ack_count;
}

void dp_nud_set_gateway_addr(struct wlan_objmgr_vdev *vdev,
			     struct qdf_mac_addr gw_mac_addr)
{
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;

	if (!dp_link) {
		dp_err("Unable to get DP link");
		return;
	}

	dp_intf = dp_link->dp_intf;
	qdf_mem_copy(dp_intf->nud_tracking.gw_mac_addr.bytes,
		     gw_mac_addr.bytes,
		     sizeof(struct qdf_mac_addr));
	dp_intf->nud_tracking.is_gw_updated = true;
}

void dp_nud_incr_gw_rx_pkt_cnt(struct wlan_dp_intf *dp_intf,
			       struct qdf_mac_addr *mac_addr)
{
	struct dp_nud_tracking_info *nud_tracking = &dp_intf->nud_tracking;

	if (!nud_tracking->is_gw_rx_pkt_track_enabled)
		return;

	if (!nud_tracking->is_gw_updated)
		return;

	if (qdf_is_macaddr_equal(&nud_tracking->gw_mac_addr,
				 mac_addr))
		qdf_atomic_inc(&nud_tracking->tx_rx_stats.gw_rx_packets);
}

void dp_nud_flush_work(struct wlan_dp_intf *dp_intf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	if (dp_intf->device_mode == QDF_STA_MODE &&
	    dp_ctx->dp_cfg.enable_nud_tracking) {
		dp_info("Flush the NUD work");
		qdf_disable_work(&dp_intf->nud_tracking.nud_event_work);
	}
}

void dp_nud_deinit_tracking(struct wlan_dp_intf *dp_intf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	if (dp_intf->device_mode == QDF_STA_MODE &&
	    dp_ctx->dp_cfg.enable_nud_tracking) {
		dp_info("DeInitialize the NUD tracking");
		qdf_destroy_work(NULL, &dp_intf->nud_tracking.nud_event_work);
	}
}

void dp_nud_ignore_tracking(struct wlan_dp_intf *dp_intf, bool ignoring)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	if (dp_intf->device_mode == QDF_STA_MODE &&
	    dp_ctx->dp_cfg.enable_nud_tracking)
		dp_intf->nud_tracking.ignore_nud_tracking = ignoring;
}

void dp_nud_reset_tracking(struct wlan_dp_intf *dp_intf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	if (dp_intf->device_mode == QDF_STA_MODE &&
	    dp_ctx->dp_cfg.enable_nud_tracking) {
		dp_info("Reset the NUD tracking");

		qdf_zero_macaddr(&dp_intf->nud_tracking.gw_mac_addr);
		dp_intf->nud_tracking.is_gw_updated = false;
		qdf_mem_zero(&dp_intf->nud_tracking.tx_rx_stats,
			     sizeof(struct dp_nud_tx_rx_stats));

		dp_intf->nud_tracking.curr_state = DP_NUD_NONE;
		qdf_atomic_set(&dp_intf
			       ->nud_tracking.tx_rx_stats.gw_rx_packets, 0);
	}
}

/**
 * dp_nud_stats_info() - display wlan NUD stats info
 * @dp_intf: Pointer to dp_intf
 *
 * Return: None
 */
static void dp_nud_stats_info(struct wlan_dp_intf *dp_intf)
{
	struct wlan_objmgr_vdev *vdev;
	struct dp_nud_tx_rx_stats *tx_rx_stats =
		&dp_intf->nud_tracking.tx_rx_stats;
	struct wlan_dp_psoc_callbacks *cb = &dp_intf->dp_ctx->dp_ops;
	uint32_t pause_map;

	vdev = dp_objmgr_get_vdev_by_user(dp_intf->def_link, WLAN_DP_ID);
	if (!vdev) {
		return;
	}

	dp_info("**** NUD STATS: ****");
	dp_info("NUD Probe Tx  : %d", tx_rx_stats->pre_tx_packets);
	dp_info("NUD Probe Ack : %d", tx_rx_stats->pre_tx_acked);
	dp_info("NUD Probe Rx  : %d", tx_rx_stats->pre_rx_packets);
	dp_info("NUD Failure Tx  : %d", tx_rx_stats->post_tx_packets);
	dp_info("NUD Failure Ack : %d", tx_rx_stats->post_tx_acked);
	dp_info("NUD Failure Rx  : %d", tx_rx_stats->post_rx_packets);
	dp_info("NUD Gateway Rx  : %d",
		qdf_atomic_read(&tx_rx_stats->gw_rx_packets));

	cb->os_if_dp_nud_stats_info(vdev);

	pause_map = cb->dp_get_pause_map(cb->callback_ctx,
					 dp_intf->dev);
	dp_info("Current pause_map value %x", pause_map);
	dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
}

/**
 * dp_nud_capture_stats() - capture wlan NUD stats
 * @dp_intf: Pointer to dp_intf
 * @nud_state: NUD state for which stats to capture
 *
 * Return: None
 */
static void dp_nud_capture_stats(struct wlan_dp_intf *dp_intf,
				 uint8_t nud_state)
{
	switch (nud_state) {
	case DP_NUD_INCOMPLETE:
	case DP_NUD_PROBE:
		dp_intf->nud_tracking.tx_rx_stats.pre_tx_packets =
				dp_intf->stats.tx_packets;
		dp_intf->nud_tracking.tx_rx_stats.pre_rx_packets =
				dp_intf->stats.rx_packets;
		dp_intf->nud_tracking.tx_rx_stats.pre_tx_acked =
				dp_txrx_get_tx_ack_count(dp_intf);
		break;
	case DP_NUD_FAILED:
		dp_intf->nud_tracking.tx_rx_stats.post_tx_packets =
				dp_intf->stats.tx_packets;
		dp_intf->nud_tracking.tx_rx_stats.post_rx_packets =
				dp_intf->stats.rx_packets;
		dp_intf->nud_tracking.tx_rx_stats.post_tx_acked =
				dp_txrx_get_tx_ack_count(dp_intf);
		break;
	default:
		break;
	}
}

/**
 * dp_nud_honour_failure() - check if nud failure to be honored
 * @dp_intf: Pointer to dp_intf
 *
 * Return: true if nud failure to be honored, else false.
 */
static bool dp_nud_honour_failure(struct wlan_dp_intf *dp_intf)
{
	uint32_t tx_transmitted, tx_acked, gw_rx_pkt, rx_received;
	struct dp_nud_tracking_info *nud_tracking = &dp_intf->nud_tracking;
	struct wlan_objmgr_vdev *vdev;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	bool ap_is_gateway;

	vdev = dp_objmgr_get_vdev_by_user(dp_intf->def_link, WLAN_DP_ID);
	if (!vdev)
		goto fail;
	wlan_vdev_mgr_get_param_bssid(vdev, bssid);
	dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);

	tx_transmitted = nud_tracking->tx_rx_stats.post_tx_packets -
		nud_tracking->tx_rx_stats.pre_tx_packets;
	tx_acked = nud_tracking->tx_rx_stats.post_tx_acked -
		nud_tracking->tx_rx_stats.pre_tx_acked;
	gw_rx_pkt = qdf_atomic_read(&nud_tracking->tx_rx_stats.gw_rx_packets);
	rx_received = nud_tracking->tx_rx_stats.post_rx_packets -
		nud_tracking->tx_rx_stats.pre_rx_packets;
	ap_is_gateway = qdf_is_macaddr_equal(&dp_intf->nud_tracking.gw_mac_addr,
					     (struct qdf_mac_addr *)bssid);

	if (!tx_transmitted || !tx_acked ||
	    !(gw_rx_pkt || (ap_is_gateway && rx_received))) {
		dp_info("NUD_FAILURE_HONORED [mac:" QDF_MAC_ADDR_FMT "]",
			QDF_MAC_ADDR_REF(nud_tracking->gw_mac_addr.bytes));
		dp_nud_stats_info(dp_intf);
		return true;
	}
fail:
	dp_info("NUD_FAILURE_NOT_HONORED [mac:" QDF_MAC_ADDR_FMT "]",
		QDF_MAC_ADDR_REF(nud_tracking->gw_mac_addr.bytes));

	dp_nud_stats_info(dp_intf);

	return false;
}

/**
 * dp_nud_set_tracking() - set the NUD tracking info
 * @dp_intf: Pointer to dp_intf
 * @nud_state: Current NUD state to set
 * @capture_enabled: GW Rx packet to be capture or not
 *
 * Return: None
 */
static void dp_nud_set_tracking(struct wlan_dp_intf *dp_intf,
				uint8_t nud_state,
				bool capture_enabled)
{
	dp_intf->nud_tracking.curr_state = nud_state;
	qdf_atomic_set(&dp_intf->nud_tracking.tx_rx_stats.gw_rx_packets, 0);
	dp_intf->nud_tracking.is_gw_rx_pkt_track_enabled = capture_enabled;
}

/**
 * dp_nud_failure_work() - work for nud event
 * @data: Pointer to dp_intf
 *
 * Return: None
 */
static void dp_nud_failure_work(void *data)
{
	struct wlan_dp_intf *dp_intf = data;
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	if (dp_intf->nud_tracking.curr_state != DP_NUD_FAILED) {
		dp_info("Not in NUD_FAILED state");
		return;
	}

	dp_ctx->dp_ops.dp_nud_failure_work(dp_ctx->dp_ops.callback_ctx,
					   dp_intf->dev);
}

void dp_nud_init_tracking(struct wlan_dp_intf *dp_intf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	if (dp_intf->device_mode == QDF_STA_MODE &&
	    dp_ctx->dp_cfg.enable_nud_tracking) {
		dp_info("Initialize the NUD tracking");

		qdf_zero_macaddr(&dp_intf->nud_tracking.gw_mac_addr);
		qdf_mem_zero(&dp_intf->nud_tracking.tx_rx_stats,
			     sizeof(struct dp_nud_tx_rx_stats));

		dp_intf->nud_tracking.curr_state = DP_NUD_NONE;
		dp_intf->nud_tracking.ignore_nud_tracking = false;
		dp_intf->nud_tracking.is_gw_updated = false;

		qdf_atomic_init(&dp_intf
				->nud_tracking.tx_rx_stats.gw_rx_packets);
		qdf_create_work(0, &dp_intf->nud_tracking.nud_event_work,
				dp_nud_failure_work, dp_intf);
	}
}

/**
 * dp_nud_process_failure_event() - processing NUD_FAILED event
 * @dp_intf: Pointer to dp_intf
 *
 * Return: None
 */
static void dp_nud_process_failure_event(struct wlan_dp_intf *dp_intf)
{
	uint8_t curr_state;

	curr_state = dp_intf->nud_tracking.curr_state;
	if (curr_state == DP_NUD_PROBE || curr_state == DP_NUD_INCOMPLETE) {
		dp_nud_capture_stats(dp_intf, DP_NUD_FAILED);
		if (dp_nud_honour_failure(dp_intf)) {
			dp_intf->nud_tracking.curr_state = DP_NUD_FAILED;
			qdf_sched_work(0, &dp_intf
					->nud_tracking.nud_event_work);
		} else {
			dp_info("NUD_START [0x%x]", DP_NUD_INCOMPLETE);
			dp_nud_capture_stats(dp_intf, DP_NUD_INCOMPLETE);
			dp_nud_set_tracking(dp_intf, DP_NUD_INCOMPLETE, true);
		}
	} else {
		dp_info("NUD FAILED -> Current State [0x%x]", curr_state);
	}
}

/**
 * dp_nud_filter_netevent() - filter netevents for STA interface
 * @netdev_addr: Pointer to neighbour
 * @gw_mac_addr: Gateway MAC address
 * @nud_state: Current NUD state
 *
 * Return: None
 */
static void dp_nud_filter_netevent(struct qdf_mac_addr *netdev_addr,
				   struct qdf_mac_addr *gw_mac_addr,
				   uint8_t nud_state)
{
	int status;
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_link *dp_link;
	struct wlan_dp_psoc_context *dp_ctx;
	struct wlan_objmgr_vdev *vdev;

	dp_enter();
	dp_ctx = dp_get_context();
	if (!dp_ctx) {
		dp_err("unable to get DP context");
		return;
	}

	dp_intf = dp_get_intf_by_macaddr(dp_ctx, netdev_addr);

	if (!dp_intf) {
		dp_err("Unable to get DP intf for MAC " QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(netdev_addr->bytes));
		return;
	}

	status = is_dp_intf_valid(dp_intf);
	if (status) {
		dp_err("invalid dp_intf");
		return;
	}

	if (dp_intf->device_mode != QDF_STA_MODE)
		return;

	if (dp_intf->nud_tracking.ignore_nud_tracking) {
		dp_info("NUD Tracking is Disabled");
		return;
	}

	if (!dp_intf->nud_tracking.is_gw_updated) {
		dp_info("GW is not updated");
		return;
	}

	/*
	 * NUD is used for STATION mode only, where all the MLO links
	 * are assumed to be connected. Hence use the deflink here to check
	 * if the interface is connected.
	 */
	dp_link = dp_intf->def_link;
	vdev = dp_objmgr_get_vdev_by_user(dp_link, WLAN_DP_ID);
	if (!vdev)
		return;

	if (!wlan_cm_is_vdev_active(vdev)) {
		dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		dp_info("Not in Connected State");
		return;
	}
	dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);

	if (!dp_link->conn_info.is_authenticated) {
		dp_info("client " QDF_MAC_ADDR_FMT
			" is in the middle of WPS/EAPOL exchange.",
			QDF_MAC_ADDR_REF(dp_intf->mac_addr.bytes));
		return;
	}

	if (!qdf_is_macaddr_equal(&dp_intf->nud_tracking.gw_mac_addr,
				  gw_mac_addr)) {
		dp_info("MAC mismatch NUD state %d GW MAC "
			 QDF_MAC_ADDR_FMT " Event MAC " QDF_MAC_ADDR_FMT,
			nud_state,
			QDF_MAC_ADDR_REF(dp_intf->nud_tracking.gw_mac_addr.bytes),
			QDF_MAC_ADDR_REF(gw_mac_addr->bytes));
		return;
	}

	if (dp_ctx->is_wiphy_suspended) {
		dp_info("wlan is suspended, ignore NUD event");
		return;
	}

	switch (nud_state) {
	case DP_NUD_PROBE:
	case DP_NUD_INCOMPLETE:
		dp_info("DP_NUD_START [0x%x]", nud_state);
		dp_nud_capture_stats(dp_intf, nud_state);
		dp_nud_set_tracking(dp_intf, nud_state, true);
		break;

	case DP_NUD_REACHABLE:
		dp_info("DP_NUD_REACHABLE [0x%x]", nud_state);
		dp_nud_set_tracking(dp_intf, DP_NUD_NONE, false);
		break;

	case DP_NUD_FAILED:
		dp_info("DP_NUD_FAILED [0x%x]", nud_state);
		/*
		 * This condition is to handle the scenario where NUD_FAILED
		 * events are received without any NUD_PROBE/INCOMPLETE event
		 * post roaming. Nud state is set to NONE as part of roaming.
		 * NUD_FAILED is not honored when the curr state is any state
		 * other than NUD_PROBE/INCOMPLETE so post roaming, nud state
		 * is moved to DP_NUD_PROBE to honor future NUD_FAILED events.
		 */
		if (dp_intf->nud_tracking.curr_state == DP_NUD_NONE) {
			dp_nud_capture_stats(dp_intf, DP_NUD_PROBE);
			dp_nud_set_tracking(dp_intf, DP_NUD_PROBE, true);
		} else {
			dp_nud_process_failure_event(dp_intf);
		}
		break;
	default:
		dp_info("NUD Event For Other State [0x%x]",
			nud_state);
		break;
	}
	dp_exit();
}

void dp_nud_netevent_cb(struct qdf_mac_addr *netdev_addr,
			struct qdf_mac_addr *gw_mac_addr, uint8_t nud_state)
{
	dp_enter();
	dp_nud_filter_netevent(netdev_addr, gw_mac_addr, nud_state);
	dp_exit();
}

void dp_nud_indicate_roam(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;

	if (!dp_link) {
		dp_err("Unable to get DP link");
		return;
	}

	dp_intf = dp_link->dp_intf;
	dp_nud_set_tracking(dp_intf, DP_NUD_NONE, false);
}
#endif
