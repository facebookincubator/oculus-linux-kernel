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

#ifndef __WLAN_DP_TXRX_H__
#define __WLAN_DP_TXRX_H__

#include <cds_api.h>
#include <qdf_tracepoint.h>
#include <qdf_pkt_add_timestamp.h>
#include <enet.h>
#include <qdf_tracepoint.h>
#include "wlan_dp_priv.h"

/** DP Tx Time out value */
#define DP_TX_TIMEOUT   qdf_system_msecs_to_ticks(5000)

#define DP_TX_STALL_THRESHOLD 4

#ifdef FEATURE_WLAN_WAPI
#define IS_DP_ETHERTYPE_WAI(_nbuf) (qdf_ntohs(qdf_nbuf_get_protocol(_nbuf)) == \
								ETHERTYPE_WAI)
#else
#define IS_DP_ETHERTYPE_WAI(_nbuf) (false)
#endif

#define DP_CONNECTIVITY_CHECK_SET_ARP		1
#define DP_CONNECTIVITY_CHECK_SET_DNS		2
#define DP_CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE	3
#define DP_CONNECTIVITY_CHECK_SET_ICMPV4	4
#define DP_CONNECTIVITY_CHECK_SET_ICMPV6	5
#define DP_CONNECTIVITY_CHECK_SET_TCP_SYN	6
#define DP_CONNECTIVITY_CHECK_SET_TCP_SYN_ACK	7
#define DP_CONNECTIVITY_CHECK_SET_TCP_ACK	8

/**
 * wlan_dp_intf_get_pkt_type_bitmap_value() - Get packt type bitmap info
 * @intf_ctx: DP interface context
 *
 * Return: bitmap information
 */
uint32_t wlan_dp_intf_get_pkt_type_bitmap_value(void *intf_ctx);

#if defined(WLAN_SUPPORT_RX_FISA)
/**
 * dp_rx_skip_fisa() - Set flags to skip fisa aggregation
 * @dp_ctx: DP component handle
 * @value: allow or skip fisa
 *
 * Return: None
 */
void dp_rx_skip_fisa(struct wlan_dp_psoc_context *dp_ctx, uint32_t value);
#endif

/**
 * dp_reset_all_intfs_connectivity_stats() - reset connectivity stats
 * @dp_ctx: pointer to DP Context
 *
 * Return: None
 */
