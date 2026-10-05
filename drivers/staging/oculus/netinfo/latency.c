// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/inet.h>
#include <linux/kernel.h>
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
#include <net/ip.h>
#include <net/netevent.h>
#include <net/tcp.h>
#include "type_header.h"
#include "stats_header.h"

static u32 ntcp_seq = 0;
static struct sk_buff *ntcp_rst_skb = NULL;
static atomic_t usb0_carrier_up = ATOMIC_INIT(0);

// it's for capturing icmp reply
static u32 nf_icmp_callback(void *priv, struct sk_buff *skb,
			    const struct nf_hook_state *state)
{
	struct iphdr *ip_header;
	struct icmphdr *icmp_header;
	struct ethhdr *eth;

	if (!skb) {
		return NF_ACCEPT;
	}

	if (skb->protocol == htons(ETH_P_IP)) {
		ip_header = ip_hdr(skb);
		if (ip_header->protocol == IPPROTO_ICMP) {
			icmp_header = icmp_hdr(skb);
			if (icmp_header->type == ICMP_ECHOREPLY &&
			    icmp_header->un.echo.id == ICMP_ID_TAG) {
				netinfo_pr_debug(
					"Received Reply id %d seq %d\n",
					icmp_header->un.echo.id,
					icmp_header->un.echo.sequence);
				eth = (struct ethhdr *)skb_mac_header(skb);
				send_session_latency_msg(&eth->h_source[0],
							 jiffies);
				return NF_DROP;
			}
		}
	}
	return NF_ACCEPT;
}

// it's for capturing arp receives from phone or home ap
static u32 nf_arp_in_callback(void *priv, struct sk_buff *skb,
			      const struct nf_hook_state *state)
{
	struct arphdr *arp_header;
	u8 *src_mac = NULL;

	if (!skb) {
		return NF_ACCEPT;
	}

	if (skb->protocol == htons(ETH_P_ARP)) {
		arp_header = arp_hdr(skb);
		netinfo_pr_debug("skb %p ar_op %x %x\n", skb->data,
				 arp_header->ar_op, htons(ARPOP_REPLY));
		if (arp_header->ar_op == htons(ARPOP_REPLY)) {
			u8 *arp_ptr = NULL;
			ADDR_INFO_t taddr;

			arp_ptr = (u8 *)(arp_header + 1);
			src_mac = arp_ptr;
			arp_ptr += ETH_ALEN;
			memcpy(&taddr.src_ip, arp_ptr, 4);
			arp_ptr += IP_ADDR_LEN;
			memcpy(taddr.dst_macaddr, arp_ptr, ETH_ALEN);
			arp_ptr += ETH_ALEN;
			memcpy(&taddr.dst_ip, arp_ptr, 4);
			if (taddr.dst_ip != taddr.src_ip) {
				netinfo_pr_info(
					"Received ARP packet from %x to %x\n",
					taddr.src_ip, taddr.dst_ip);
				send_session_update_msg(src_mac, taddr,
							skb->dev);
			} else {
				// if dhcp is not started, parse garp to see if
				// phone want to reuse previous ip
				if (!dhcp_flag_get()) {
					update_garp_stats(taddr.dst_ip);
				}
			}
		}
	}

	return NF_ACCEPT;
}

// it's for capturing arp receives from SN
static u32 nf_arp_out_callback(void *priv, struct sk_buff *skb,
			       const struct nf_hook_state *state)
{
	struct arphdr *arp_header;
	u8 *src_mac = NULL;

	if (!skb) {
		return NF_ACCEPT;
	}

	if (skb->protocol == htons(ETH_P_ARP)) {
		arp_header = arp_hdr(skb);
		netinfo_pr_debug("skb %p ar_op %x %x\n", skb->data,
				 arp_header->ar_op, htons(ARPOP_REPLY));
		if (arp_header->ar_op == htons(ARPOP_REPLY)) {
			u8 *arp_ptr = NULL;
			ADDR_INFO_t taddr;

			arp_ptr = (u8 *)(arp_header + 1);
			memcpy(taddr.dst_macaddr, arp_ptr, ETH_ALEN);
			arp_ptr += ETH_ALEN;
			memcpy(&taddr.dst_ip, arp_ptr, 4);
			arp_ptr += IP_ADDR_LEN;
			src_mac = arp_ptr;
			arp_ptr += ETH_ALEN;
			memcpy(&taddr.src_ip, arp_ptr, 4);
			if (taddr.dst_ip != taddr.src_ip) {
				netinfo_pr_info(
					"Received ARP packet from %x to %x\n",
					taddr.src_ip, taddr.dst_ip);
				send_session_update_msg(src_mac, taddr,
							skb->dev);
			}
		}
	}

	return NF_ACCEPT;
}

