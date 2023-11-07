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
  * DOC: wlan_dp_txrx.c
  * DP TX/RX path implementation
  *
  *
  */

#include <wlan_dp_priv.h>
#include <wlan_dp_main.h>
#include <wlan_dp_txrx.h>
#include <qdf_types.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_misc.h>
#include <cdp_txrx_flow_ctrl_v2.h>
#include "wlan_dp_rx_thread.h"
#if defined(WLAN_SUPPORT_RX_FISA)
#include "wlan_dp_fisa_rx.h"
#endif
#include "nan_public_structs.h"
#include "wlan_nan_api_i.h"
#include <wlan_cm_api.h>
#include <enet.h>
#include <cds_utils.h>
#include <wlan_dp_bus_bandwidth.h>
#include "wlan_tdls_api.h"
#include <qdf_trace.h>
#include <qdf_net_stats.h>

uint32_t wlan_dp_intf_get_pkt_type_bitmap_value(void *intf_ctx)
{
	struct wlan_dp_intf *dp_intf = (struct wlan_dp_intf *)intf_ctx;

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}

	return dp_intf->pkt_type_bitmap;
}

#if defined(WLAN_SUPPORT_RX_FISA)
void dp_rx_skip_fisa(struct cdp_soc_t *cdp_soc, uint32_t value)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;

	qdf_atomic_set(&soc->skip_fisa_param.skip_fisa, !value);
}
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void dp_get_tx_resource(struct wlan_dp_intf *dp_intf,
			struct qdf_mac_addr *mac_addr)
{
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_intf->dp_ctx->dp_ops;

	dp_ops->dp_get_tx_resource(dp_intf->intf_id,
				   mac_addr);
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * dp_event_eapol_log() - send event to wlan diag
 * @nbuf: Network buffer ptr
 * @dir: direction
 *
 * Return: None
 */
void dp_event_eapol_log(qdf_nbuf_t nbuf, enum qdf_proto_dir dir)
{
	int16_t eapol_key_info;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event, struct host_event_wlan_eapol);

	if (dir == QDF_TX && QDF_NBUF_CB_PACKET_TYPE_EAPOL !=
	    QDF_NBUF_CB_GET_PACKET_TYPE(nbuf))
		return;
	else if (!qdf_nbuf_is_ipv4_eapol_pkt(nbuf))
		return;

	eapol_key_info = (uint16_t)(*(uint16_t *)
				(nbuf->data + EAPOL_KEY_INFO_OFFSET));

	wlan_diag_event.event_sub_type =
		(dir == QDF_TX ?
		 WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED :
		 WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED);
	wlan_diag_event.eapol_packet_type = (uint8_t)(*(uint8_t *)
				(nbuf->data + EAPOL_PACKET_TYPE_OFFSET));
	wlan_diag_event.eapol_key_info = eapol_key_info;
	wlan_diag_event.eapol_rate = 0;
	qdf_mem_copy(wlan_diag_event.dest_addr,
		     (nbuf->data + QDF_NBUF_DEST_MAC_OFFSET),
		     sizeof(wlan_diag_event.dest_addr));
	qdf_mem_copy(wlan_diag_event.src_addr,
		     (nbuf->data + QDF_NBUF_SRC_MAC_OFFSET),
		     sizeof(wlan_diag_event.src_addr));

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_EAPOL);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

static int dp_intf_is_tx_allowed(qdf_nbuf_t nbuf,
				 uint8_t intf_id, void *soc,
				 uint8_t *peer_mac)
{
	enum ol_txrx_peer_state peer_state;

	peer_state = cdp_peer_state_get(soc, intf_id, peer_mac);
	if (qdf_likely(OL_TXRX_PEER_STATE_AUTH == peer_state))
		return true;
	if (OL_TXRX_PEER_STATE_CONN == peer_state &&
	    (qdf_ntohs(qdf_nbuf_get_protocol(nbuf)) == ETHERTYPE_PAE ||
	     IS_DP_ETHERTYPE_WAI(nbuf)))
		return true;

	dp_info("Invalid peer state for Tx: %d", peer_state);
	return false;
}

/**
 * dp_tx_rx_is_dns_domain_name_match() - function to check whether dns
 * domain name in the received nbuf matches with the tracking dns domain
 * name or not
 *
 * @nbuf: Network buffer pointer
 * @dp_intf: DP interface pointer
 *
 * Returns: true if matches else false
 */
static bool dp_tx_rx_is_dns_domain_name_match(qdf_nbuf_t nbuf,
					      struct wlan_dp_intf *dp_intf)
{
	uint8_t *domain_name;

	if (dp_intf->track_dns_domain_len == 0)
		return false;

	/* check OOB , is strncmp accessing data more than skb->len */
	if ((dp_intf->track_dns_domain_len +
	    QDF_NBUF_PKT_DNS_NAME_OVER_UDP_OFFSET) > qdf_nbuf_len(nbuf))
		return false;

	domain_name = qdf_nbuf_get_dns_domain_name(nbuf,
						dp_intf->track_dns_domain_len);
	if (qdf_str_ncmp(domain_name, dp_intf->dns_payload,
			 dp_intf->track_dns_domain_len) == 0)
		return true;
	else
		return false;
}

/**
 * dp_clear_tx_rx_connectivity_stats() - clear connectivity stats
 * @dp_intf: pointer to DP interface
 *
 * Return: None
 */
static void dp_clear_tx_rx_connectivity_stats(struct wlan_dp_intf *dp_intf)
{
	dp_debug("Clear txrx connectivity stats");
	qdf_mem_zero(&dp_intf->dp_stats.arp_stats,
		     sizeof(dp_intf->dp_stats.arp_stats));
	qdf_mem_zero(&dp_intf->dp_stats.dns_stats,
		     sizeof(dp_intf->dp_stats.dns_stats));
	qdf_mem_zero(&dp_intf->dp_stats.tcp_stats,
		     sizeof(dp_intf->dp_stats.tcp_stats));
	qdf_mem_zero(&dp_intf->dp_stats.icmpv4_stats,
		     sizeof(dp_intf->dp_stats.icmpv4_stats));
	dp_intf->pkt_type_bitmap = 0;
	dp_intf->track_arp_ip = 0;
	qdf_mem_zero(dp_intf->dns_payload, dp_intf->track_dns_domain_len);
	dp_intf->track_dns_domain_len = 0;
	dp_intf->track_src_port = 0;
	dp_intf->track_dest_port = 0;
	dp_intf->track_dest_ipv4 = 0;
}

void dp_reset_all_intfs_connectivity_stats(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_dp_intf *dp_intf = NULL;

	qdf_spin_lock_bh(&dp_ctx->intf_list_lock);
	for (dp_get_front_intf_no_lock(dp_ctx, &dp_intf); dp_intf;
	     dp_get_next_intf_no_lock(dp_ctx, dp_intf, &dp_intf)) {
		dp_clear_tx_rx_connectivity_stats(dp_intf);
	}
	qdf_spin_unlock_bh(&dp_ctx->intf_list_lock);
}

void
dp_tx_rx_collect_connectivity_stats_info(qdf_nbuf_t nbuf, void *context,
		enum connectivity_stats_pkt_status action, uint8_t *pkt_type)
{
	uint32_t pkt_type_bitmap;
	struct wlan_dp_intf *dp_intf =  (struct  wlan_dp_intf *)context;

	/* ARP tracking is done already. */
	pkt_type_bitmap = dp_intf->pkt_type_bitmap;

	pkt_type_bitmap &=  ~dp_intf->dp_ctx->arp_connectivity_map;

	if (!pkt_type_bitmap)
		return;