void dp_reset_all_intfs_connectivity_stats(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_softap_check_wait_for_tx_eap_pkt() - Check and wait for eap failure
 * pkt completion event
 * @dp_intf: pointer to DP interface
 * @mac_addr: mac address of peer
 *
 * Check and wait for eap failure pkt tx completion.
 *
 * Return: void
 */
void dp_softap_check_wait_for_tx_eap_pkt(struct wlan_dp_intf *dp_intf,
					 struct qdf_mac_addr *mac_addr);

#ifdef SAP_DHCP_FW_IND
/**
 * dp_post_dhcp_ind() - Send DHCP START/STOP indication to FW
 * @dp_link: DP link handle
 * @mac_addr: mac address
 * @dhcp_start: true if DHCP start, otherwise DHCP stop
 *
 * Return: error number
 */
int dp_post_dhcp_ind(struct wlan_dp_link *dp_link,
		     uint8_t *mac_addr, bool dhcp_start);

/**
 * dp_softap_inspect_dhcp_packet() - Inspect DHCP packet
 * @dp_link: DP link handle
 * @nbuf: pointer to OS packet (sk_buff)
 * @dir: direction
 *
 * Inspect the Tx/Rx frame, and send DHCP START/STOP notification to the FW
 * through WMI message, during DHCP based IP address acquisition phase.
 *
 * - Send DHCP_START notification to FW when SAP gets DHCP Discovery
 * - Send DHCP_STOP notification to FW when SAP sends DHCP ACK/NAK
 *
 * DHCP subtypes are determined by a status octet in the DHCP Message type
 * option (option code 53 (0x35)).
 *
 * Each peer will be in one of 4 DHCP phases, starts from QDF_DHCP_PHASE_ACK,
 * and transitioned per DHCP message type as it arrives.
 *
 * - QDF_DHCP_PHASE_DISCOVER: upon receiving DHCP_DISCOVER message in ACK phase
 * - QDF_DHCP_PHASE_OFFER: upon receiving DHCP_OFFER message in DISCOVER phase
 * - QDF_DHCP_PHASE_REQUEST: upon receiving DHCP_REQUEST message in OFFER phase
 *	or ACK phase (Renewal process)
 * - QDF_DHCP_PHASE_ACK : upon receiving DHCP_ACK/NAK message in REQUEST phase
 *	or DHCP_DELINE message in OFFER phase
 *
 * Return: error number
 */
int dp_softap_inspect_dhcp_packet(struct wlan_dp_link *dp_link,
				  qdf_nbuf_t nbuf,
				  enum qdf_proto_dir dir);
#else
static inline
int dp_post_dhcp_ind(struct wlan_dp_link *dp_link,
		     uint8_t *mac_addr, bool dhcp_start)
{
	return 0;
}

static inline
int dp_softap_inspect_dhcp_packet(struct wlan_dp_link *dp_link,
				  qdf_nbuf_t nbuf,
				  enum qdf_proto_dir dir)
{
	return 0;
}
#endif

/**
 * dp_rx_flush_packet_cbk() - flush rx packet handler
 * @dp_link_context: pointer to DP link context
 * @link_id: vdev_id of the packets to be flushed
 *
 * Flush rx packet callback registered with data path. DP will call this to
 * notify when packets for a particular vdev is to be flushed out.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS dp_rx_flush_packet_cbk(void *dp_link_context, uint8_t link_id);

/**
 * dp_softap_start_xmit() - Transmit a frame for SAP interface
 * @nbuf: pointer to Network buffer
 * @dp_link: DP link handle
 *
 * Return: QDF_STATUS_SUCCESS on successful transmission
 */
QDF_STATUS dp_softap_start_xmit(qdf_nbuf_t nbuf, struct wlan_dp_link *dp_link);

/**
 * dp_softap_tx_timeout() - TX timeout handler
 * @dp_intf: pointer to DP interface
 *
 * Timeout API called for mode interfaces (SoftAP/P2P GO)
 * when TX transmission takes too long.
 * called by the OS_IF layer legacy driver.
 *
 * Return: None
 */
void dp_softap_tx_timeout(struct wlan_dp_intf *dp_intf);

/**
 * dp_softap_rx_packet_cbk() - Receive packet handler for SAP
 * @intf_ctx: pointer to DP interface context
 * @rx_buf: pointer to rx qdf_nbuf
 *
 * Receive callback registered with data path.  DP will call this to notify
 * when one or more packets were received for a registered
 * STA.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS
dp_softap_rx_packet_cbk(void *intf_ctx, qdf_nbuf_t rx_buf);

/**
 * dp_start_xmit() - Transmit a frame for STA interface
 * @nbuf: pointer to Network buffer
 * @dp_link: DP link handle
 *
 * Return: QDF_STATUS_SUCCESS on successful transmission
 */
QDF_STATUS
dp_start_xmit(struct wlan_dp_link *dp_link, qdf_nbuf_t nbuf);

/**
 * dp_tx_timeout() - DP Tx timeout API
 * @dp_intf: Data path interface pointer
 *
 * Function called by OS_IF there is any timeout during transmission.
 *
 * Return: none
 */
void dp_tx_timeout(struct wlan_dp_intf *dp_intf);

/**
 * dp_rx_packet_cbk() - Receive packet handler
 * @dp_link_context: pointer to DP link context
 * @rx_buf: pointer to rx qdf_nbuf
 *
 * Receive callback registered with data path.  DP will call this to notify
 * when one or more packets were received for a registered
 * STA.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS dp_rx_packet_cbk(void *dp_link_context, qdf_nbuf_t rx_buf);

#if defined(WLAN_SUPPORT_RX_FISA)
/**
 * wlan_dp_rx_fisa_cbk() - Entry function to FISA to handle aggregation
 * @dp_soc: core txrx main context
 * @dp_vdev: Handle DP vdev
 * @nbuf_list: List nbufs to be aggregated
 *
 * Return: Success on aggregation
 */
QDF_STATUS wlan_dp_rx_fisa_cbk(void *dp_soc, void *dp_vdev,
			       qdf_nbuf_t nbuf_list);

/**
 * wlan_dp_rx_fisa_flush_by_ctx_id() - Flush function to end of context
 *				   flushing of aggregates
 * @dp_soc: core txrx main context
 * @ring_num: REO number to flush the flow Rxed on the REO
 *
 * Return: Success on flushing the flows for the REO
 */
QDF_STATUS wlan_dp_rx_fisa_flush_by_ctx_id(void *dp_soc, int ring_num);

/**
 * wlan_dp_rx_fisa_flush_by_vdev_id() - Flush fisa aggregates per vdev id
 * @dp_soc: core txrx main context
 * @vdev_id: vdev ID
 *
 * Return: Success on flushing the flows for the vdev
 */
QDF_STATUS wlan_dp_rx_fisa_flush_by_vdev_id(void *dp_soc, uint8_t vdev_id);
#else
static inline QDF_STATUS wlan_dp_rx_fisa_flush_by_vdev_id(void *dp_soc,
							  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_dp_rx_deliver_to_stack() - DP helper function to deliver RX pkts to
 *                                 stack
 * @dp_intf: pointer to DP interface context
 * @nbuf: pointer to nbuf
 *
 * The function calls the appropriate stack function depending upon the packet
 * type and whether GRO/LRO is enabled.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS wlan_dp_rx_deliver_to_stack(struct wlan_dp_intf *dp_intf,
				       qdf_nbuf_t nbuf);

/**
 * dp_rx_thread_gro_flush_ind_cbk() - receive handler to flush GRO packets
 * @link_ctx: pointer to DP interface context
 * @rx_ctx_id: RX CTX Id for which flush should happen
 *
 * Receive callback registered with DP layer which flushes GRO packets
 * for a given RX CTX ID (RX Thread)
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS dp_rx_thread_gro_flush_ind_cbk(void *link_ctx, int rx_ctx_id);

/**
 * dp_rx_pkt_thread_enqueue_cbk() - receive pkt handler to enqueue into thread
 * @link_ctx: pointer to DP link context
 * @nbuf_list: pointer to qdf_nbuf list
 *
 * Receive callback registered with DP layer which enqueues packets into dp rx
 * thread
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS dp_rx_pkt_thread_enqueue_cbk(void *link_ctx,
					qdf_nbuf_t nbuf_list);

/**
 * dp_disable_rx_ol_for_low_tput() - Disable Rx offload in low TPUT scenario
 * @dp_ctx: dp context
 * @disable: true/false to disable/enable the Rx offload
 *
 * Return: none
 */
void dp_disable_rx_ol_for_low_tput(struct wlan_dp_psoc_context *dp_ctx,
				   bool disable);

/**
 * dp_tx_rx_collect_connectivity_stats_info() - collect connectivity stats
 * @nbuf: pointer to n/w buffer
 * @context: pointer to DP interface
 * @action: action done on pkt.
 * @pkt_type: data pkt type
 *
 * Return: None
 */
void
dp_tx_rx_collect_connectivity_stats_info(qdf_nbuf_t nbuf, void *context,
		enum connectivity_stats_pkt_status action, uint8_t *pkt_type);

static inline void
dp_nbuf_fill_gso_size(qdf_netdev_t dev, qdf_nbuf_t nbuf)
{
	unsigned long val;

	if (qdf_nbuf_is_cloned(nbuf) && qdf_nbuf_is_nonlinear(nbuf) &&
	    qdf_nbuf_get_gso_size(nbuf) == 0 &&
	    qdf_nbuf_is_ipv4_tcp_pkt(nbuf)) {
		val = dev->mtu - ((qdf_nbuf_transport_header(nbuf) -
				   qdf_nbuf_network_header(nbuf))
				  + qdf_nbuf_get_tcp_hdr_len(nbuf));
		qdf_nbuf_set_gso_size(nbuf, val);
	}
}

#ifdef CONFIG_HL_SUPPORT
static inline QDF_STATUS
dp_nbuf_nontso_linearize(qdf_nbuf_t nbuf)
{
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_nbuf_nontso_linearize(qdf_nbuf_t nbuf)
{
	if (qdf_nbuf_is_nonlinear(nbuf) && qdf_nbuf_is_tso(nbuf) == false) {
		if (qdf_unlikely(qdf_nbuf_linearize(nbuf)))
			return QDF_STATUS_E_NOMEM;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void dp_event_eapol_log(qdf_nbuf_t nbuf, enum qdf_proto_dir dir);
#else
static inline
void dp_event_eapol_log(qdf_nbuf_t nbuf, enum qdf_proto_dir dir)
{}
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
static inline
qdf_nbuf_t dp_nbuf_orphan(struct wlan_dp_intf *dp_intf,
			  qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_ctx->dp_ops;
	unsigned int tx_flow_low_watermark;
	int need_orphan = 0;
	int cpu;

	tx_flow_low_watermark =
	   dp_ops->dp_get_tx_flow_low_watermark(dp_ops->callback_ctx,
						dp_intf->dev);
	if (tx_flow_low_watermark > 0) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 19, 0))
		/*
		 * The TCP TX throttling logic is changed a little after
		 * 3.19-rc1 kernel, the TCP sending limit will be smaller,
		 * which will throttle the TCP packets to the host driver.
		 * The TCP UP LINK throughput will drop heavily. In order to
		 * fix this issue, need to orphan the socket buffer asap, which
		 * will call skb's destructor to notify the TCP stack that the
		 * SKB buffer is unowned. And then the TCP stack will pump more
		 * packets to host driver.
		 *
		 * The TX packets might be dropped for UDP case in the iperf
		 * testing. So need to be protected by follow control.
		 */
		need_orphan = 1;
#else
		if (dp_ctx->dp_cfg.tx_orphan_enable)
			need_orphan = 1;
#endif
	} else if (dp_ctx->dp_cfg.tx_orphan_enable) {
		if (qdf_nbuf_is_ipv4_tcp_pkt(nbuf) ||
		    qdf_nbuf_is_ipv6_tcp_pkt(nbuf))
			need_orphan = 1;
	}

	if (need_orphan) {
		qdf_nbuf_orphan(nbuf);
		cpu = qdf_get_smp_processor_id();
		++dp_intf->dp_stats.tx_rx_stats.per_cpu[cpu].tx_orphaned;
	} else {
		nbuf = __qdf_nbuf_unshare(nbuf);
	}

	return nbuf;
}

/**
 * dp_get_tx_resource() - check tx resources and take action
 * @dp_link: DP link handle
 * @mac_addr: mac address
 *
 * Return: none
 */
void dp_get_tx_resource(struct wlan_dp_link *dp_link,
			struct qdf_mac_addr *mac_addr);

#else
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 19, 0))
/**
 * dp_nbuf_orphan() - skb_unshare a cloned packed else skb_orphan
 * @dp_intf: pointer to DP interface
 * @nbuf: pointer to nbuf data packet
 *
 * Return: pointer to nbuf structure
 */