static const u16 ntcp_dest_port = 23070, datax_dest_port = 20203;
// it's for capturing tcp sync receives from SN
static u32 nf_tcp_sync_ack_out_callback(void *priv, struct sk_buff *skb,
					const struct nf_hook_state *state)
{
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph;
	// Check if packet is IPv4 and TCP
	if (iph->protocol != IPPROTO_TCP) {
		return NF_ACCEPT;
	}
	tcph = tcp_hdr(skb);
	if (tcph) {
		// Check if SYN flag is set
		if ((tcp_flag_word(tcph) & TCP_FLAG_SYN)) {
			if (tcph->dest == htons(ntcp_dest_port)) {
				netinfo_pr_info(
					"Received SYN packet IP from %d to %d\n",
					htons(tcph->source), htons(tcph->dest));
				if (atomic_read(&usb0_carrier_up)) {
					// duplicate it when it's retail demo.
					ntcp_rst_skb =
						skb_copy(skb, GFP_ATOMIC);
					if (!ntcp_rst_skb) {
						netinfo_pr_err(
							"Failed to create a copy of the skb\n");
						return -ENOMEM;
					}
				}
			}
			if (tcph->dest == htons(datax_dest_port)) {
				send_session_cancel_rx_handler_msg(skb->dev);
			}
		}
	}
	return NF_ACCEPT;
}

// it's for capturing egress tcp ack_seq from SN
static u32 nf_tcp_egress_seq_callback(void *priv, struct sk_buff *skb,
				      const struct nf_hook_state *state)
{
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph;
	// Check if packet is IPv4 and TCP at retail demo case.
	if (atomic_read(&usb0_carrier_up) && (iph->protocol != IPPROTO_TCP)) {
		return NF_ACCEPT;
	}
	tcph = tcp_hdr(skb);
	if (tcph) {
		if (tcph->source == htons(ntcp_dest_port)) {
			ntcp_seq = get_unaligned(&tcph->ack_seq);
			netinfo_pr_debug(
				"Update TCP packet seq %x ack_seq %x source %d destp %d\n",
				tcph->seq, ntohl(ntcp_seq), tcph->source,
				ntcp_dest_port);
		}
	}
	return NF_ACCEPT;
}

static int send_tcp_rst2datax_socket(void)
{
	int ret = 0;
	if (ntcp_rst_skb) {
		struct tcphdr *th = tcp_hdr(ntcp_rst_skb);
		struct iphdr *iph = ip_hdr(ntcp_rst_skb);
		u16 ip_total_len = sizeof(struct iphdr) + sizeof(struct tcphdr);

		// since it's copied from tcp_sync, there maybe some tcp options which we don't need.
		skb_trim(ntcp_rst_skb, ip_total_len);
		// clear the timestamp to force regenerate in the kernel.
		ntcp_rst_skb->tstamp = 0;

		th->ack = 0;
		th->ack_seq = 0;
		th->cwr = 0;
		th->doff = 5;
		th->ece = 0;
		th->fin = 0;
		th->rst = 1;
		th->syn = 0;
		th->window = 0;
		iph->tos = 2;
		put_unaligned(htons(ip_total_len), &(iph->tot_len));
		put_unaligned(ntcp_seq, &(th->seq));
		th->check = 0;
		iph->check = 0;

		iph->check = ip_fast_csum((const unsigned char *)iph, iph->ihl);
		th->check = tcp_v4_check(sizeof(struct tcphdr), iph->saddr,
					 iph->daddr,
					 csum_partial(th, th->doff << 2, 0));

		ret = netif_rx_ni(ntcp_rst_skb);
		if (ret != NET_RX_SUCCESS) {
			netinfo_pr_err("Failed to send TCP reset packet\n");
		} else {
			netinfo_pr_debug("TCP reset is sent with seq %x\n",
					 ntcp_seq);
			return ret;
		}
	} else {
		netinfo_pr_info("No receive TCP SYNC, do nothing\n");
	}

	return ret;
}