	switch (action) {
	case PKT_TYPE_REQ:
	case PKT_TYPE_TX_HOST_FW_SENT:
		if (qdf_nbuf_is_icmp_pkt(nbuf)) {
			if (qdf_nbuf_data_is_icmpv4_req(nbuf) &&
			    dp_intf->track_dest_ipv4 ==
			    qdf_nbuf_get_icmpv4_tgt_ip(nbuf)) {
				*pkt_type = DP_CONNECTIVITY_CHECK_SET_ICMPV4;
				if (action == PKT_TYPE_REQ) {
					++dp_intf->dp_stats.icmpv4_stats.
							tx_icmpv4_req_count;
					dp_info("ICMPv4 Req packet");
				} else
					/* host receives tx completion */
					++dp_intf->dp_stats.icmpv4_stats.
						tx_host_fw_sent;
			}
		} else if (qdf_nbuf_is_ipv4_tcp_pkt(nbuf)) {
			if (qdf_nbuf_data_is_tcp_syn(nbuf) &&
			    dp_intf->track_dest_port ==
			    qdf_nbuf_data_get_tcp_dst_port(nbuf)) {
				*pkt_type = DP_CONNECTIVITY_CHECK_SET_TCP_SYN;
				if (action == PKT_TYPE_REQ) {
					++dp_intf->dp_stats.tcp_stats.
							tx_tcp_syn_count;
					dp_info("TCP Syn packet");
				} else {
					/* host receives tx completion */
					++dp_intf->dp_stats.tcp_stats.
							tx_tcp_syn_host_fw_sent;
				}
			} else if ((dp_intf->dp_stats.tcp_stats.
				    is_tcp_syn_ack_rcv || dp_intf->dp_stats.
					tcp_stats.is_tcp_ack_sent) &&
				   qdf_nbuf_data_is_tcp_ack(nbuf) &&
				   (dp_intf->track_dest_port ==
				    qdf_nbuf_data_get_tcp_dst_port(nbuf))) {
				*pkt_type = DP_CONNECTIVITY_CHECK_SET_TCP_ACK;
				if (action == PKT_TYPE_REQ &&
					dp_intf->dp_stats.tcp_stats.
							is_tcp_syn_ack_rcv) {
					++dp_intf->dp_stats.tcp_stats.
							tx_tcp_ack_count;
					dp_intf->dp_stats.tcp_stats.
						is_tcp_syn_ack_rcv = false;
					dp_intf->dp_stats.tcp_stats.
						is_tcp_ack_sent = true;
					dp_info("TCP Ack packet");
				} else if (action == PKT_TYPE_TX_HOST_FW_SENT &&
					dp_intf->dp_stats.tcp_stats.
							is_tcp_ack_sent) {
					/* host receives tx completion */
					++dp_intf->dp_stats.tcp_stats.
							tx_tcp_ack_host_fw_sent;
					dp_intf->dp_stats.tcp_stats.
							is_tcp_ack_sent = false;
				}
			}
		} else if (qdf_nbuf_is_ipv4_udp_pkt(nbuf)) {
			if (qdf_nbuf_data_is_dns_query(nbuf) &&
			    dp_tx_rx_is_dns_domain_name_match(nbuf, dp_intf)) {
				*pkt_type = DP_CONNECTIVITY_CHECK_SET_DNS;
				if (action == PKT_TYPE_REQ) {
					++dp_intf->dp_stats.dns_stats.
							tx_dns_req_count;
					dp_info("DNS query packet");
				} else
					/* host receives tx completion */
					++dp_intf->dp_stats.dns_stats.
								tx_host_fw_sent;
			}
		}
		break;

	case PKT_TYPE_RSP:
		if (qdf_nbuf_is_icmp_pkt(nbuf)) {
			if (qdf_nbuf_data_is_icmpv4_rsp(nbuf) &&
			    (dp_intf->track_dest_ipv4 ==
					qdf_nbuf_get_icmpv4_src_ip(nbuf))) {
				++dp_intf->dp_stats.icmpv4_stats.
							rx_icmpv4_rsp_count;
				*pkt_type =
				DP_CONNECTIVITY_CHECK_SET_ICMPV4;
				dp_info("ICMPv4 Res packet");
			}
		} else if (qdf_nbuf_is_ipv4_tcp_pkt(nbuf)) {
			if (qdf_nbuf_data_is_tcp_syn_ack(nbuf) &&
			    (dp_intf->track_dest_port ==
					qdf_nbuf_data_get_tcp_src_port(nbuf))) {
				++dp_intf->dp_stats.tcp_stats.
							rx_tcp_syn_ack_count;
				dp_intf->dp_stats.tcp_stats.
					is_tcp_syn_ack_rcv = true;
				*pkt_type =
				DP_CONNECTIVITY_CHECK_SET_TCP_SYN_ACK;
				dp_info("TCP Syn ack packet");
			}
		} else if (qdf_nbuf_is_ipv4_udp_pkt(nbuf)) {
			if (qdf_nbuf_data_is_dns_response(nbuf) &&
			    dp_tx_rx_is_dns_domain_name_match(nbuf, dp_intf)) {
				++dp_intf->dp_stats.dns_stats.
							rx_dns_rsp_count;
				*pkt_type = DP_CONNECTIVITY_CHECK_SET_DNS;
				dp_info("DNS response packet");
			}
		}
		break;

	case PKT_TYPE_TX_DROPPED:
		switch (*pkt_type) {
		case DP_CONNECTIVITY_CHECK_SET_ICMPV4:
			++dp_intf->dp_stats.icmpv4_stats.tx_dropped;
			dp_info("ICMPv4 Req packet dropped");
			break;
		case DP_CONNECTIVITY_CHECK_SET_TCP_SYN:
			++dp_intf->dp_stats.tcp_stats.tx_tcp_syn_dropped;
			dp_info("TCP syn packet dropped");
			break;
		case DP_CONNECTIVITY_CHECK_SET_TCP_ACK:
			++dp_intf->dp_stats.tcp_stats.tx_tcp_ack_dropped;
			dp_info("TCP ack packet dropped");
			break;
		case DP_CONNECTIVITY_CHECK_SET_DNS:
			++dp_intf->dp_stats.dns_stats.tx_dropped;
			dp_info("DNS query packet dropped");
			break;
		default:
			break;
		}
		break;
	case PKT_TYPE_RX_DELIVERED:
		switch (*pkt_type) {
		case DP_CONNECTIVITY_CHECK_SET_ICMPV4:
			++dp_intf->dp_stats.icmpv4_stats.rx_delivered;
			break;
		case DP_CONNECTIVITY_CHECK_SET_TCP_SYN_ACK:
			++dp_intf->dp_stats.tcp_stats.rx_delivered;
			break;
		case DP_CONNECTIVITY_CHECK_SET_DNS:
			++dp_intf->dp_stats.dns_stats.rx_delivered;
			break;
		default:
			break;
		}
		break;
	case PKT_TYPE_RX_REFUSED:
		switch (*pkt_type) {
		case DP_CONNECTIVITY_CHECK_SET_ICMPV4:
			++dp_intf->dp_stats.icmpv4_stats.rx_refused;
			break;
		case DP_CONNECTIVITY_CHECK_SET_TCP_SYN_ACK:
			++dp_intf->dp_stats.tcp_stats.rx_refused;
			break;
		case DP_CONNECTIVITY_CHECK_SET_DNS:
			++dp_intf->dp_stats.dns_stats.rx_refused;
			break;
		default:
			break;
		}
		break;
	case PKT_TYPE_TX_ACK_CNT:
		switch (*pkt_type) {
		case DP_CONNECTIVITY_CHECK_SET_ICMPV4:
			++dp_intf->dp_stats.icmpv4_stats.tx_ack_cnt;
			break;
		case DP_CONNECTIVITY_CHECK_SET_TCP_SYN:
			++dp_intf->dp_stats.tcp_stats.tx_tcp_syn_ack_cnt;
			break;
		case DP_CONNECTIVITY_CHECK_SET_TCP_ACK:
			++dp_intf->dp_stats.tcp_stats.tx_tcp_ack_ack_cnt;
			break;
		case DP_CONNECTIVITY_CHECK_SET_DNS:
			++dp_intf->dp_stats.dns_stats.tx_ack_cnt;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

/**
 * dp_get_transmit_mac_addr() - Get the mac address to validate the xmit
 * @dp_intf: DP interface
 * @nbuf: The network buffer
 * @mac_addr_tx_allowed: The mac address to be filled
 *
 * Return: None
 */
static
void dp_get_transmit_mac_addr(struct wlan_dp_intf *dp_intf,
			      qdf_nbuf_t nbuf,
			      struct qdf_mac_addr *mac_addr_tx_allowed)
{
	bool is_mc_bc_addr = false;
	enum nan_datapath_state state;

	switch (dp_intf->device_mode) {
	case QDF_NDI_MODE:
		state = wlan_nan_get_ndi_state(dp_intf->vdev);
		if (state == NAN_DATA_NDI_CREATED_STATE ||
		    state == NAN_DATA_CONNECTED_STATE ||
		    state == NAN_DATA_CONNECTING_STATE ||
		    state == NAN_DATA_PEER_CREATE_STATE) {
			if (QDF_NBUF_CB_GET_IS_BCAST(nbuf) ||
			    QDF_NBUF_CB_GET_IS_MCAST(nbuf))
				is_mc_bc_addr = true;
			if (is_mc_bc_addr)
				qdf_copy_macaddr(mac_addr_tx_allowed,
						 &dp_intf->mac_addr);
			else
				qdf_copy_macaddr(mac_addr_tx_allowed,
				(struct qdf_mac_addr *)qdf_nbuf_data(nbuf));
		}
		break;
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
		if (wlan_cm_is_vdev_active(dp_intf->vdev))
			qdf_copy_macaddr(mac_addr_tx_allowed,
					 &dp_intf->conn_info.bssid);
		break;
	default:
		break;
	}
}

#ifdef HANDLE_BROADCAST_EAPOL_TX_FRAME
/**
 * dp_fix_broadcast_eapol() - Fix broadcast eapol
 * @dp_intf: pointer to dp interface
 * @nbuf: pointer to nbuf
 *
 * Override DA of broadcast eapol with bssid addr.
 *
 * Return: None
 */
static void dp_fix_broadcast_eapol(struct wlan_dp_intf *dp_intf,
				   qdf_nbuf_t nbuf)
{
	qdf_ether_header_t *eth_hdr = (qdf_ether_header_t *)qdf_nbuf_data(nbuf);
	unsigned char *ap_mac_addr =
		&dp_intf->conn_info.bssid.bytes[0];

	if (qdf_unlikely((QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) ==
			  QDF_NBUF_CB_PACKET_TYPE_EAPOL) &&
			 QDF_NBUF_CB_GET_IS_BCAST(nbuf))) {
		dp_debug("SA: "QDF_MAC_ADDR_FMT " override DA: "QDF_MAC_ADDR_FMT " with AP mac address "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(&eth_hdr->ether_shost[0]),
			  QDF_MAC_ADDR_REF(&eth_hdr->ether_dhost[0]),
			  QDF_MAC_ADDR_REF(ap_mac_addr));

		qdf_mem_copy(&eth_hdr->ether_dhost, ap_mac_addr,
			     QDF_MAC_ADDR_SIZE);
	}
}
#else
static void dp_fix_broadcast_eapol(struct wlan_dp_intf *dp_intf,
				   qdf_nbuf_t nbuf)
{
}
#endif /* HANDLE_BROADCAST_EAPOL_TX_FRAME */

#ifdef WLAN_DP_FEATURE_MARK_ICMP_REQ_TO_FW
/**
 * dp_mark_icmp_req_to_fw() - Mark the ICMP request at a certain time interval
 *			       to be sent to the FW.
 * @dp_ctx: Global dp context
 * @nbuf: packet to be transmitted
 *
 * This func sets the "to_fw" flag in the packet context block, if the
 * current packet is an ICMP request packet. This marking is done at a
 * specific time interval, unless the INI value indicates to disable/enable
 * this for all frames.
 *
 * Return: none
 */
static void dp_mark_icmp_req_to_fw(struct wlan_dp_psoc_context *dp_ctx,
				   qdf_nbuf_t nbuf)
{
	uint64_t curr_time, time_delta;
	int time_interval_ms = dp_ctx->dp_cfg.icmp_req_to_fw_mark_interval;
	static uint64_t prev_marked_icmp_time;

	if (!dp_ctx->dp_cfg.icmp_req_to_fw_mark_interval)
		return;

	if ((qdf_nbuf_get_icmp_subtype(nbuf) != QDF_PROTO_ICMP_REQ) &&
	    (qdf_nbuf_get_icmpv6_subtype(nbuf) != QDF_PROTO_ICMPV6_REQ))
		return;

	/* Mark all ICMP request to be sent to FW */
	if (time_interval_ms == WLAN_CFG_ICMP_REQ_TO_FW_MARK_ALL)
		QDF_NBUF_CB_TX_PACKET_TO_FW(nbuf) = 1;

	curr_time = qdf_get_log_timestamp();
	time_delta = curr_time - prev_marked_icmp_time;
	if (time_delta >= (time_interval_ms *
			   QDF_LOG_TIMESTAMP_CYCLES_PER_10_US * 100)) {
		QDF_NBUF_CB_TX_PACKET_TO_FW(nbuf) = 1;
		prev_marked_icmp_time = curr_time;
	}
}
#else
static void dp_mark_icmp_req_to_fw(struct wlan_dp_psoc_context *dp_ctx,
				   qdf_nbuf_t nbuf)
{
}
#endif

#ifdef CONFIG_DP_PKT_ADD_TIMESTAMP
void wlan_dp_pkt_add_timestamp(struct wlan_dp_intf *dp_intf,
			       enum qdf_pkt_timestamp_index index,
			       qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_callbacks *dp_ops;

	if (qdf_unlikely(qdf_is_dp_pkt_timestamp_enabled())) {
		uint64_t tsf_time;

		dp_ops = &dp_intf->dp_ctx->dp_ops;
		dp_ops->dp_get_tsf_time(dp_intf->intf_id,
					qdf_get_log_timestamp(),
					&tsf_time);
		qdf_add_dp_pkt_timestamp(nbuf, index, tsf_time);
	}
}
#endif

QDF_STATUS
dp_start_xmit(struct wlan_dp_intf *dp_intf, qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct dp_tx_rx_stats *stats;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	enum qdf_proto_subtype subtype = QDF_PROTO_INVALID;
	bool is_arp = false;
	bool is_eapol = false;
	bool is_dhcp = false;
	uint8_t pkt_type;
	struct qdf_mac_addr mac_addr_tx_allowed = QDF_MAC_ADDR_ZERO_INIT;
	int cpu = qdf_get_smp_processor_id();

	stats = &dp_intf->dp_stats.tx_rx_stats;
	++stats->per_cpu[cpu].tx_called;
	stats->cont_txtimeout_cnt = 0;

	if (qdf_unlikely(cds_is_driver_transitioning())) {
		dp_err_rl("driver is transitioning, drop pkt");
		goto drop_pkt;
	}

	if (qdf_unlikely(dp_ctx->is_suspend)) {
		dp_err_rl("Device is system suspended, drop pkt");
		goto drop_pkt;
	}

	QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(nbuf) = 1;

	pkt_type = QDF_NBUF_CB_GET_PACKET_TYPE(nbuf);

	if (pkt_type == QDF_NBUF_CB_PACKET_TYPE_ARP) {
		if (qdf_nbuf_data_is_arp_req(nbuf) &&
		    (dp_intf->track_arp_ip == qdf_nbuf_get_arp_tgt_ip(nbuf))) {
			is_arp = true;
			++dp_intf->dp_stats.arp_stats.tx_arp_req_count;
			dp_info("ARP packet");
		}
	} else if (pkt_type == QDF_NBUF_CB_PACKET_TYPE_EAPOL) {
		subtype = qdf_nbuf_get_eapol_subtype(nbuf);
		if (subtype == QDF_PROTO_EAPOL_M2) {
			++dp_intf->dp_stats.eapol_stats.eapol_m2_count;
			is_eapol = true;
		} else if (subtype == QDF_PROTO_EAPOL_M4) {
			++dp_intf->dp_stats.eapol_stats.eapol_m4_count;
			is_eapol = true;
		}
	} else if (pkt_type == QDF_NBUF_CB_PACKET_TYPE_DHCP) {
		subtype = qdf_nbuf_get_dhcp_subtype(nbuf);
		if (subtype == QDF_PROTO_DHCP_DISCOVER) {
			++dp_intf->dp_stats.dhcp_stats.dhcp_dis_count;
			is_dhcp = true;
		} else if (subtype == QDF_PROTO_DHCP_REQUEST) {
			++dp_intf->dp_stats.dhcp_stats.dhcp_req_count;
			is_dhcp = true;
		}
	} else if ((pkt_type == QDF_NBUF_CB_PACKET_TYPE_ICMP) ||
		   (pkt_type == QDF_NBUF_CB_PACKET_TYPE_ICMPv6)) {
		dp_mark_icmp_req_to_fw(dp_ctx, nbuf);
	}

	wlan_dp_pkt_add_timestamp(dp_intf, QDF_PKT_TX_DRIVER_ENTRY, nbuf);

	/* track connectivity stats */
	if (dp_intf->pkt_type_bitmap)
		dp_tx_rx_collect_connectivity_stats_info(nbuf, dp_intf,
							 PKT_TYPE_REQ,
							 &pkt_type);

	dp_get_transmit_mac_addr(dp_intf, nbuf, &mac_addr_tx_allowed);
	if (qdf_is_macaddr_zero(&mac_addr_tx_allowed)) {
		dp_info_rl("tx not allowed, transmit operation suspended");
		goto drop_pkt;
	}

	dp_get_tx_resource(dp_intf, &mac_addr_tx_allowed);

	if (!qdf_nbuf_ipa_owned_get(nbuf)) {
		nbuf = dp_nbuf_orphan(dp_intf, nbuf);
		if (!nbuf)
			goto drop_pkt_accounting;
	}

	/*
	 * Add SKB to internal tracking table before further processing
	 * in WLAN driver.
	 */
	qdf_net_buf_debug_acquire_skb(nbuf, __FILE__, __LINE__);

	qdf_net_stats_add_tx_bytes(&dp_intf->stats, qdf_nbuf_len(nbuf));

	if (qdf_nbuf_is_tso(nbuf)) {
		qdf_net_stats_add_tx_pkts(&dp_intf->stats,
					  qdf_nbuf_get_tso_num_seg(nbuf));
	} else {
		qdf_net_stats_add_tx_pkts(&dp_intf->stats, 1);
		dp_ctx->no_tx_offload_pkt_cnt++;
	}

	dp_event_eapol_log(nbuf, QDF_TX);
	QDF_NBUF_CB_TX_PACKET_TRACK(nbuf) = QDF_NBUF_TX_PKT_DATA_TRACK;
	QDF_NBUF_UPDATE_TX_PKT_COUNT(nbuf, QDF_NBUF_TX_PKT_DP);

	qdf_dp_trace_set_track(nbuf, QDF_TX);

	DPTRACE(qdf_dp_trace(nbuf, QDF_DP_TRACE_TX_PACKET_PTR_RECORD,
			     QDF_TRACE_DEFAULT_PDEV_ID,
			     qdf_nbuf_data_addr(nbuf),
			     sizeof(qdf_nbuf_data(nbuf)),
			     QDF_TX));

	if (!dp_intf_is_tx_allowed(nbuf, dp_intf->intf_id, soc,
				   mac_addr_tx_allowed.bytes)) {
		dp_info("Tx not allowed for sta:" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_addr_tx_allowed.bytes));
		goto drop_pkt_and_release_nbuf;
	}

	/* check whether need to linearize nbuf, like non-linear udp data */
	if (dp_nbuf_nontso_linearize(nbuf) != QDF_STATUS_SUCCESS) {
		dp_err(" nbuf %pK linearize failed. drop the pkt", nbuf);
		goto drop_pkt_and_release_nbuf;
	}

	/*
	 * If a transmit function is not registered, drop packet
	 */
	if (!dp_intf->tx_fn) {
		dp_err("TX function not registered by the data path");
		goto drop_pkt_and_release_nbuf;
	}

	dp_fix_broadcast_eapol(dp_intf, nbuf);

	if (dp_intf->tx_fn(soc, dp_intf->intf_id, nbuf)) {
		dp_debug_rl("Failed to send packet from adapter %u",
			    dp_intf->intf_id);
		goto drop_pkt_and_release_nbuf;
	}

	return QDF_STATUS_SUCCESS;

drop_pkt_and_release_nbuf:
	qdf_net_buf_debug_release_skb(nbuf);
drop_pkt:

	/* track connectivity stats */
	if (dp_intf->pkt_type_bitmap)
		dp_tx_rx_collect_connectivity_stats_info(nbuf, dp_intf,
							 PKT_TYPE_TX_DROPPED,
							 &pkt_type);
	qdf_dp_trace_data_pkt(nbuf, QDF_TRACE_DEFAULT_PDEV_ID,
			      QDF_DP_TRACE_DROP_PACKET_RECORD, 0,
			      QDF_TX);
	qdf_nbuf_kfree(nbuf);

drop_pkt_accounting:

	qdf_net_stats_inc_tx_dropped(&dp_intf->stats);
	++stats->per_cpu[cpu].tx_dropped;
	if (is_arp) {
		++dp_intf->dp_stats.arp_stats.tx_dropped;
		dp_info_rl("ARP packet dropped");
	} else if (is_eapol) {
		++dp_intf->dp_stats.eapol_stats.
				tx_dropped[subtype - QDF_PROTO_EAPOL_M1];
	} else if (is_dhcp) {
		++dp_intf->dp_stats.dhcp_stats.
				tx_dropped[subtype - QDF_PROTO_DHCP_DISCOVER];
	}

	return QDF_STATUS_E_FAILURE;
}

void dp_tx_timeout(struct wlan_dp_intf *dp_intf)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	u64 diff_time;