static inline
qdf_nbuf_t dp_nbuf_orphan(struct wlan_dp_intf *dp_intf,
			  qdf_nbuf_t nbuf)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	int cpu;

	dp_nbuf_fill_gso_size(dp_intf->dev, nbuf);

	if (unlikely(dp_ctx->dp_cfg.tx_orphan_enable) ||
	    qdf_nbuf_is_cloned(nbuf)) {
		/*
		 * For UDP packets we want to orphan the packet to allow the app
		 * to send more packets. The flow would ultimately be controlled
		 * by the limited number of tx descriptors for the vdev.
		 */
		cpu = qdf_get_smp_processor_id();
		++dp_intf->dp_stats.tx_rx_stats.per_cpu[cpu].tx_orphaned;
		qdf_nbuf_orphan(nbuf);
	}
	return nbuf;
}
#else
static inline
qdf_nbuf_t dp_nbuf_orphan(struct wlan_dp_intf *dp_intf,
			  qdf_nbuf_t nbuf)
{
	qdf_nbuf_t nskb;

	dp_nbuf_fill_gso_size(dp_intf->dev, nbuf);
	nskb =  __qdf_nbuf_unshare(nbuf);

	return nskb;
}
#endif

/**
 * dp_get_tx_resource() - check tx resources and take action
 * @dp_link: DP link handle
 * @mac_addr: mac address
 *
 * Return: none
 */
