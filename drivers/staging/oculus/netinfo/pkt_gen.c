// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/inet.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_arp.h>
#include <linux/netfilter_bridge.h>
#include <linux/printk.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <net/ip.h>
#include <net/netevent.h>
#include <net/tcp.h>
#include "type_header.h"
#include "stats_header.h"

static u16 ip_id = 0, icmp_seq = 0;
static atomic_t softap_interface_up = ATOMIC_INIT(0);
static atomic_t dhcp_started = ATOMIC_INIT(0);

int send_icmp_skb(MSG_BUF_t *msg)
{
	struct ethhdr *eth;
	struct iphdr *iph;
	struct icmphdr *icmph;
	u8 ip_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
	int ret = 0;
	struct sk_buff *skb = NULL;

	if (msg == NULL || msg->dev == NULL) {
		netinfo_pr_err("Failed due to msg %p or dev null\n", msg);
		return -ENOMEM;
	}
	// only allocate 512 bytes since there is no payload need to add
	skb = __alloc_skb(512, GFP_ATOMIC, SKB_ALLOC_RX, NUMA_NO_NODE);
	if (!skb) {
		netinfo_pr_err("%s Failed to allocate skb\n", NETINFO_TAG);
		return -ENOMEM;
	}
	skb_reserve(skb, ip_len + sizeof(struct ethhdr));
	skb_put(skb, ip_len);

	//prepare icmp header
	skb_push(skb, sizeof(struct icmphdr));
	icmph = (struct icmphdr *)skb->data;
	icmph->type = ICMP_ECHO;
	icmph->code = 0;
	icmph->checksum = 0;
	icmph->un.echo.id = ICMP_ID_TAG;
	icmph->un.echo.sequence = htons(icmp_seq++);
	icmph->checksum =
		ip_compute_csum((unsigned char *)icmph, sizeof(struct icmphdr));

	//prepare ip header
	skb_push(skb, sizeof(*iph));
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);
	put_unaligned(0x45, (unsigned char *)iph);
	iph->tos = 0;
	put_unaligned(htons(ip_len), &(iph->tot_len));
	iph->id = htons(ip_id++);
	iph->frag_off = 0;
	iph->ttl = 64;
	iph->protocol = IPPROTO_ICMP;
	iph->check = 0;
	put_unaligned(msg->addr.dst_ip, &(iph->saddr));
	put_unaligned(msg->addr.src_ip, &(iph->daddr));
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

	//prepare ethernet header
	eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	skb->protocol = eth->h_proto = htons(ETH_P_IP);
	memcpy(eth->h_dest, msg->src_macaddr, ETH_ALEN);
	memcpy(eth->h_source, msg->addr.dst_macaddr, ETH_ALEN);

	skb->dev = msg->dev;
	skb->protocol = htons(ETH_P_IP);
	skb->pkt_type = PACKET_OUTGOING;

	netinfo_pr_info("Sent Request seq %d src %x to dst %x msg %p\n",
			icmp_seq, iph->saddr, iph->daddr, msg);

	ret = dev_queue_xmit(skb);

	return ret;
}