	cdp_dump_flow_pool_info(soc);

	++dp_intf->dp_stats.tx_rx_stats.tx_timeout_cnt;
	++dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt;

	diff_time = qdf_system_ticks() -
		dp_intf->dp_stats.tx_rx_stats.last_txtimeout;

	if ((dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt > 1) &&
	    (diff_time > (DP_TX_TIMEOUT * 2))) {
		/*
		 * In case when there is no traffic is running, it may
		 * possible tx time-out may once happen and later system
		 * recovered then continuous tx timeout count has to be
		 * reset as it is gets modified only when traffic is running.
		 * If over a period of time if this count reaches to threshold
		 * then host triggers a false subsystem restart. In genuine
		 * time out case OS will call the tx time-out back to back
		 * at interval of DP_TX_TIMEOUT. Here now check if previous
		 * TX TIME out has occurred more than twice of DP_TX_TIMEOUT
		 * back then host may recovered here from data stall.
		 */
		dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt = 0;
		dp_info("Reset continuous tx timeout stat");
	}

	dp_intf->dp_stats.tx_rx_stats.last_txtimeout = qdf_system_ticks();

	if (dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt >
	    DP_TX_STALL_THRESHOLD) {
		dp_err("Data stall due to continuous TX timeouts");
		dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt = 0;

		if (dp_is_data_stall_event_enabled(DP_HOST_STA_TX_TIMEOUT))
			cdp_post_data_stall_event(soc,
					  DATA_STALL_LOG_INDICATOR_HOST_DRIVER,
					  DATA_STALL_LOG_HOST_STA_TX_TIMEOUT,
					  OL_TXRX_PDEV_ID, 0xFF,
					  DATA_STALL_LOG_RECOVERY_TRIGGER_PDR);
	}
}