static inline
void dp_get_tx_resource(struct wlan_dp_link *dp_link,
			struct qdf_mac_addr *mac_addr)
{
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

/**
 * dp_start_xmit() - Transmit a frame
 * @dp_link: DP link handle
 * @nbuf: n/w buffer
 *
 * Function called to Transmit a n/w buffer in STA mode.
 *
 * Return: Status of the transmission
 */
QDF_STATUS
dp_start_xmit(struct wlan_dp_link *dp_link, qdf_nbuf_t nbuf);

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * dp_mon_rx_packet_cbk() - Receive callback registered with OL layer.
 * @context: pointer to qdf context
 * @rxbuf: pointer to rx qdf_nbuf
 *
 * TL will call this to notify the HDD when one or more packets were
 * received for a registered STA.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_mon_rx_packet_cbk(void *context, qdf_nbuf_t rxbuf);

/**
 * dp_monitor_set_rx_monitor_cb(): Set rx monitor mode callback function
 * @txrx: pointer to txrx ops
 * @rx_monitor_cb: pointer to callback function
 *
 * Returns: None
 */
void dp_monitor_set_rx_monitor_cb(struct ol_txrx_ops *txrx,
				  ol_txrx_rx_mon_fp rx_monitor_cb);
/**
 * dp_rx_monitor_callback(): Callback function for receive monitor mode
 * @vdev: Handle to vdev object
 * @mpdu: pointer to mpdu to be delivered to os
 * @rx_status: receive status
 *
 * Returns: None
 */
void dp_rx_monitor_callback(ol_osif_vdev_handle vdev,
			    qdf_nbuf_t mpdu,
			    void *rx_status);

#else
static inline
QDF_STATUS dp_mon_rx_packet_cbk(void *context, qdf_nbuf_t rxbuf)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_monitor_set_rx_monitor_cb(struct ol_txrx_ops *txrx,
				  ol_txrx_rx_mon_fp rx_monitor_cb) { }

static inline
void dp_rx_monitor_callback(ol_osif_vdev_handle vdev, qdf_nbuf_t mpdu,
			    void *rx_status) { }
#endif

/**
 * dp_sta_notify_tx_comp_cb() - notify tx comp callback registered with dp
 * @nbuf: pointer to nbuf
 * @ctx: osif context
 * @flag: tx status flag
 *
 * Return: None
 */
void dp_sta_notify_tx_comp_cb(qdf_nbuf_t nbuf, void *ctx, uint16_t flag);

/**
 * dp_softap_notify_tx_compl_cbk() - notify softap tx comp registered with dp
 * @nbuf: pointer to nbuf
 * @context: osif context
 * @flag: tx status flag
 *
 * Return: None
 */
void dp_softap_notify_tx_compl_cbk(qdf_nbuf_t nbuf,
				   void *context, uint16_t flag);

/**
 * dp_rx_pkt_tracepoints_enabled() - Get the state of rx pkt tracepoint
 *
 * Return: True if any rx pkt tracepoint is enabled else false
 */
static inline bool dp_rx_pkt_tracepoints_enabled(void)
{
	return (qdf_trace_dp_rx_tcp_pkt_enabled() ||
		qdf_trace_dp_rx_udp_pkt_enabled() ||
		qdf_trace_dp_rx_pkt_enabled());
}

#ifdef CONFIG_DP_PKT_ADD_TIMESTAMP
/**
 * wlan_dp_pkt_add_timestamp() - add timestamp in data payload
 * @dp_intf: DP interface
 * @index: timestamp index which decides offset in payload
 * @nbuf: Network socket buffer
 *
 * Return: none
 */
void wlan_dp_pkt_add_timestamp(struct wlan_dp_intf *dp_intf,
			       enum qdf_pkt_timestamp_index index,
			       qdf_nbuf_t nbuf);
#else
static inline
void wlan_dp_pkt_add_timestamp(struct wlan_dp_intf *dp_intf,
			  enum qdf_pkt_timestamp_index index,
			  qdf_nbuf_t nbuf)
{
}
#endif

#if defined(FEATURE_LRO)
/**
 * dp_lro_set_reset() - API for Disable/Enable LRO
 * @dp_intf: DP interface pointer
 * @enable_flag: enable or disable LRO.
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS dp_lro_set_reset(struct wlan_dp_intf *dp_intf, uint8_t enable_flag);
#else
static inline
QDF_STATUS dp_lro_set_reset(struct wlan_dp_intf *dp_intf,
			    uint8_t enable_flag)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* FEATURE_LRO */

#ifdef RECEIVE_OFFLOAD
/**
 * dp_rx_ol_init() - Initialize Rx offload mode (LRO or GRO)
 * @dp_ctx: pointer to DP Context
 * @is_wifi3_0_target: true if it wifi3.0 target
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS dp_rx_ol_init(struct wlan_dp_psoc_context *dp_ctx,
			 bool is_wifi3_0_target);
#else /* RECEIVE_OFFLOAD */

static inline QDF_STATUS
dp_rx_ol_init(struct wlan_dp_psoc_context *dp_ctx,
	      bool is_wifi3_0_target)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
static inline
void dp_rx_pkt_da_check(struct wlan_dp_intf *dp_intf, qdf_nbuf_t nbuf)
{
	/* only do DA check for RX frame from non-regular path */
	if (!qdf_nbuf_is_exc_frame(nbuf))
		return;

	if (qdf_mem_cmp(qdf_nbuf_data(nbuf), dp_intf->mac_addr.bytes,
			ETH_ALEN)) {
		dp_info("da mac:" QDF_MAC_ADDR_FMT "intf_mac:" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(qdf_nbuf_data(nbuf)),
			QDF_MAC_ADDR_REF(dp_intf->mac_addr.bytes));
		qdf_mem_copy(qdf_nbuf_data(nbuf), dp_intf->mac_addr.bytes,
			     ETH_ALEN);
	}
}
#else
static inline
void dp_rx_pkt_da_check(struct wlan_dp_intf *dp_intf, qdf_nbuf_t nbuf)
{
}
#endif

#endif
