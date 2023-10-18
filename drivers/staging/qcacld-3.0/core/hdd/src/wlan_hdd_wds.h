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
 * DOC: wlan_hdd_wds.h
 *
 * WLAN Host Device Driver file for wds (4 address format in mac header when
 * SA and TA are not same) support.
 *
 */

#if !defined(WLAN_HDD_WDS_H)
#define WLAN_HDD_WDS_H

struct wlan_objmgr_vdev;
struct hdd_adapter;

#ifdef FEATURE_WDS
/**
 * hdd_wds_config_dp_repeater_mode - Function to enable wds on the AP vdev
 * @vdev: object manager vdev context
 *
 * Set the wds_enabled flag for dp vdev. The wds source port learning
 * is triggered when this flag is enabled and AST entry for the remote
 * station(wds node) is added to the AST list.
 *
 * Return: None
 */
void hdd_wds_config_dp_repeater_mode(struct wlan_objmgr_vdev *vdev);

/**
 * hdd_wds_replace_peer_mac() - Replace the mac address for the wds next hop
 * @soc: SOC TXRX handle
 * @adapter: the pointer to adapter
 * @mac_addr: mac address of the peer or wds station
 *
 * The wds stations are reachable through a directly connected peer.
 * Replace the destination address with the mac address of the next
 * hop peer to reach the wds station, if the destination is not a
 * directly connected peer.
 *
 * Return: None
 */
void hdd_wds_replace_peer_mac(void *soc, struct hdd_adapter *adapter,
			      uint8_t *mac_addr);
#else
static inline
void hdd_wds_config_dp_repeater_mode(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void hdd_wds_replace_peer_mac(void *soc, struct hdd_adapter *adapter,
			      uint8_t *mac_addr)
{
}
#endif /* FEATURE_WDS*/
#endif /* if !defined(WLAN_HDD_WDS_H)*/