void dp_sta_notify_tx_comp_cb(qdf_nbuf_t nbuf, void *ctx, uint16_t flag)
{
	struct wlan_dp_intf *dp_intf = ctx;
	enum qdf_proto_subtype subtype;
	struct qdf_mac_addr *dest_mac_addr;
	QDF_STATUS status;

	if (is_dp_intf_valid(dp_intf))
		return;

	dest_mac_addr = (struct qdf_mac_addr *)qdf_nbuf_data(nbuf);

	switch (QDF_NBUF_CB_GET_PACKET_TYPE(nbuf)) {
	case QDF_NBUF_CB_PACKET_TYPE_ARP:
		if (flag & BIT(QDF_TX_RX_STATUS_DOWNLOAD_SUCC))
			++dp_intf->dp_stats.arp_stats.
				tx_host_fw_sent;
		if (flag & BIT(QDF_TX_RX_STATUS_OK))
			++dp_intf->dp_stats.arp_stats.tx_ack_cnt;
		break;
	case QDF_NBUF_CB_PACKET_TYPE_EAPOL:
		subtype = qdf_nbuf_get_eapol_subtype(nbuf);
		if (!(flag & BIT(QDF_TX_RX_STATUS_OK)) &&
		    subtype != QDF_PROTO_INVALID &&
		    subtype <= QDF_PROTO_EAPOL_M4)
			++dp_intf->dp_stats.eapol_stats.
				tx_noack_cnt[subtype - QDF_PROTO_EAPOL_M1];
		break;
	case QDF_NBUF_CB_PACKET_TYPE_DHCP:
		subtype = qdf_nbuf_get_dhcp_subtype(nbuf);
		if (!(flag & BIT(QDF_TX_RX_STATUS_OK)) &&
		    subtype != QDF_PROTO_INVALID &&
		    subtype <= QDF_PROTO_DHCP_ACK)
			++dp_intf->dp_stats.dhcp_stats.
				tx_noack_cnt[subtype - QDF_PROTO_DHCP_DISCOVER];
		break;
	default:
		break;
	}

	/* Since it is TDLS call took TDLS vdev ref*/
	status = wlan_objmgr_vdev_try_get_ref(dp_intf->vdev, WLAN_TDLS_SB_ID);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		wlan_tdls_update_tx_pkt_cnt(dp_intf->vdev, dest_mac_addr);
		wlan_objmgr_vdev_release_ref(dp_intf->vdev, WLAN_TDLS_SB_ID);
	}
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
QDF_STATUS dp_mon_rx_packet_cbk(void *context, qdf_nbuf_t rxbuf)
{
	struct wlan_dp_intf *dp_intf;
	QDF_STATUS status;
	qdf_nbuf_t nbuf;
	qdf_nbuf_t nbuf_next;
	unsigned int cpu_index;
	struct dp_tx_rx_stats *stats;

	/* Sanity check on inputs */
	if ((!context) || (!rxbuf)) {
		dp_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf = (struct wlan_dp_intf *)context;

	cpu_index = qdf_get_cpu();
	stats = &dp_intf->dp_stats.tx_rx_stats;

	/* walk the chain until all are processed */
	nbuf =  rxbuf;
	while (nbuf) {
		nbuf_next = qdf_nbuf_next(nbuf);
		qdf_nbuf_set_dev(nbuf, dp_intf->dev);

		++stats->per_cpu[cpu_index].rx_packets;
		qdf_net_stats_add_rx_pkts(&dp_intf->stats, 1);
		qdf_net_stats_add_rx_bytes(&dp_intf->stats,
					   qdf_nbuf_len(nbuf));

		/* Remove SKB from internal tracking table before submitting
		 * it to stack
		 */
		qdf_net_buf_debug_release_skb(nbuf);

		/*
		 * If this is not a last packet on the chain
		 * Just put packet into backlog queue, not scheduling RX sirq
		 */
		if (qdf_nbuf_next(nbuf)) {
			status = dp_intf->dp_ctx->dp_ops.dp_nbuf_push_pkt(nbuf,
							DP_NBUF_PUSH_SIMPLE);
		} else {
			/*
			 * This is the last packet on the chain
			 * Scheduling rx sirq
			 */
			status = dp_intf->dp_ctx->dp_ops.dp_nbuf_push_pkt(nbuf,
							DP_NBUF_PUSH_NAPI);
		}

		if (QDF_IS_STATUS_SUCCESS(status))
			++stats->per_cpu[cpu_index].rx_delivered;
		else
			++stats->per_cpu[cpu_index].rx_refused;

		nbuf = nbuf_next;
	}

	return QDF_STATUS_SUCCESS;
}

void dp_monitor_set_rx_monitor_cb(struct ol_txrx_ops *txrx,
				  ol_txrx_rx_mon_fp rx_monitor_cb)
{
	txrx->rx.mon = rx_monitor_cb;
}

void dp_rx_monitor_callback(ol_osif_vdev_handle context,
			    qdf_nbuf_t rxbuf,
			    void *rx_status)
{
	dp_mon_rx_packet_cbk(context, rxbuf);
}
#endif

/**
 * dp_is_rx_wake_lock_needed() - check if wake lock is needed
 * @nbuf: pointer to sk_buff
 *
 * RX wake lock is needed for:
 * 1) Unicast data packet OR
 * 2) Local ARP data packet
 *
 * Return: true if wake lock is needed or false otherwise.
 */
static bool dp_is_rx_wake_lock_needed(qdf_nbuf_t nbuf)
{
	if ((!qdf_nbuf_pkt_type_is_mcast(nbuf) &&
	     !qdf_nbuf_pkt_type_is_bcast(nbuf)) ||
	    qdf_nbuf_is_arp_local(nbuf))
		return true;

	return false;
}

#ifdef RECEIVE_OFFLOAD
/**
 * dp_resolve_rx_ol_mode() - Resolve Rx offload method, LRO or GRO
 * @dp_ctx: pointer to DP psoc Context
 *
 * Return: None
 */
static void dp_resolve_rx_ol_mode(struct wlan_dp_psoc_context *dp_ctx)
{
	void *soc;

	soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!(cdp_cfg_get(soc, cfg_dp_lro_enable) ^
	    cdp_cfg_get(soc, cfg_dp_gro_enable))) {
		cdp_cfg_get(soc, cfg_dp_lro_enable) &&
			cdp_cfg_get(soc, cfg_dp_gro_enable) ?
		dp_info("Can't enable both LRO and GRO, disabling Rx offload"):
		dp_info("LRO and GRO both are disabled");
		dp_ctx->ol_enable = 0;
	} else if (cdp_cfg_get(soc, cfg_dp_lro_enable)) {
		dp_info("Rx offload LRO is enabled");
		dp_ctx->ol_enable = CFG_LRO_ENABLED;
	} else {
		dp_info("Rx offload: GRO is enabled");
		dp_ctx->ol_enable = CFG_GRO_ENABLED;
	}
}

