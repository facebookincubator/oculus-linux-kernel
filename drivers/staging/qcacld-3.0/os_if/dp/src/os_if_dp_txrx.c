/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 *  DOC: osif_dp_txrx.c
 *  This file contains DP component's TX/RX osif API implementation
 */
#include "os_if_dp.h"
#include "os_if_dp_lro.h"
#include <wlan_dp_public_struct.h>
#include <wlan_objmgr_vdev_obj.h>
#include "osif_sync.h"
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/inetdevice.h>
#include <linux/wireless.h>
#include <linux/rtnetlink.h>
#include <net/cfg80211.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_misc.h>
#include <net/tcp.h>
#include <ol_defines.h>
#include <hif_napi.h>
#include <hif.h>
#include <wlan_hdd_main.h>
#include "wlan_hdd_wmm.h"

/**
 * osif_dp_classify_pkt() - classify packet
 * @skb:  sk buff
 *
 * Return: none
 */
void osif_dp_classify_pkt(struct sk_buff *skb)
{
	struct ethhdr *eh = (struct ethhdr *)skb->data;

	qdf_mem_zero(skb->cb, sizeof(skb->cb));

	/* check destination mac address is broadcast/multicast */
	if (is_broadcast_ether_addr((uint8_t *)eh))
		QDF_NBUF_CB_GET_IS_BCAST(skb) = true;
	else if (is_multicast_ether_addr((uint8_t *)eh))
		QDF_NBUF_CB_GET_IS_MCAST(skb) = true;

	if (qdf_nbuf_is_ipv4_arp_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_ARP;
	else if (qdf_nbuf_is_ipv4_dhcp_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_DHCP;
	else if (qdf_nbuf_is_ipv4_eapol_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_EAPOL;
	else if (qdf_nbuf_is_ipv4_wapi_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_WAPI;
	else if (qdf_nbuf_is_icmp_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_ICMP;
	else if (qdf_nbuf_is_icmpv6_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_ICMPv6;
}

/**
 * osif_dp_mark_critical_pkt() - Identify and mark critical packets
 * @skb: skb ptr
 *
 * Return: None
 */
static void osif_dp_mark_critical_pkt(struct sk_buff *skb)
{
	if (qdf_nbuf_is_ipv4_eapol_pkt(skb)) {
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
				QDF_NBUF_CB_PACKET_TYPE_EAPOL;
	} else if (qdf_nbuf_is_ipv4_arp_pkt(skb)) {
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
				QDF_NBUF_CB_PACKET_TYPE_ARP;
	} else if (qdf_nbuf_is_ipv4_dhcp_pkt(skb)) {
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
				QDF_NBUF_CB_PACKET_TYPE_DHCP;
	} else if (qdf_nbuf_is_ipv6_dhcp_pkt(skb)) {
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
				QDF_NBUF_CB_PACKET_TYPE_DHCPV6;
	} else if (qdf_nbuf_is_icmpv6_pkt(skb)) {
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_ICMPv6;
	}

	QDF_NBUF_CB_TX_EXTRA_IS_CRITICAL(skb) = true;
}

#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
/**
 * osif_dp_mark_pkt_type_by_priority() - mark packet type to skb->cb
 *                                       by type from priority of skb
 * @skb: network buffer
 *
 * Return: true - packet type marked, false - not marked
 */
static inline
bool osif_dp_mark_pkt_type_by_priority(struct sk_buff *skb)
{
	bool type_marked = false;
	uint32_t pkt_type =
		qdf_nbuf_get_priority_pkt_type(skb);

	if (qdf_unlikely(pkt_type == QDF_NBUF_PRIORITY_PKT_TCP_ACK)) {
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
					QDF_NBUF_CB_PACKET_TYPE_TCP_ACK;
		type_marked = true;
	}
	/* cleanup the packet type in priority */
	qdf_nbuf_remove_priority_pkt_type(skb);

	return type_marked;
}
#else
static inline
bool osif_dp_mark_pkt_type_by_priority(struct sk_buff *skb)
{
	return false;
}
#endif

/**
 * osif_dp_mark_non_critical_pkt() - Identify and mark non-critical packets
 * @skb: skb ptr
 *
 * Return: None
 */
static void osif_dp_mark_non_critical_pkt(struct sk_buff *skb)
{
	/* check if packet type is marked from skb->priority already */
	if (osif_dp_mark_pkt_type_by_priority(skb))
		return;

	if (qdf_nbuf_is_icmp_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
				QDF_NBUF_CB_PACKET_TYPE_ICMP;
	else if (qdf_nbuf_is_ipv4_wapi_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_WAPI;
}

void osif_dp_mark_pkt_type(struct sk_buff *skb)
{
	struct ethhdr *eh = (struct ethhdr *)skb->data;

	/*
	 * Zero out CB before accessing it. Expectation is that cb is accessed
	 * for the first time here on TX path in hard_start_xmit.
	 */
	qdf_mem_zero(skb->cb, sizeof(skb->cb));

	/* check destination mac address is broadcast/multicast */
	if (is_broadcast_ether_addr((uint8_t *)eh))
		QDF_NBUF_CB_GET_IS_BCAST(skb) = true;
	else if (is_multicast_ether_addr((uint8_t *)eh))
		QDF_NBUF_CB_GET_IS_MCAST(skb) = true;

	/*
	 * TX Packets in the HI_PRIO queue are assumed to be critical and
	 * marked accordingly.
	 */
	if (skb->queue_mapping == TX_GET_QUEUE_IDX(HDD_LINUX_AC_HI_PRIO, 0))
		osif_dp_mark_critical_pkt(skb);
	else
		osif_dp_mark_non_critical_pkt(skb);
}

/*
 * When bus bandwidth is idle, if RX data is delivered with
 * napi_gro_receive, to reduce RX delay related with GRO,
 * check gro_result returned from napi_gro_receive to determine
 * is extra GRO flush still necessary.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
#define DP_IS_EXTRA_GRO_FLUSH_NECESSARY(_gro_ret) true
#define GRO_DROP_UPDATE_STATUS(gro_ret, status)
#else
#define GRO_DROP_UPDATE_STATUS(gro_ret, status) \
	if ((gro_ret) == GRO_DROP) ((status) = QDF_STATUS_E_GRO_DROP)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define DP_IS_EXTRA_GRO_FLUSH_NECESSARY(_gro_ret) \
	((_gro_ret) != GRO_DROP)
#else
#define DP_IS_EXTRA_GRO_FLUSH_NECESSARY(_gro_ret) \
	((_gro_ret) != GRO_DROP && (_gro_ret) != GRO_NORMAL)
#endif
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
/**
 * osif_dp_rx_thread_napi_gro_flush() - do gro flush
 * @napi: napi used to do gro flush
 * @flush_code: flush_code differentiating low_tput_flush and normal_flush
 *
 * if there is RX GRO_NORMAL packets pending in napi
 * rx_list, flush them manually right after napi_gro_flush.
 *
 * Return: none
 */
static inline
void osif_dp_rx_thread_napi_gro_flush(struct napi_struct *napi,
				      enum dp_rx_gro_flush_code flush_code)
{
	if (napi->poll) {
		/* Skipping GRO flush in low TPUT */
		if (flush_code != DP_RX_GRO_LOW_TPUT_FLUSH)
			napi_gro_flush(napi, false);

		if (napi->rx_count) {
			netif_receive_skb_list(&napi->rx_list);
			qdf_init_list_head(&napi->rx_list);
			napi->rx_count = 0;
		}
	}
}
#else
static inline
void osif_dp_rx_thread_napi_gro_flush(struct napi_struct *napi,
				      enum dp_rx_gro_flush_code flush_code)
{
	if (napi->poll) {
		/* Skipping GRO flush in low TPUT */
		if (flush_code != DP_RX_GRO_LOW_TPUT_FLUSH)
			napi_gro_flush(napi, false);
	}
}
#endif

/**
 * osif_dp_rx_napi_gro_flush() - GRO RX/flush function.
 * @napi_to_use: napi to be used to give packets to the stack, gro flush
 * @nbuf: pointer to n/w buff
 * @low_tput_force_flush: Is force flush required in low tput
 *
 * Function calls napi_gro_receive for the skb. If the skb indicates that a
 * flush needs to be done (set by the lower DP layer), the function also calls
 * napi_gro_flush. Local softirqs are disabled (and later enabled) while making
 * napi_gro__ calls.
 *
 * Return: QDF_STATUS_SUCCESS if not dropped by napi_gro_receive or
 *	   QDF error code.
 */

static QDF_STATUS
osif_dp_rx_napi_gro_flush(qdf_napi_struct *napi_to_use,
			  qdf_nbuf_t nbuf,
			  uint8_t *low_tput_force_flush)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	gro_result_t gro_ret;

	skb_set_hash(nbuf, QDF_NBUF_CB_RX_FLOW_ID(nbuf), PKT_HASH_TYPE_L4);

	local_bh_disable();
	gro_ret = napi_gro_receive((struct napi_struct *)napi_to_use, nbuf);

	if (DP_IS_EXTRA_GRO_FLUSH_NECESSARY(gro_ret)) {
		*low_tput_force_flush = 1;
		osif_dp_rx_thread_napi_gro_flush((struct napi_struct *)napi_to_use,
						 DP_RX_GRO_NORMAL_FLUSH);
	}

	local_bh_enable();
	GRO_DROP_UPDATE_STATUS(gro_ret, status);

	return status;
}

/**
 * osif_dp_rx_napi_gro_receive() - GRO RX receive function.
 * @napi_to_use: napi to be used to give packets to the stack
 * @nbuf: pointer to n/w buff
 *
 * Function calls napi_gro_receive for the skb.
 * napi_gro_flush. Local softirqs are disabled (and later enabled) while making
 * napi_gro__ calls.
 *
 * Return: QDF_STATUS_SUCCESS if not dropped by napi_gro_receive or
 *	   QDF error code.
 */
static QDF_STATUS
osif_dp_rx_napi_gro_receive(qdf_napi_struct *napi_to_use,
			    qdf_nbuf_t nbuf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	gro_result_t gro_ret;

	skb_set_hash(nbuf, QDF_NBUF_CB_RX_FLOW_ID(nbuf), PKT_HASH_TYPE_L4);

	local_bh_disable();
	gro_ret = napi_gro_receive((struct napi_struct *)napi_to_use, nbuf);

	local_bh_enable();
	GRO_DROP_UPDATE_STATUS(gro_ret, status);

	return status;
}

#ifdef RECEIVE_OFFLOAD
/**
 * osif_dp_rxthread_napi_normal_gro_flush() - GRO flush cbk for NAPI+Rx_Thread
 * Rx mode
 * @data: hif NAPI context
 *
 * Return: none
 */
static void osif_dp_rxthread_napi_normal_gro_flush(void *data)
{
	struct qca_napi_info *qca_napi = (struct qca_napi_info *)data;

	local_bh_disable();
	/*
	 * As we are breaking context in Rxthread mode, there is rx_thread NAPI
	 * corresponds each hif_napi.
	 */
	osif_dp_rx_thread_napi_gro_flush(&qca_napi->rx_thread_napi,
					 DP_RX_GRO_NORMAL_FLUSH);
	local_bh_enable();
}

/**
 * osif_dp_hif_napi_gro_flush() - GRO flush callback for NAPI Rx mode
 * @data: hif NAPI context
 *
 * Return: none
 */
static void osif_dp_hif_napi_gro_flush(void *data)
{
	struct qca_napi_info *qca_napi = (struct qca_napi_info *)data;

	local_bh_disable();
	napi_gro_flush(&qca_napi->napi, false);
	local_bh_enable();
}
#endif

#ifdef FEATURE_LRO
/**
 * osif_dp_qdf_lro_flush() - LRO flush wrapper
 * @data: hif NAPI context
 *
 * Return: none
 */
static void osif_dp_qdf_lro_flush(void *data)
{
	struct qca_napi_info *qca_napii = (struct qca_napi_info *)data;
	qdf_lro_ctx_t qdf_lro_ctx = qca_napii->lro_ctx;

	qdf_lro_flush(qdf_lro_ctx);
}
#elif defined(RECEIVE_OFFLOAD)
static void osif_dp_qdf_lro_flush(void *data)
{
}
#endif

#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static enum qdisc_filter_status
__osif_check_for_prio_filter_in_clsact_qdisc(struct tcf_block *block,
					     uint32_t prio)
{
	struct tcf_chain *chain;
	struct tcf_proto *tp;
	struct tcf_proto *tp_next;
	enum qdisc_filter_status ret = QDISC_FILTER_PRIO_MISMATCH;

	mutex_lock(&block->lock);
	list_for_each_entry(chain, &block->chain_list, list) {
		mutex_lock(&chain->filter_chain_lock);
		tp = tcf_chain_dereference(chain->filter_chain, chain);
		while (tp) {
			tp_next = rtnl_dereference(tp->next);
			if (tp->prio == (prio << 16)) {
				ret = QDISC_FILTER_PRIO_MATCH;
				break;
			}
			tp = tp_next;
		}
		mutex_unlock(&chain->filter_chain_lock);

		if (ret == QDISC_FILTER_PRIO_MATCH)
			break;
	}
	mutex_unlock(&block->lock);

	return ret;
}
#else
static enum qdisc_filter_status
__osif_check_for_prio_filter_in_clsact_qdisc(struct tcf_block *block,
					     uint32_t prio)
{
	struct tcf_chain *chain;
	struct tcf_proto *tp;
	enum qdisc_filter_status ret = QDISC_FILTER_PRIO_MISMATCH;

	if (!rtnl_trylock())
		return QDISC_FILTER_RTNL_LOCK_FAIL;

	list_for_each_entry(chain, &block->chain_list, list) {
		for (tp = rtnl_dereference(chain->filter_chain); tp;
		     tp = rtnl_dereference(tp->next)) {
			if (tp->prio == (prio << 16))
				ret = QDISC_FILTER_PRIO_MATCH;
		}
	}
	rtnl_unlock();

	return ret;
}
#endif

/**
 * osif_check_for_prio_filter_in_clsact_qdisc() - Check if priority 3 filter
 *  is configured in the ingress clsact qdisc
 * @qdisc: pointer to clsact qdisc
 * @prio: traffic priority
 *
 * Return: qdisc filter status
 */
static enum qdisc_filter_status
osif_check_for_prio_filter_in_clsact_qdisc(struct Qdisc *qdisc, uint32_t prio)
{
	const struct Qdisc_class_ops *cops;
	struct tcf_block *ingress_block;

	cops = qdisc->ops->cl_ops;
	if (qdf_unlikely(!cops || !cops->tcf_block))
		return QDISC_FILTER_PRIO_MISMATCH;

	ingress_block = cops->tcf_block(qdisc, TC_H_MIN_INGRESS, NULL);
	if (qdf_unlikely(!ingress_block))
		return QDISC_FILTER_PRIO_MISMATCH;

	return __osif_check_for_prio_filter_in_clsact_qdisc(ingress_block,
							    prio);
}

/**
 * osif_dp_rx_check_qdisc_configured() - Check if any ingress qdisc
 * configured for given netdev
 * @ndev: pointer to netdev
 * @prio: traffic priority
 *
 * The function checks if ingress qdisc is registered for a given
 * net device.
 *
 * Return: None
 */
static QDF_STATUS
osif_dp_rx_check_qdisc_configured(qdf_netdev_t ndev, uint32_t prio)
{
	struct netdev_queue *ingress_q;
	struct Qdisc *ingress_qdisc;
	struct net_device *dev = (struct net_device *)ndev;
	bool disable_gro = false;
	enum qdisc_filter_status status;

	if (!dev->ingress_queue)
		goto reset_wl;

	if (!rtnl_trylock())
		return QDF_STATUS_E_AGAIN;

	ingress_q = rtnl_dereference(dev->ingress_queue);
	if (qdf_unlikely(!ingress_q))
		goto reset;

	ingress_qdisc = rtnl_dereference(ingress_q->qdisc);
	if (qdf_unlikely(!ingress_qdisc))
		goto reset;

	if (qdf_str_eq(ingress_qdisc->ops->id, "ingress")) {
		disable_gro = true;
	} else if (qdf_str_eq(ingress_qdisc->ops->id, "clsact")) {
		status = osif_check_for_prio_filter_in_clsact_qdisc(
								  ingress_qdisc,
								  prio);

		if (status == QDISC_FILTER_PRIO_MISMATCH)
			goto reset;

		disable_gro = true;
	}

	if (disable_gro) {
		rtnl_unlock();
		return QDF_STATUS_SUCCESS;
	}

reset:
	rtnl_unlock();

reset_wl:
	return QDF_STATUS_E_NOSUPPORT;
}

#else
static QDF_STATUS
osif_dp_rx_check_qdisc_configured(qdf_netdev_t ndev, uint32_t prio)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0))
static void
osif_dp_register_arp_unsolicited_cbk(struct wlan_dp_psoc_callbacks *cb_obj)
{
	cb_obj->dp_is_gratuitous_arp_unsolicited_na = NULL;
}
#else
static bool osif_dp_is_gratuitous_arp_unsolicited_na(qdf_nbuf_t nbuf)
{
	return cfg80211_is_gratuitous_arp_unsolicited_na((struct sk_buff *)nbuf);
}

static void
osif_dp_register_arp_unsolicited_cbk(struct wlan_dp_psoc_callbacks *cb_obj)
{
	cb_obj->dp_is_gratuitous_arp_unsolicited_na =
		osif_dp_is_gratuitous_arp_unsolicited_na;
}
#endif

#if defined(CFG80211_CTRL_FRAME_SRC_ADDR_TA_ADDR)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
static
bool osif_dp_cfg80211_rx_control_port(qdf_netdev_t dev, u8 *ta_addr,
				      qdf_nbuf_t nbuf, bool unencrypted)
{
	return cfg80211_rx_control_port((struct net_device *)dev,
					(struct sk_buff *)nbuf,
					unencrypted, -1);
}

#else
static
bool osif_dp_cfg80211_rx_control_port(qdf_netdev_t dev, u8 *ta_addr,
				      qdf_nbuf_t nbuf, bool unencrypted)
{
	return cfg80211_rx_control_port((struct net_device *)dev,
					ta_addr, (struct sk_buff *)nbuf,
					unencrypted);
}
#endif

static void
osif_dp_register_send_rx_pkt_over_nl(struct wlan_dp_psoc_callbacks *cb_obj)
{
	cb_obj->dp_send_rx_pkt_over_nl = osif_dp_cfg80211_rx_control_port;
}

#else
static void
osif_dp_register_send_rx_pkt_over_nl(struct wlan_dp_psoc_callbacks *cb_obj)
{
	cb_obj->dp_send_rx_pkt_over_nl = NULL;
}
#endif

#ifdef RECEIVE_OFFLOAD
static
void osif_dp_register_rx_offld_flush_cb(enum dp_rx_offld_flush_cb cb_type)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (cb_type == DP_RX_FLUSH_LRO)
		cdp_register_rx_offld_flush_cb(soc, osif_dp_qdf_lro_flush);
	else if (cb_type == DP_RX_FLUSH_THREAD)
		cdp_register_rx_offld_flush_cb(soc,
					       osif_dp_rxthread_napi_normal_gro_flush);
	else if (cb_type == DP_RX_FLUSH_NAPI)
		cdp_register_rx_offld_flush_cb(soc,
					       osif_dp_hif_napi_gro_flush);
}
#else

static
void osif_dp_register_rx_offld_flush_cb(enum dp_rx_offld_flush_cb cb_type) { }
#endif

static
QDF_STATUS osif_dp_rx_pkt_to_nw(qdf_nbuf_t nbuf, enum dp_nbuf_push_type type)
{
	int netif_status;

	if (type == DP_NBUF_PUSH_BH_DISABLE) {
		local_bh_disable();
		netif_status = netif_receive_skb(nbuf);
		local_bh_enable();
	} else if (type == DP_NBUF_PUSH_NI) {
		netif_status = netif_rx_ni(nbuf);
	} else if (type == DP_NBUF_PUSH_NAPI) {
		netif_status = netif_receive_skb(nbuf);
	} else {
		netif_status = netif_rx(nbuf);
	}

	if (qdf_likely(netif_status == NET_RX_SUCCESS))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

void os_if_dp_register_txrx_callbacks(struct wlan_dp_psoc_callbacks *cb_obj)
{
	cb_obj->dp_nbuf_push_pkt = osif_dp_rx_pkt_to_nw;
	cb_obj->dp_rx_napi_gro_flush = osif_dp_rx_napi_gro_flush;
	cb_obj->dp_rx_napi_gro_receive = osif_dp_rx_napi_gro_receive;
	cb_obj->dp_rx_thread_napi_gro_flush = osif_dp_rx_thread_napi_gro_flush;
	cb_obj->dp_lro_rx_cb = osif_dp_lro_rx;
	cb_obj->dp_register_rx_offld_flush_cb =
		osif_dp_register_rx_offld_flush_cb;
	cb_obj->dp_rx_check_qdisc_configured =
		osif_dp_rx_check_qdisc_configured;

	osif_dp_register_arp_unsolicited_cbk(cb_obj);

	osif_dp_register_send_rx_pkt_over_nl(cb_obj);
}