int send_arp_reply(struct net_device *dev, u8 *target_mac, __be32 target_ip)
{
	struct sk_buff *skb;
	struct arphdr *arp;
	struct ethhdr *eth;
	unsigned char *arp_ptr;
	int arp_hdr_len =
		sizeof(struct arphdr) + 2 * dev->addr_len + 2 * sizeof(__be32);
	skb = alloc_skb(LL_RESERVED_SPACE(dev) + arp_hdr_len, GFP_ATOMIC);
	if (!skb) {
		printk(KERN_ERR "Failed to allocate skb\n");
		return -ENOMEM;
	}
	skb_reserve(skb, LL_RESERVED_SPACE(dev));
	skb->dev = dev;
	skb->protocol = htons(ETH_P_ARP);
	skb->pkt_type = PACKET_OUTGOING;
	eth = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
	memcpy(eth->h_dest, target_mac, ETH_ALEN); // Destination MAC
	memcpy(eth->h_source, dev->dev_addr, ETH_ALEN); // Source MAC
	eth->h_proto = htons(ETH_P_ARP);
	arp = (struct arphdr *)skb_put(skb, arp_hdr_len);
	arp->ar_hrd = htons(ARPHRD_ETHER);
	arp->ar_pro = htons(ETH_P_IP);
	arp->ar_hln = dev->addr_len;
	arp->ar_pln = 4;
	arp->ar_op = htons(ARPOP_REPLY);
	arp_ptr = (unsigned char *)(arp + 1);
	memcpy(arp_ptr, dev->dev_addr, dev->addr_len); // Sender MAC
	*arp_ptr = 0;
	arp_ptr += dev->addr_len;
	memcpy(arp_ptr, &target_ip, sizeof(__be32)); // Sender IP
	arp_ptr += sizeof(__be32);
	memcpy(arp_ptr, target_mac, dev->addr_len); // Target MAC
	arp_ptr += dev->addr_len;
	memcpy(arp_ptr, &target_ip, sizeof(__be32)); // Target IP
	dev_queue_xmit(skb);
	return 0;
}

// check if garp need to be sent(return 1) or not (return 0)
static int check_garp_conditions(ADDR_INFO_t taddr)
{
	netinfo_pr_info(
		"GARP conditions: dhcpstart %x ip %x enable %x debug %x\n",
		atomic_read(&dhcp_started), taddr.src_ip,
		netinfo_garp_feature_enabled(),
		netinfo_garp_debug_feature_enabled());
	if ((taddr.src_ip & htonl(SOFTAP_NETWORK_MASK)) !=
	    htonl(SOFTAP_NETWORK_RANGE))
		return 0;
	if (!atomic_read(&dhcp_started))
		return 1;
	return 0;
}

static rx_handler_result_t pre_tcp_rx_handler(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct ethhdr *eth;
	int ret = RX_HANDLER_PASS;

	if (!skb || skb->len < sizeof(struct ethhdr))
		return ret;
	eth = eth_hdr(skb);

	netinfo_pr_info("Packet received on interface: %s length %d type %x\n",
			skb->dev->name, skb->len, ntohs(eth->h_proto));

	if (eth->h_proto == htons(ETH_P_PAE)) {
		if (update_eapol_message_counter(skb, NULL) >= 0) {
			/* handle eapol-key messages and exit */
			return ret;
		}
	}

	if (!netinfo_garp_feature_enabled())
		return ret;

	if (ntohs(eth->h_proto) == 0x0800 && skb->len >= MIN_DHCP_TOTAL_LEN) {
		struct iphdr *iph;
		struct udphdr *udph;
		struct dhcphdr *dhcph;

		iph = ip_hdr(skb);
		if (iph->protocol != IPPROTO_UDP)
			return ret;
		udph = (struct udphdr *)((unsigned char *)iph + iph->ihl * 4);

		netinfo_pr_info("%s %d: UDP source %x dest %x\n", __func__,
				__LINE__, ntohs(udph->source),
				ntohs(udph->dest));
		if (ntohs(udph->source) != DHCP_CLIENT_PORT ||
		    ntohs(udph->dest) != DHCP_SERVER_PORT)
			return ret;

		dhcph = (struct dhcphdr *)((unsigned char *)udph +
					   sizeof(struct udphdr));
		netinfo_pr_info(
			"%s %d: DHCP Option0 %x Option2 %x TransId %x\n",
			__func__, __LINE__, dhcph->options[0],
			dhcph->options[2], dhcph->xid);

		if (dhcph->options[0] == DHCP_OPTION_MESSAGE_TYPE) {
			if (dhcph->options[2] == DHCPDISCOVER ||
			    dhcph->options[2] == DHCPREQUEST ||
			    dhcph->options[2] == DHCPDECLINE) {
				atomic_set(&dhcp_started, 1);
			}
		}
	}
	if (skb->protocol == htons(ETH_P_ARP)) {
		struct arphdr *arp_header = arp_hdr(skb);
		if (arp_header->ar_op == htons(ARPOP_REQUEST)) {
			u8 *arp_ptr = NULL;
			u8 *src_mac = NULL;
			ADDR_INFO_t taddr;

			arp_ptr = (u8 *)(arp_header + 1);
			memcpy(taddr.dst_macaddr, arp_ptr, ETH_ALEN);
			arp_ptr += ETH_ALEN;
			memcpy(&taddr.dst_ip, arp_ptr, 4);
			arp_ptr += IP_ADDR_LEN;
			src_mac = arp_ptr;
			arp_ptr += ETH_ALEN;
			memcpy(&taddr.src_ip, arp_ptr, 4);
			if (taddr.dst_ip == taddr.src_ip) {
				// it's ARP announcement for last IP
				if (check_garp_conditions(taddr)) {
					send_session_send_garp_msg(taddr,
								   skb->dev);
					netinfo_pr_info(
						"Received ARP announcement packet from " PRINT_MAC_ADDR_FMT
						" to %x\n",
						PRINT_MAC_ADDR_REF(
							taddr.dst_macaddr),
						taddr.dst_ip);
				}
			} else {
				netinfo_pr_info(
					"Received ARP REQ packet from " PRINT_MAC_ADDR_FMT
					" %x to %x\n",
					PRINT_MAC_ADDR_REF(taddr.dst_macaddr),
					taddr.dst_ip, taddr.src_ip);
			}
		}
	}

	return ret;
}