#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
/**
 * dp_gro_rx_bh_disable() - GRO RX/flush function.
 * @dp_intf: DP interface pointer
 * @napi_to_use: napi to be used to give packets to the stack, gro flush
 * @nbuf: pointer to n/w buff
 *
 * Function calls napi_gro_receive for the skb. If the skb indicates that a
 * flush needs to be done (set by the lower DP layer), the function also calls
 * napi_gro_flush. Local softirqs are disabled (and later enabled) while making
 * napi_gro__ calls.
 *
 * Return: QDF_STATUS_SUCCESS if not dropped by napi_gro_receive or
 *	   QDF error code.
 */
static QDF_STATUS dp_gro_rx_bh_disable(struct wlan_dp_intf *dp_intf,
				       qdf_napi_struct *napi_to_use,
				       qdf_nbuf_t nbuf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	uint32_t rx_aggregation;
	uint8_t rx_ctx_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	uint8_t low_tput_force_flush = 0;
	int32_t gro_disallowed;

	rx_aggregation = qdf_atomic_read(&dp_ctx->dp_agg_param.rx_aggregation);
	gro_disallowed = qdf_atomic_read(&dp_intf->gro_disallowed);

	if (dp_get_current_throughput_level(dp_ctx) == PLD_BUS_WIDTH_IDLE ||
	    !rx_aggregation || gro_disallowed) {
		status = dp_ctx->dp_ops.dp_rx_napi_gro_flush(napi_to_use, nbuf,
						   &low_tput_force_flush);
		if (!low_tput_force_flush)
			dp_intf->dp_stats.tx_rx_stats.
					rx_gro_low_tput_flush++;
		if (!rx_aggregation)
			dp_ctx->dp_agg_param.gro_force_flush[rx_ctx_id] = 1;
		if (gro_disallowed)
			dp_intf->gro_flushed[rx_ctx_id] = 1;
	} else {
		status = dp_ctx->dp_ops.dp_rx_napi_gro_receive(napi_to_use,
							      nbuf);
	}

	return status;
}

#else /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */

/**
 * dp_gro_rx_bh_disable() - GRO RX/flush function.
 * @dp_intf: DP interface pointer
 * @napi_to_use: napi to be used to give packets to the stack, gro flush
 * @nbuf: pointer to nbuff
 *
 * Function calls napi_gro_receive for the skb. If the skb indicates that a
 * flush needs to be done (set by the lower DP layer), the function also calls
 * napi_gro_flush. Local softirqs are disabled (and later enabled) while making
 * napi_gro__ calls.
 *
 * Return: QDF_STATUS_SUCCESS if not dropped by napi_gro_receive or
 *	   QDF error code.
 */

static QDF_STATUS dp_gro_rx_bh_disable(struct wlan_dp_intf *dp_intf,
				       qdf_napi_struct *napi_to_use,
				       qdf_nbuf_t nbuf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	uint8_t low_tput_force_flush = 0;

	if (dp_get_current_throughput_level(dp_ctx) == PLD_BUS_WIDTH_IDLE) {
		status = dp_ctx->dp_ops.dp_rx_napi_gro_flush(napi_to_use, nbuf,
							&low_tput_force_flush);
		if (!low_tput_force_flush)
			dp_intf->dp_stats.tx_rx_stats.
					rx_gro_low_tput_flush++;
	} else {
		status = dp_ctx->dp_ops.dp_rx_napi_gro_receive(napi_to_use,
							      nbuf);
	}

	return status;
}
#endif /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */

#if defined(FEATURE_LRO)
/**
 * dp_lro_rx() - Handle Rx processing via LRO
 * @dp_intf: pointer to DP interface
 * @nbuf: pointer to n/w buff
 *
 * Return: QDF_STATUS_SUCCESS if processed via LRO or non zero return code
 */
static inline QDF_STATUS
dp_lro_rx(struct wlan_dp_intf *dp_intf, qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	return dp_ctx->dp_ops.dp_lro_rx_cb(dp_intf->dev, nbuf);
}

/**
 * dp_is_lro_enabled() - Is LRO enabled
 * @dp_ctx: DP interface
 *
 * This function checks if LRO is enabled in DP context.
 *
 * Return: 0 - success, < 0 - failure
 */
static inline QDF_STATUS
dp_is_lro_enabled(struct wlan_dp_psoc_context *dp_ctx)
{
	if (dp_ctx->ol_enable != CFG_LRO_ENABLED)
		return QDF_STATUS_E_NOSUPPORT;
}

QDF_STATUS dp_lro_set_reset(struct wlan_dp_intf *dp_intf, uint8_t enable_flag)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	if ((dp_ctx->ol_enable != CFG_LRO_ENABLED) ||
	    (dp_intf->device_mode != QDF_STA_MODE)) {
		dp_info("LRO is already Disabled");
		return QDF_STATUS_E_INVAL;
	}

	if (enable_flag) {
		qdf_atomic_set(&dp_ctx->vendor_disable_lro_flag, 0);
	} else {
		/* Disable LRO, Enable tcpdelack*/
		qdf_atomic_set(&dp_ctx->vendor_disable_lro_flag, 1);
		dp_info("LRO Disabled");

		if (dp_ctx->dp_cfg.enable_tcp_delack) {
			struct wlan_rx_tp_data rx_tp_data;

			dp_info("Enable TCP delack as LRO is disabled");
			rx_tp_data.rx_tp_flags = TCP_DEL_ACK_IND;
			rx_tp_data.level =
				DP_BUS_BW_CFG(dp_ctx->dp_cfg.cur_rx_level);
			wlan_dp_update_tcp_rx_param(dp_ctx, &rx_tp_data);
			dp_ctx->en_tcp_delack_no_lro = 1;
		}
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS dp_lro_rx(struct wlan_dp_intf *dp_intf,
		     qdf_nbuf_t nbuf)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
int dp_is_lro_enabled(struct wlan_dp_psoc_context *dp_ctx)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* FEATURE_LRO */

/**
 * dp_gro_rx_thread() - Handle Rx processing via GRO for DP thread
 * @dp_intf: pointer to DP interface
 * @nbuf: pointer to n/w buff
 *
 * Return: QDF_STATUS_SUCCESS if processed via GRO or non zero return code
 */
static
QDF_STATUS dp_gro_rx_thread(struct wlan_dp_intf *dp_intf,
			    qdf_nbuf_t nbuf)
{
	qdf_napi_struct *napi_to_use = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!dp_intf->dp_ctx->enable_dp_rx_threads) {
		dp_err_rl("gro not supported without DP RX thread!");
		return status;
	}

	napi_to_use =
		(qdf_napi_struct *)dp_rx_get_napi_context(cds_get_context(QDF_MODULE_ID_SOC),
				       QDF_NBUF_CB_RX_CTX_ID(nbuf));

	if (!napi_to_use) {
		dp_err_rl("no napi to use for GRO!");
		return status;
	}

	return dp_gro_rx_bh_disable(dp_intf, napi_to_use, nbuf);
}

/**
 * dp_gro_rx_legacy() - Handle Rx processing via GRO for ihelium based targets
 * @dp_intf: pointer to DP interface
 * @nbuf: pointer to n/w buf
 *
 * Supports GRO for only station mode
 *
 * Return: QDF_STATUS_SUCCESS if processed via GRO or non zero return code
 */
