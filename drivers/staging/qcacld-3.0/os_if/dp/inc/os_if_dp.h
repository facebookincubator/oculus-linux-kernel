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
  * DOC: os_if_dp.h
  *
  *
  */
#ifndef __OSIF_DP_H__
#define __OSIF_DP_H__

#include "wlan_dp_public_struct.h"
#include <wlan_cfg80211.h>

#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
/**
 * enum qdisc_filter_status - QDISC filter status
 * @QDISC_FILTER_RTNL_LOCK_FAIL: rtnl lock acquire failed
 * @QDISC_FILTER_PRIO_MATCH: qdisc filter with priority match
 * @QDISC_FILTER_PRIO_MISMATCH: no filter match with configured priority
 */
enum qdisc_filter_status {
	QDISC_FILTER_RTNL_LOCK_FAIL,
	QDISC_FILTER_PRIO_MATCH,
	QDISC_FILTER_PRIO_MISMATCH,
};
#endif

/**
 * osif_dp_classify_pkt() - classify packet
 * @skb: sk buff
 *
 * Return: None
 */
void osif_dp_classify_pkt(struct sk_buff *skb);

/**
 * osif_dp_mark_pkt_type() - Mark pkt type in CB
 * @skb: sk buff
 *
 * Return: None
 */
void osif_dp_mark_pkt_type(struct sk_buff *skb);

/* wait time for nud stats in milliseconds */
#define WLAN_WAIT_TIME_NUD_STATS 800
/* nud stats skb max length */
#define WLAN_NUD_STATS_LEN 800
/* ARP packet type for NUD debug stats */
#define WLAN_NUD_STATS_ARP_PKT_TYPE 1

#define MAX_USER_COMMAND_SIZE 4096
#define DNS_DOMAIN_NAME_MAX_LEN 255
#define ICMPV6_ADDR_LEN 16

#define CONNECTIVITY_CHECK_SET_ARP \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ARP
#define CONNECTIVITY_CHECK_SET_DNS \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_DNS
#define CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE
#define CONNECTIVITY_CHECK_SET_ICMPV4 \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ICMPV4
#define CONNECTIVITY_CHECK_SET_ICMPV6 \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ICMPV6
#define CONNECTIVITY_CHECK_SET_TCP_SYN \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_SYN
#define CONNECTIVITY_CHECK_SET_TCP_SYN_ACK \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_SYN_ACK
#define CONNECTIVITY_CHECK_SET_TCP_ACK \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_ACK

/**
 * os_if_dp_register_txrx_callbacks() - Register TX/RX OSIF callbacks
 * @cb_obj: Call back object pointer for ops registration
 *
 * Return: None
 */
void os_if_dp_register_txrx_callbacks(struct wlan_dp_psoc_callbacks *cb_obj);

/**
 * os_if_dp_register_hdd_callbacks() - Register callback handlers
 * @psoc: Pointer to psoc context
 * @cb_obj: Callback object pointer
 *
 * Return: None
 */
void os_if_dp_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct wlan_dp_psoc_callbacks *cb_obj);

/**
 * osif_dp_nud_register_netevent_notifier() - Register netevent notifier
 * @psoc: Pointer to psoc context
 *
 * Return: 0 on success, error code on failure
 */
int osif_dp_nud_register_netevent_notifier(struct wlan_objmgr_psoc *psoc);

/**
 * osif_dp_nud_unregister_netevent_notifier() - Unregister netevent notifier
 * @psoc: Pointer to psoc context
 *
 * Return: None
 */
void osif_dp_nud_unregister_netevent_notifier(struct wlan_objmgr_psoc *psoc);

/**
 * osif_dp_get_nud_stats() - get arp stats command to firmware
 * @wiphy: pointer to wireless wiphy structure.
 * @vdev: pointer to vdev context.
 * @data: pointer to apfind configuration data.
 * @data_len: the length in byte of apfind data.
 *
 * This is called when wlan driver needs to get arp stats to
 * firmware.
 *
 * Return: An error code or 0 on success.
 */
int osif_dp_get_nud_stats(struct wiphy *wiphy, struct wlan_objmgr_vdev *vdev,
			  const void *data, int data_len);

/**
 * osif_dp_set_nud_stats() - set arp stats command to firmware
 * @wiphy: pointer to wireless wiphy structure.
 * @vdev: pointer to wireless_dev structure.
 * @data: pointer to apfind configuration data.
 * @data_len: the length in byte of apfind data.
 *
 * This is called when wlan driver needs to send arp stats to
 * firmware.
 *
 * Return: An error code or 0 on success.
 */
int osif_dp_set_nud_stats(struct wiphy *wiphy, struct wlan_objmgr_vdev *vdev,
			  const void *data, int data_len);
#endif /* __OSIF_DP_H__ */
