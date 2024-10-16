/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains nud event tracking function declarations
 */

#ifndef _WLAN_DP_NUD_TRACKING_H_
#define _WLAN_DP_NUD_TRACKING_H_

#ifdef WLAN_NUD_TRACKING

/**
 * struct dp_nud_tx_rx_stats - Capture tx and rx count during NUD tracking
 * @pre_tx_packets: Number of tx packets at NUD_PROBE event
 * @pre_tx_acked: Number of tx acked at NUD_PROBE event
 * @pre_rx_packets: Number of rx packets at NUD_PROBE event
 * @post_tx_packets: Number of tx packets at NUD_FAILED event
 * @post_tx_acked: Number of tx acked at NUD_FAILED event
 * @post_rx_packets: Number of rx packets at NUD_FAILED event
 * @gw_rx_packets: Number of rx packets from the registered gateway
 *                 during the period from NUD_PROBE to NUD_FAILED
 */
struct dp_nud_tx_rx_stats {
	uint32_t pre_tx_packets;
	uint32_t pre_tx_acked;
	uint32_t pre_rx_packets;
	uint32_t post_tx_packets;
	uint32_t post_tx_acked;
	uint32_t post_rx_packets;
	qdf_atomic_t gw_rx_packets;
};

 /**
  * struct dp_nud_tracking_info - structure to keep track for NUD information
  * @curr_state: current state of NUD machine
  * @ignore_nud_tracking: true if nud tracking is not required else false
  * @tx_rx_stats: Number of packets during NUD tracking
  * @gw_mac_addr: gateway mac address for which NUD events are tracked
  * @nud_event_work: work to be scheduled during NUD_FAILED
  * @is_gw_rx_pkt_track_enabled: true if rx pkt capturing is enabled for GW,
  *                              else false
  * @is_gw_updated: true if GW is updated for NUD Tracking
  */
struct dp_nud_tracking_info {
	uint8_t curr_state;
	bool ignore_nud_tracking;
	struct dp_nud_tx_rx_stats tx_rx_stats;
	struct qdf_mac_addr gw_mac_addr;
	qdf_work_t nud_event_work;
	bool is_gw_rx_pkt_track_enabled;
	bool is_gw_updated;
};

/**
 * dp_nud_set_gateway_addr() - set gateway mac address
 * @vdev: vdev handle
 * @gw_mac_addr: mac address to be set
 *
 * Return: none
 */
void dp_nud_set_gateway_addr(struct wlan_objmgr_vdev *vdev,
			     struct qdf_mac_addr gw_mac_addr);

/**
 * dp_nud_incr_gw_rx_pkt_cnt() - Increment rx count for gateway
 * @dp_intf: Pointer to DP interface
 * @mac_addr: Gateway mac address
 *
 * Return: None
 */
void dp_nud_incr_gw_rx_pkt_cnt(struct wlan_dp_intf *dp_intf,
			       struct qdf_mac_addr *mac_addr);

/**
 * dp_nud_init_tracking() - initialize NUD tracking
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
void dp_nud_init_tracking(struct wlan_dp_intf *dp_intf);

/**
 * dp_nud_reset_tracking() - reset NUD tracking
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
void dp_nud_reset_tracking(struct wlan_dp_intf *dp_intf);

/**
 * dp_nud_deinit_tracking() - deinitialize NUD tracking
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
void dp_nud_deinit_tracking(struct wlan_dp_intf *dp_intf);

/**
 * dp_nud_ignore_tracking() - set/reset nud trackig status
 * @dp_intf: Pointer to dp interface
 * @ignoring: Ignore status to set
 *
 * Return: None
 */
void dp_nud_ignore_tracking(struct wlan_dp_intf *dp_intf,
			    bool ignoring);

/**
 * dp_nud_flush_work() - flush pending nud work
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
void dp_nud_flush_work(struct wlan_dp_intf *dp_intf);

/**
 * dp_nud_indicate_roam() - reset NUD when roaming happens
 * @vdev: vdev handle
 *
 * Return: None
 */
void dp_nud_indicate_roam(struct wlan_objmgr_vdev *vdev);

/**
 * dp_nud_netevent_cb() - netevent callback
 * @netdev_addr: netdev_addr
 * @gw_mac_addr: Gateway MAC address
 * @nud_state : NUD State
 *
 * Return: None
 */
void dp_nud_netevent_cb(struct qdf_mac_addr *netdev_addr,
			struct qdf_mac_addr *gw_mac_addr, uint8_t nud_state);
#else
static inline void dp_nud_set_gateway_addr(struct wlan_objmgr_vdev *vdev,
					   struct qdf_mac_addr gw_mac_addr)
{
}

static inline void dp_nud_incr_gw_rx_pkt_cnt(struct wlan_dp_intf *dp_intf,
					     struct qdf_mac_addr *mac_addr)
{
}

static inline void dp_nud_init_tracking(struct wlan_dp_intf *dp_intf)
{
}

static inline void dp_nud_reset_tracking(struct wlan_dp_intf *dp_intf)
{
}

static inline void dp_nud_deinit_tracking(struct wlan_dp_intf *dp_intf)
{
}

static inline void dp_nud_ignore_tracking(struct wlan_dp_intf *dp_intf,
					  bool status)
{
}

static inline void
dp_nud_flush_work(struct wlan_dp_intf *dp_intf)
{
}

static inline void
dp_nud_indicate_roam(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void dp_nud_netevent_cb(struct qdf_mac_addr *netdev_addr,
			struct qdf_mac_addr *gw_mac_addr, uint8_t nud_state)
{
}
#endif /* WLAN_NUD_TRACKING */
#endif /* end  of _WLAN_NUD_TRACKING_H_ */