static
QDF_STATUS dp_gro_rx_legacy(struct wlan_dp_intf *dp_intf, qdf_nbuf_t nbuf)
{
	qdf_napi_struct *napi_to_use;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	/* Only enabling it for STA mode like LRO today */
	if (QDF_STA_MODE != dp_intf->device_mode)
		return QDF_STATUS_E_NOSUPPORT;

	if (qdf_atomic_read(&dp_ctx->disable_rx_ol_in_low_tput) ||
	    qdf_atomic_read(&dp_ctx->disable_rx_ol_in_concurrency))
		return QDF_STATUS_E_NOSUPPORT;

	napi_to_use = dp_ctx->dp_ops.dp_gro_rx_legacy_get_napi(nbuf,
						dp_ctx->enable_rxthread);
	if (!napi_to_use)
		goto out;

	status = dp_gro_rx_bh_disable(dp_intf, napi_to_use, nbuf);
out:

	return status;
}

/**
 * dp_register_rx_ol_cb() - Register LRO/GRO rx processing callbacks
 * @dp_ctx: pointer to dp_ctx
 * @wifi3_0_target: whether its a lithium/beryllium arch based target or not
 *
 * Return: none
 */
static void dp_register_rx_ol_cb(struct wlan_dp_psoc_context *dp_ctx,
				 bool wifi3_0_target)
{
	if  (!dp_ctx) {
		dp_err("DP context is NULL");
		return;
	}

	dp_ctx->en_tcp_delack_no_lro = 0;

	if (!dp_is_lro_enabled(dp_ctx)) {
		dp_ctx->dp_ops.dp_register_rx_offld_flush_cb(DP_RX_FLUSH_LRO);
		dp_ctx->receive_offload_cb = dp_lro_rx;
		dp_info("LRO is enabled");
	} else if (dp_ctx->ol_enable == CFG_GRO_ENABLED) {
		qdf_atomic_set(&dp_ctx->dp_agg_param.rx_aggregation, 1);
		if (wifi3_0_target) {
		/* no flush registration needed, it happens in DP thread */
			dp_ctx->receive_offload_cb = dp_gro_rx_thread;
		} else {
			/*ihelium based targets */
			if (dp_ctx->enable_rxthread)
				dp_ctx->dp_ops.dp_register_rx_offld_flush_cb(
							DP_RX_FLUSH_THREAD);
			else
				dp_ctx->dp_ops.dp_register_rx_offld_flush_cb(
							DP_RX_FLUSH_NAPI);
			dp_ctx->receive_offload_cb = dp_gro_rx_legacy;
		}
		dp_info("GRO is enabled");
	} else if (DP_BUS_BW_CFG(dp_ctx->dp_cfg.enable_tcp_delack)) {
		dp_ctx->en_tcp_delack_no_lro = 1;
		dp_info("TCP Del ACK is enabled");
	}
}

/**
 * dp_rx_ol_send_config() - Send RX offload configuration to FW
 * @dp_ctx: pointer to DP_ctx
 *
 * This function is only used for non lithium targets. Lithium based targets are
 * sending LRO config to FW in vdev attach implemented in cmn DP layer.
 *
 * Return: 0 on success, non zero on failure
 */
static QDF_STATUS dp_rx_ol_send_config(struct wlan_dp_psoc_context *dp_ctx)
{
	struct cdp_lro_hash_config lro_config = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/*
	 * This will enable flow steering and Toeplitz hash
	 * So enable it for LRO or GRO processing.
	 */
	if (dp_ctx->dp_cfg.gro_enable ||
	    dp_ctx->dp_cfg.lro_enable) {
		lro_config.lro_enable = 1;
		lro_config.tcp_flag = QDF_TCPHDR_ACK;
		lro_config.tcp_flag_mask = QDF_TCPHDR_FIN | QDF_TCPHDR_SYN |
					   QDF_TCPHDR_RST | QDF_TCPHDR_ACK |
					   QDF_TCPHDR_URG | QDF_TCPHDR_ECE |
					   QDF_TCPHDR_CWR;
	}

	qdf_get_random_bytes(lro_config.toeplitz_hash_ipv4,
			     (sizeof(lro_config.toeplitz_hash_ipv4[0]) *
			      LRO_IPV4_SEED_ARR_SZ));

	qdf_get_random_bytes(lro_config.toeplitz_hash_ipv6,
			     (sizeof(lro_config.toeplitz_hash_ipv6[0]) *
			      LRO_IPV6_SEED_ARR_SZ));

	status = dp_ctx->sb_ops.dp_lro_config_cmd(dp_ctx->psoc, &lro_config);
	dp_info("LRO Config: lro_enable: 0x%x tcp_flag 0x%x tcp_flag_mask 0x%x",
		lro_config.lro_enable, lro_config.tcp_flag,
		lro_config.tcp_flag_mask);

	return status;
}

QDF_STATUS dp_rx_ol_init(struct wlan_dp_psoc_context *dp_ctx,
			 bool is_wifi3_0_target)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	dp_resolve_rx_ol_mode(dp_ctx);
	dp_register_rx_ol_cb(dp_ctx, is_wifi3_0_target);

	dp_info("ol init");
	if (!is_wifi3_0_target) {
		status = dp_rx_ol_send_config(dp_ctx);
		if (status) {
			dp_ctx->ol_enable = 0;
			dp_err("Failed to send LRO/GRO configuration! %u", status);
			return status;
		}
	}

	return 0;
}

void dp_disable_rx_ol_for_low_tput(struct wlan_dp_psoc_context *dp_ctx,
				   bool disable)
{
	if (disable)
		qdf_atomic_set(&dp_ctx->disable_rx_ol_in_low_tput, 1);
	else
		qdf_atomic_set(&dp_ctx->disable_rx_ol_in_low_tput, 0);
}

#else /* RECEIVE_OFFLOAD */
void dp_disable_rx_ol_for_low_tput(struct wlan_dp_psoc_context *dp_ctx,
				   bool disable)
{
}
#endif /* RECEIVE_OFFLOAD */

#ifdef WLAN_FEATURE_TSF_PLUS_SOCK_TS
static inline void dp_tsf_timestamp_rx(struct wlan_dp_psoc_context *dp_ctx,
				       qdf_nbuf_t netbuf)
{
	dp_ctx->dp_ops.dp_tsf_timestamp_rx(dp_ctx->dp_ops.callback_ctx,
					   netbuf);
}
#else
static inline void dp_tsf_timestamp_rx(struct wlan_dp_psoc_context *dp_ctx,
				       qdf_nbuf_t netbuf)
{
}
#endif

QDF_STATUS
dp_rx_thread_gro_flush_ind_cbk(void *intf_ctx, int rx_ctx_id)
{
	struct wlan_dp_intf *dp_intf = intf_ctx;
	enum dp_rx_gro_flush_code gro_flush_code = DP_RX_GRO_NORMAL_FLUSH;

	if (qdf_unlikely((!dp_intf) || (!dp_intf->dp_ctx))) {
		dp_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}

	if (dp_intf->runtime_disable_rx_thread)
		return QDF_STATUS_SUCCESS;

	if (dp_is_low_tput_gro_enable(dp_intf->dp_ctx)) {
		dp_intf->dp_stats.tx_rx_stats.rx_gro_flush_skip++;
		gro_flush_code = DP_RX_GRO_LOW_TPUT_FLUSH;
	}

	return dp_rx_gro_flush_ind(cds_get_context(QDF_MODULE_ID_SOC),
				   rx_ctx_id, gro_flush_code);
}

