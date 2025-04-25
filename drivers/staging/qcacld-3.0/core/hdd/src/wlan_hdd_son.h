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
 * DOC: contains son hdd API implementation
 */

#ifndef WLAN_HDD_SON_H
#define WLAN_HDD_SON_H

#include <qdf_types.h>
#include <wlan_hdd_main.h>

/*
 * Determines type of event sent for MLME_EVENT_ASSOC_DISASSOC
 * @ALD_ASSOC_EVENT: event name to send assoc event
 * @ALD_DISASSOC_EVENT: event name to send disassoc event
 */
enum assoc_disassoc_event {
	ALD_ASSOC_EVENT,
	ALD_DISASSOC_EVENT,
};

#ifdef WLAN_FEATURE_SON

/**
 * hdd_son_register_callbacks() - register hdd callback for son
 * @hdd_ctx: hdd context
 *
 * Return: void
 */
void hdd_son_register_callbacks(struct hdd_context *hdd_ctx);

/**
 * hdd_son_deliver_acs_complete_event() - send acs complete event to son
 * @adapter: adapter object
 *
 * Return: 0 if event is sent successfully
 */
int hdd_son_deliver_acs_complete_event(struct hdd_adapter *adapter);

/**
 * hdd_son_deliver_cac_status_event() - send cac status to son
 * @adapter: adapter object
 * @freq: the operating frequency when radar is detected
 * @radar_detected: true if radar is detected else false
 *
 * Return: 0 if event is sent successfully
 */
int hdd_son_deliver_cac_status_event(struct hdd_adapter *adapter,
				     qdf_freq_t freq, bool radar_detected);

/**
 * hdd_son_deliver_assoc_disassoc_event() - send sta assoc disassoc event
 *						to son
 * @adapter: adapter object
 * @sta_mac: Mac address of the sta
 * @reason_code: reason code
 * @flag: determines the type of event sent(Assoc/disassoc)
 *
 * Return: 0 if event is sent successfully
 */
int hdd_son_deliver_assoc_disassoc_event(struct hdd_adapter *adapter,
					 struct qdf_mac_addr sta_mac,
					 uint32_t reason_code,
					 enum assoc_disassoc_event flag);
/**
 * hdd_son_deliver_peer_authorize_event() - Deliver peer auth event to SON
 * @link_info: Link info pointer in HDD adapter
 * @peer_mac: Peer mac address
 *
 * Return: Void
 */
void hdd_son_deliver_peer_authorize_event(struct wlan_hdd_link_info *link_info,
					  uint8_t *peer_mac);

/**
 * hdd_son_send_set_wifi_generic_command() - Send Generic SET command to SON
 * @wiphy: standard kernel wiphy
 * @wdev: wireless device
 * @tb: NL attributes
 *
 * Return: 0 on success
 */
int hdd_son_send_set_wifi_generic_command(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct nlattr **tb);

/**
 * hdd_son_send_get_wifi_generic_command() - Send Generic GET command to SON
 * @wiphy: standard kernel wiphy
 * @wdev: wireless device
 * @tb: NL attributes
 *
 * Return: 0 on success
 */
int hdd_son_send_get_wifi_generic_command(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct nlattr **tb);

/**
 * hdd_son_get_peer_max_mcs_idx() - Get peer max mcs index
 * @vdev: vdev object
 * @peer: peer obj
 *
 * Return: number of max mcs on success or 0 on failure
 */
uint32_t hdd_son_get_peer_max_mcs_idx(struct wlan_objmgr_vdev *vdev,
				      struct wlan_objmgr_peer *peer);

/**
 * hdd_son_deliver_chan_change_event() - send chan change to SON
 * @adapter: pointer to adapter
 * @freq: new operating channel frequency
 *
 * Return: 0 on success
 */
int hdd_son_deliver_chan_change_event(struct hdd_adapter *adapter,
				      qdf_freq_t freq);
#else

static inline void hdd_son_register_callbacks(struct hdd_context *hdd_ctx)
{
}

static inline int
	hdd_son_deliver_acs_complete_event(struct hdd_adapter *adapter)
{
	return 0;
}

static inline int
	hdd_son_deliver_cac_status_event(struct hdd_adapter *adapter,
					 qdf_freq_t freq, bool radar_detected)
{
	return 0;
}

static inline int
	hdd_son_deliver_assoc_disassoc_event(struct hdd_adapter *adapter,
					     struct qdf_mac_addr sta_mac,
					     uint32_t reason_code,
					     enum assoc_disassoc_event flag)
{
	return 0;
}

static inline void
hdd_son_deliver_peer_authorize_event(struct wlan_hdd_link_info *link_info,
				     uint8_t *peer_mac)
{
}

static inline
int hdd_son_send_set_wifi_generic_command(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct nlattr **tb)
{
	return -EINVAL;
}

static inline
int hdd_son_send_get_wifi_generic_command(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct nlattr **tb)
{
	return -EINVAL;
}

static inline
uint32_t hdd_son_get_peer_max_mcs_idx(struct wlan_objmgr_vdev *vdev,
				      struct wlan_objmgr_peer *peer)
{
	return 0;
}

static inline
int hdd_son_deliver_chan_change_event(struct hdd_adapter *adapter,
				      qdf_freq_t freq)
{
	return 0;
}
#endif /* WLAN_FEATURE_SON */
#endif