int register_pretcp_rx_handler(struct net_device *dev)
{
	if (!dev) {
		netinfo_pr_info("Failed to get network device\n");
		return -ENODEV;
	}
	if (!dev->ieee80211_ptr ||
	    dev->ieee80211_ptr->iftype != NL80211_IFTYPE_AP) {
		return -EINVAL;
	}

	if (!atomic_read(&softap_interface_up)) {
		atomic_set(&softap_interface_up, 1);
		atomic_set(&dhcp_started, 0);
		netdev_rx_handler_register(dev, pre_tcp_rx_handler, NULL);
		register_dhcp_nf_callback();
		enable_dev_queue_xmit_kp();
		netinfo_pr_info("Register_pretcp_rx_handler\n");
	}
	return 0;
}

int unregister_pretcp_rx_handler(struct net_device *dev)
{
	if (!dev) {
		netinfo_pr_info("Failed to get network device\n");
		return -ENODEV;
	}
	if (!dev->ieee80211_ptr ||
	    dev->ieee80211_ptr->iftype != NL80211_IFTYPE_AP)
		return -EINVAL;

	if (dev && atomic_read(&softap_interface_up)) {
		atomic_set(&softap_interface_up, 0);
		disable_dev_queue_xmit_kp();
		unregister_dhcp_nf_callback();
		netdev_rx_handler_unregister(dev);
		netinfo_pr_info("Unregister_pretcp_rx_handler\n");
	}
	return 0;
}

void netdev_state_event_handler(struct notifier_block *nb, unsigned long state,
				void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	if (!dev) {
		return;
	}
	switch (state) {
	case NETDEV_UP:
		netinfo_pr_info("Network device up: %s handler %p\n", dev->name,
				dev->rx_handler);
		register_pretcp_rx_handler(dev);
		reset_stats_counters();
		break;
	case NETDEV_DOWN:
		netinfo_pr_info("Network device down: %s handler %p\n",
				dev->name, dev->rx_handler);
		unregister_pretcp_rx_handler(dev);
		netinfo_disable_all_feature();
		break;
	default:
		break;
	}
}

int dhcp_flag_get(void)
{
	return (atomic_read(&dhcp_started) == 1) ? 1 : 0;
}

void dhcp_flag_start(void)
{
	atomic_set(&dhcp_started, 1);
}