QDF_STATUS dp_rx_pkt_thread_enqueue_cbk(void *intf_ctx,
					qdf_nbuf_t nbuf_list)
{
	struct wlan_dp_intf *dp_intf;
	uint8_t intf_id;
	qdf_nbuf_t head_ptr;

	if (qdf_unlikely(!intf_ctx || !nbuf_list)) {
		dp_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf = (struct wlan_dp_intf *)intf_ctx;
	if (is_dp_intf_valid(dp_intf))
		return QDF_STATUS_E_FAILURE;

	if (dp_intf->runtime_disable_rx_thread &&
	    dp_intf->rx_stack)
		return dp_intf->rx_stack(dp_intf, nbuf_list);

	intf_id = dp_intf->intf_id;

	head_ptr = nbuf_list;
	while (head_ptr) {
		qdf_nbuf_cb_update_vdev_id(head_ptr,
					   intf_id);
		head_ptr = qdf_nbuf_next(head_ptr);
	}

	return dp_rx_enqueue_pkt(cds_get_context(QDF_MODULE_ID_SOC), nbuf_list);
}

#ifdef CONFIG_HL_SUPPORT
QDF_STATUS wlan_dp_rx_deliver_to_stack(struct wlan_dp_intf *dp_intf,
				       qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;

	dp_intf->dp_stats.tx_rx_stats.rx_non_aggregated++;
	dp_ctx->no_rx_offload_pkt_cnt++;

	return dp_ctx->dp_ops.dp_nbuf_push_pkt(nbuf, DP_NBUF_PUSH_NI);
}
#else

#if defined(WLAN_SUPPORT_RX_FISA)
/**
 * wlan_dp_set_fisa_disallowed_for_vdev() - Set fisa disallowed bit for a vdev
 * @soc: DP soc handle
 * @vdev_id: Vdev id
 * @rx_ctx_id: rx context id
 * @val: Enable or disable
 *
 * The function sets the fisa disallowed flag for a given vdev
 *
 * Return: None
 */
static inline
void wlan_dp_set_fisa_disallowed_for_vdev(ol_txrx_soc_handle soc,
					  uint8_t vdev_id,
					  uint8_t rx_ctx_id, uint8_t val)
{
	dp_set_fisa_disallowed_for_vdev(soc, vdev_id, rx_ctx_id, val);
}
#else
static inline
void wlan_dp_set_fisa_disallowed_for_vdev(ol_txrx_soc_handle soc,
					  uint8_t vdev_id,
					  uint8_t rx_ctx_id, uint8_t val)
{
}
#endif

#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
QDF_STATUS wlan_dp_rx_deliver_to_stack(struct wlan_dp_intf *dp_intf,
				       qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_ctx->dp_ops;
	int status = QDF_STATUS_E_FAILURE;
	bool nbuf_receive_offload_ok = false;
	enum dp_nbuf_push_type push_type;
	uint8_t rx_ctx_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);
	int32_t gro_disallowed;

	if (QDF_NBUF_CB_RX_TCP_PROTO(nbuf) &&
	    !QDF_NBUF_CB_RX_PEER_CACHED_FRM(nbuf))
		nbuf_receive_offload_ok = true;

	gro_disallowed = qdf_atomic_read(&dp_intf->gro_disallowed);
	if (gro_disallowed == 0 &&
	    dp_intf->gro_flushed[rx_ctx_id] != 0) {
		if (qdf_likely(soc))
			wlan_dp_set_fisa_disallowed_for_vdev(soc,
							     dp_intf->intf_id,
							     rx_ctx_id, 0);
		dp_intf->gro_flushed[rx_ctx_id] = 0;
	} else if (gro_disallowed &&
		   dp_intf->gro_flushed[rx_ctx_id] == 0) {
		if (qdf_likely(soc))
			wlan_dp_set_fisa_disallowed_for_vdev(soc,
							     dp_intf->intf_id,
							     rx_ctx_id, 1);
	}

	if (nbuf_receive_offload_ok && dp_ctx->receive_offload_cb &&
	    !dp_ctx->dp_agg_param.gro_force_flush[rx_ctx_id] &&
	    !dp_intf->gro_flushed[rx_ctx_id] &&
	    !dp_intf->runtime_disable_rx_thread) {
		status = dp_ctx->receive_offload_cb(dp_intf, nbuf);

		if (QDF_IS_STATUS_SUCCESS(status)) {
			dp_intf->dp_stats.tx_rx_stats.rx_aggregated++;
			return status;
		}

		if (status == QDF_STATUS_E_GRO_DROP) {
			dp_intf->dp_stats.tx_rx_stats.rx_gro_dropped++;
			return status;
		}
	}

	/*
	 * The below case handles the scenario when rx_aggregation is
	 * re-enabled dynamically, in which case gro_force_flush needs
	 * to be reset to 0 to allow GRO.
	 */
	if (qdf_atomic_read(&dp_ctx->dp_agg_param.rx_aggregation) &&
	    dp_ctx->dp_agg_param.gro_force_flush[rx_ctx_id])
		dp_ctx->dp_agg_param.gro_force_flush[rx_ctx_id] = 0;

	dp_intf->dp_stats.tx_rx_stats.rx_non_aggregated++;

	/* Account for GRO/LRO ineligible packets, mostly UDP */
	if (qdf_nbuf_get_gso_segs(nbuf) == 0)
		dp_ctx->no_rx_offload_pkt_cnt++;

	if (qdf_likely((dp_ctx->enable_dp_rx_threads ||
			dp_ctx->enable_rxthread) &&
		       !dp_intf->runtime_disable_rx_thread)) {
		push_type = DP_NBUF_PUSH_BH_DISABLE;
	} else if (qdf_unlikely(QDF_NBUF_CB_RX_PEER_CACHED_FRM(nbuf))) {
		/*
		 * Frames before peer is registered to avoid contention with
		 * NAPI softirq.
		 * Refer fix:
		 * qcacld-3.0: Do netif_rx_ni() for frames received before
		 * peer assoc
		 */
		push_type = DP_NBUF_PUSH_NI;
	} else { /* NAPI Context */
		push_type = DP_NBUF_PUSH_NAPI;
	}

	return dp_ops->dp_nbuf_push_pkt(nbuf, push_type);
}

#else /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */

QDF_STATUS wlan_dp_rx_deliver_to_stack(struct wlan_dp_intf *dp_intf,
				       qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_ctx->dp_ops;
	int status = QDF_STATUS_E_FAILURE;
	bool nbuf_receive_offload_ok = false;
	enum dp_nbuf_push_type push_type;

	if (QDF_NBUF_CB_RX_TCP_PROTO(nbuf) &&
	    !QDF_NBUF_CB_RX_PEER_CACHED_FRM(nbuf))
		nbuf_receive_offload_ok = true;

	if (nbuf_receive_offload_ok && dp_ctx->receive_offload_cb) {
		status = dp_ctx->receive_offload_cb(dp_intf, nbuf);

		if (QDF_IS_STATUS_SUCCESS(status)) {
			dp_intf->dp_stats.tx_rx_stats.rx_aggregated++;
			return status;
		}

		if (status == QDF_STATUS_E_GRO_DROP) {
			dp_intf->dp_stats.tx_rx_stats.rx_gro_dropped++;
			return status;
		}
	}

	dp_intf->dp_stats.tx_rx_stats.rx_non_aggregated++;

	/* Account for GRO/LRO ineligible packets, mostly UDP */
	if (qdf_nbuf_get_gso_segs(nbuf) == 0)
		dp_ctx->no_rx_offload_pkt_cnt++;

	if (qdf_likely((dp_ctx->enable_dp_rx_threads ||
			dp_ctx->enable_rxthread) &&
		       !dp_intf->runtime_disable_rx_thread)) {
		push_type = DP_NBUF_PUSH_BH_DISABLE;
	} else if (qdf_unlikely(QDF_NBUF_CB_RX_PEER_CACHED_FRM(nbuf))) {
		/*
		 * Frames before peer is registered to avoid contention with
		 * NAPI softirq.
		 * Refer fix:
		 * qcacld-3.0: Do netif_rx_ni() for frames received before
		 * peer assoc
		 */
		push_type = DP_NBUF_PUSH_NI;
	} else { /* NAPI Context */
		push_type = DP_NBUF_PUSH_NAPI;
	}

	return dp_ops->dp_nbuf_push_pkt(nbuf, push_type);
}
#endif /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */
#endif

static inline bool
dp_is_gratuitous_arp_unsolicited_na(struct wlan_dp_psoc_context *dp_ctx,
				    qdf_nbuf_t nbuf)
{
	if (qdf_unlikely(dp_ctx->dp_ops.dp_is_gratuitous_arp_unsolicited_na))
		return dp_ctx->dp_ops.dp_is_gratuitous_arp_unsolicited_na(nbuf);

	return false;
}

QDF_STATUS dp_rx_flush_packet_cbk(void *dp_intf_context, uint8_t intf_id)
{
	struct wlan_dp_intf *dp_intf = (struct wlan_dp_intf *)dp_intf_context;
	struct wlan_dp_psoc_context *dp_ctx;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (qdf_unlikely(!soc))
		return QDF_STATUS_E_FAILURE;

	dp_ctx = dp_intf->dp_ctx;
	if (qdf_unlikely(!dp_ctx))
		return QDF_STATUS_E_FAILURE;

	qdf_atomic_inc(&dp_intf->num_active_task);

	/* do fisa flush for this vdev */
	if (dp_ctx->dp_cfg.fisa_enable)
		wlan_dp_rx_fisa_flush_by_vdev_id((struct dp_soc *)soc, intf_id);

	if (dp_ctx->enable_dp_rx_threads)
		dp_txrx_flush_pkts_by_vdev_id(soc, intf_id);

	qdf_atomic_dec(&dp_intf->num_active_task);

	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_SUPPORT_RX_FISA)
QDF_STATUS wlan_dp_rx_fisa_cbk(void *dp_soc,
			       void *dp_vdev, qdf_nbuf_t nbuf_list)
{
	return dp_fisa_rx((struct dp_soc *)dp_soc, (struct dp_vdev *)dp_vdev,
			  nbuf_list);
}

QDF_STATUS wlan_dp_rx_fisa_flush_by_ctx_id(void *dp_soc, int ring_num)
{
	return dp_rx_fisa_flush_by_ctx_id((struct dp_soc *)dp_soc, ring_num);
}

QDF_STATUS wlan_dp_rx_fisa_flush_by_vdev_id(void *dp_soc, uint8_t vdev_id)
{
	return dp_rx_fisa_flush_by_vdev_id((struct dp_soc *)dp_soc, vdev_id);
}
#endif