static int netdev_event_notifier_callback(struct notifier_block *nb,
					  unsigned long state, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	if (!dev) {
		return NOTIFY_OK;
	}

	// non retail-demo case and compare "usb" without number
	if (strncmp(dev->name, RETAIL_DEMO_INTERFACE_NAME,
		    strlen(RETAIL_DEMO_INTERFACE_NAME) - 1) != 0) {
		netdev_state_event_handler(nb, state, ptr);
		return NOTIFY_OK;
	}

	if (!test_bit(__LINK_STATE_NOCARRIER, &dev->state)) {
		// USB0 network device marks as up
		atomic_set(&usb0_carrier_up, 1);
		register_tcp_nf_callback();
	} else {
		if (atomic_read(&usb0_carrier_up)) {
			// USB0 network device has gone up and down
			netinfo_pr_info(
				"Network device '%s' went down and trigger tcp reset\n",
				dev->name);
			send_tcp_rst2datax_socket();
		}
		// USB0 network device marks as down
		atomic_set(&usb0_carrier_up, 0);
		unregister_tcp_nf_callback();
	}
	return NOTIFY_OK;
}

struct nf_hook_ops nfho_icmp, nfho_in_arp, nfho_out_arp, nfho_out_tcp_sync_ack,
	nfho_out_tcp_ack_seq;
static struct notifier_block carrier_off_notifier = {
	.notifier_call = netdev_event_notifier_callback,
};

void register_nf_callback(void)
{
	nfho_icmp.hook = nf_icmp_callback;
	nfho_icmp.hooknum = NF_INET_PRE_ROUTING;
	nfho_icmp.pf = PF_INET;
	nfho_icmp.priority = NF_IP_PRI_FIRST;
	nf_register_net_hook(&init_net, &nfho_icmp);

	nfho_in_arp.hook = nf_arp_in_callback;
	nfho_in_arp.hooknum = NF_ARP_IN;
	nfho_in_arp.pf = NFPROTO_ARP;
	nfho_in_arp.priority = NF_IP_PRI_FIRST;
	nf_register_net_hook(&init_net, &nfho_in_arp);

	nfho_out_arp.hook = nf_arp_out_callback;
	nfho_out_arp.hooknum = NF_ARP_OUT;
	nfho_out_arp.pf = NFPROTO_ARP;
	nfho_out_arp.priority = NF_IP_PRI_FIRST;
	nf_register_net_hook(&init_net, &nfho_out_arp);
}

void unregister_nf_callback(void)
{
	nf_unregister_net_hook(&init_net, &nfho_icmp);
	nf_unregister_net_hook(&init_net, &nfho_in_arp);
	nf_unregister_net_hook(&init_net, &nfho_out_arp);
}

void register_tcp_nf_callback(void)
{
	nfho_out_tcp_sync_ack.hook = nf_tcp_sync_ack_out_callback;
	nfho_out_tcp_sync_ack.hooknum = NF_INET_PRE_ROUTING;
	nfho_out_tcp_sync_ack.pf = PF_INET;
	nfho_out_tcp_sync_ack.priority = NF_IP_PRI_FIRST;
	nf_register_net_hook(&init_net, &nfho_out_tcp_sync_ack);

	nfho_out_tcp_ack_seq.hook = nf_tcp_egress_seq_callback;
	nfho_out_tcp_ack_seq.hooknum = NF_INET_POST_ROUTING;
	nfho_out_tcp_ack_seq.pf = PF_INET;
	nfho_out_tcp_ack_seq.priority = NF_IP_PRI_FIRST;
	nf_register_net_hook(&init_net, &nfho_out_tcp_ack_seq);
}

void unregister_tcp_nf_callback(void)
{
	nf_unregister_net_hook(&init_net, &nfho_out_tcp_sync_ack);
	nf_unregister_net_hook(&init_net, &nfho_out_tcp_ack_seq);
}

void register_carrier_notifier(void)
{
	int ret = 0;
	ret = register_netdevice_notifier(&carrier_off_notifier);
	if (ret) {
		netinfo_pr_err("Failed to register notifier\n");
	}
}

void unregister_carrier_notifier(void)
{
	unregister_netdevice_notifier(&carrier_off_notifier);
}