QDF_STATUS dp_rx_packet_cbk(void *dp_intf_context,
			    qdf_nbuf_t rxBuf)
{
	struct wlan_dp_intf *dp_intf = NULL;
	struct wlan_dp_psoc_context *dp_ctx = NULL;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	qdf_nbuf_t nbuf = NULL;
	qdf_nbuf_t next = NULL;
	unsigned int cpu_index;
	struct qdf_mac_addr *mac_addr, *dest_mac_addr;
	bool wake_lock = false;
	bool track_arp = false;
	enum qdf_proto_subtype subtype = QDF_PROTO_INVALID;
	bool is_eapol, send_over_nl;
	bool is_dhcp;
	struct dp_tx_rx_stats *stats;
	QDF_STATUS status;
	uint8_t pkt_type;

	/* Sanity check on inputs */
	if (qdf_unlikely((!dp_intf_context) || (!rxBuf))) {
		dp_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf = (struct wlan_dp_intf *)dp_intf_context;
	dp_ctx = dp_intf->dp_ctx;

	cpu_index = qdf_get_cpu();
	stats = &dp_intf->dp_stats.tx_rx_stats;

	next = rxBuf;

	while (next) {
		nbuf = next;
		next = qdf_nbuf_next(nbuf);
		qdf_nbuf_set_next(nbuf, NULL);
		is_eapol = false;
		is_dhcp = false;
		send_over_nl = false;

		if (qdf_nbuf_is_ipv4_arp_pkt(nbuf)) {
			if (qdf_nbuf_data_is_arp_rsp(nbuf) &&
			    (dp_intf->track_arp_ip ==
			     qdf_nbuf_get_arp_src_ip(nbuf))) {
				++dp_intf->dp_stats.arp_stats.
					rx_arp_rsp_count;
				dp_debug("ARP packet received");
				track_arp = true;
			}
		} else if (qdf_nbuf_is_ipv4_eapol_pkt(nbuf)) {
			subtype = qdf_nbuf_get_eapol_subtype(nbuf);
			send_over_nl = true;

			/* Mac address check between RX packet DA and dp_intf's */
			dp_rx_pkt_da_check(dp_intf, nbuf);
			if (subtype == QDF_PROTO_EAPOL_M1) {
				++dp_intf->dp_stats.eapol_stats.
						eapol_m1_count;
				is_eapol = true;
			} else if (subtype == QDF_PROTO_EAPOL_M3) {
				++dp_intf->dp_stats.eapol_stats.
						eapol_m3_count;
				is_eapol = true;
			}
		} else if (qdf_nbuf_is_ipv4_dhcp_pkt(nbuf)) {
			subtype = qdf_nbuf_get_dhcp_subtype(nbuf);
			if (subtype == QDF_PROTO_DHCP_OFFER) {
				++dp_intf->dp_stats.dhcp_stats.
						dhcp_off_count;
				is_dhcp = true;
			} else if (subtype == QDF_PROTO_DHCP_ACK) {
				++dp_intf->dp_stats.dhcp_stats.
						dhcp_ack_count;
				is_dhcp = true;
			}
		}

		wlan_dp_pkt_add_timestamp(dp_intf, QDF_PKT_RX_DRIVER_EXIT,
					  nbuf);

		/* track connectivity stats */
		if (dp_intf->pkt_type_bitmap)
			dp_tx_rx_collect_connectivity_stats_info(nbuf, dp_intf,
								 PKT_TYPE_RSP,
								 &pkt_type);

		if ((dp_intf->conn_info.proxy_arp_service) &&
		    dp_is_gratuitous_arp_unsolicited_na(dp_ctx, nbuf)) {
			qdf_atomic_inc(&stats->rx_usolict_arp_n_mcast_drp);
			/* Remove SKB from internal tracking table before
			 * submitting it to stack.
			 */
			qdf_nbuf_free(nbuf);
			continue;
		}

		dp_event_eapol_log(nbuf, QDF_RX);
		qdf_dp_trace_log_pkt(dp_intf->intf_id, nbuf, QDF_RX,
				     QDF_TRACE_DEFAULT_PDEV_ID,
				     dp_intf->device_mode);

		DPTRACE(qdf_dp_trace(nbuf,
				     QDF_DP_TRACE_RX_PACKET_PTR_RECORD,
				     QDF_TRACE_DEFAULT_PDEV_ID,
				     qdf_nbuf_data_addr(nbuf),
				     sizeof(qdf_nbuf_data(nbuf)), QDF_RX));

		DPTRACE(qdf_dp_trace_data_pkt(nbuf, QDF_TRACE_DEFAULT_PDEV_ID,
					      QDF_DP_TRACE_RX_PACKET_RECORD,
					      0, QDF_RX));

		dest_mac_addr = (struct qdf_mac_addr *)(qdf_nbuf_data(nbuf) +
						QDF_NBUF_DEST_MAC_OFFSET);
		mac_addr = (struct qdf_mac_addr *)(qdf_nbuf_data(nbuf) +
						   QDF_NBUF_SRC_MAC_OFFSET);

		status = wlan_objmgr_vdev_try_get_ref(dp_intf->vdev,
						      WLAN_TDLS_SB_ID);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			wlan_tdls_update_rx_pkt_cnt(dp_intf->vdev, mac_addr,
						    dest_mac_addr);
			wlan_objmgr_vdev_release_ref(dp_intf->vdev,
						     WLAN_TDLS_SB_ID);
		}

		if (dp_rx_pkt_tracepoints_enabled())
			qdf_trace_dp_packet(nbuf, QDF_RX, NULL, 0);

		qdf_nbuf_set_dev(nbuf, dp_intf->dev);
		qdf_nbuf_set_protocol_eth_tye_trans(nbuf);
		++stats->per_cpu[cpu_index].rx_packets;
		qdf_net_stats_add_rx_pkts(&dp_intf->stats, 1);
		/* count aggregated RX frame into stats */
		qdf_net_stats_add_rx_pkts(&dp_intf->stats,
					  qdf_nbuf_get_gso_segs(nbuf));
		qdf_net_stats_add_rx_bytes(&dp_intf->stats,
					   qdf_nbuf_len(nbuf));

		/* Incr GW Rx count for NUD tracking based on GW mac addr */
		dp_nud_incr_gw_rx_pkt_cnt(dp_intf, mac_addr);

		/* Check & drop replayed mcast packets (for IPV6) */
		if (dp_ctx->dp_cfg.multicast_replay_filter &&
				qdf_nbuf_is_mcast_replay(nbuf)) {
			qdf_atomic_inc(&stats->rx_usolict_arp_n_mcast_drp);
			qdf_nbuf_free(nbuf);
			continue;
		}

		/* hold configurable wakelock for unicast traffic */
		if (!dp_is_current_high_throughput(dp_ctx) &&
		    dp_ctx->dp_cfg.rx_wakelock_timeout &&
		    dp_intf->conn_info.is_authenticated)
			wake_lock = dp_is_rx_wake_lock_needed(nbuf);

		if (wake_lock) {
			cds_host_diag_log_work(&dp_ctx->rx_wake_lock,
					dp_ctx->dp_cfg.rx_wakelock_timeout,
					WIFI_POWER_EVENT_WAKELOCK_HOLD_RX);
			qdf_wake_lock_timeout_acquire(&dp_ctx->rx_wake_lock,
					dp_ctx->dp_cfg.rx_wakelock_timeout);
		}

		/* Remove SKB from internal tracking table before submitting
		 * it to stack
		 */
		qdf_net_buf_debug_release_skb(nbuf);

		dp_tsf_timestamp_rx(dp_ctx, nbuf);

		if (send_over_nl && dp_ctx->dp_ops.dp_send_rx_pkt_over_nl) {
			if (dp_ctx->dp_ops.dp_send_rx_pkt_over_nl(dp_intf->dev,
					(u8 *)&dp_intf->conn_info.peer_macaddr,
								  nbuf, false))
				qdf_status = QDF_STATUS_SUCCESS;
			else
				qdf_status = QDF_STATUS_E_INVAL;
			qdf_nbuf_dev_kfree(nbuf);
		} else {
			qdf_status = wlan_dp_rx_deliver_to_stack(dp_intf, nbuf);
		}

		if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
			++stats->per_cpu[cpu_index].rx_delivered;
			if (track_arp)
				++dp_intf->dp_stats.arp_stats.rx_delivered;
			if (is_eapol)
				++dp_intf->dp_stats.eapol_stats.
				rx_delivered[subtype - QDF_PROTO_EAPOL_M1];
			else if (is_dhcp)
				++dp_intf->dp_stats.dhcp_stats.
				rx_delivered[subtype - QDF_PROTO_DHCP_DISCOVER];

			/* track connectivity stats */
			if (dp_intf->pkt_type_bitmap)
				dp_tx_rx_collect_connectivity_stats_info(
					nbuf, dp_intf,
					PKT_TYPE_RX_DELIVERED,
					&pkt_type);
		} else {
			++stats->per_cpu[cpu_index].rx_refused;
			if (track_arp)
				++dp_intf->dp_stats.arp_stats.rx_refused;

			if (is_eapol)
				++dp_intf->dp_stats.eapol_stats.
				       rx_refused[subtype - QDF_PROTO_EAPOL_M1];
			else if (is_dhcp)
				++dp_intf->dp_stats.dhcp_stats.
				  rx_refused[subtype - QDF_PROTO_DHCP_DISCOVER];

			/* track connectivity stats */
			if (dp_intf->pkt_type_bitmap)
				dp_tx_rx_collect_connectivity_stats_info(
					nbuf, dp_intf,
					PKT_TYPE_RX_REFUSED,
					&pkt_type);
		}
	}

	return QDF_STATUS_SUCCESS;
}

