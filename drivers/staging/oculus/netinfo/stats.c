// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_bridge.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include "type_header.h"
#include "stats_header.h"

#define DHCP_COUNTERS_MAX DHCPRENEW + 1
#define EAPOL_KEY_MAX 4

static u32 dhcp_counters[DHCP_COUNTERS_MAX] = { 0 };
static u32 eapol_counters[EAPOL_KEY_MAX] = { 0 };

void reset_stats_counters(void)
{
	memset(dhcp_counters, 0, sizeof(dhcp_counters));
	memset(eapol_counters, 0, sizeof(eapol_counters));
}

void update_garp_stats(u32 ip)
{
	// if it's belonged to softap network range,
	// set it as 1.
	if ((ip >> 24) == 192)
		dhcp_counters[0] = 1;
}

void update_dhcp_stats(int type)
{
	switch (type) {
	case DHCPDISCOVER:
	case DHCPOFFER:
	case DHCPREQUEST:
	case DHCPACK:
	case DHCPNACK:
	case DHCPDECLINE:
	case DHCPRELEASE:
	case DHCPINFORM:
	case DHCPRENEW:
		dhcp_counters[type] = 1;
		break;
	default:
		break;
	}
}

static u32 nf_dhcp_parse_callback(void *priv, struct sk_buff *skb,
				  const struct nf_hook_state *state)
{
	struct iphdr *iph = NULL;
	struct udphdr *udph = NULL;
	struct dhcphdr *dhcph = NULL;
	u16 src_port, dst_port;

	if (!skb) {
		return NF_ACCEPT;
	}
	if (skb->len < MIN_DHCP_TOTAL_LEN) {
		return NF_ACCEPT;
	}
	iph = ip_hdr(skb);

	// Check if packet is IPv4 and UDP
	if (iph->protocol != IPPROTO_UDP) {
		return NF_ACCEPT;
	}
	udph = udp_hdr(skb);
	src_port = ntohs(udph->source);
	dst_port = ntohs(udph->dest);
	if ((src_port == DHCP_SERVER_PORT || src_port == DHCP_CLIENT_PORT) &&
	    (dst_port == DHCP_SERVER_PORT || dst_port == DHCP_CLIENT_PORT)) {
		dhcp_flag_start();
		dhcph = (struct dhcphdr *)((unsigned char *)udph +
					   sizeof(struct udphdr));
		netinfo_pr_debug(
			"%s %d: DHCP Option0 %x Option2 %x TransId %x\n",
			__func__, __LINE__, dhcph->options[0],
			dhcph->options[2], dhcph->xid);

		if (dhcph->options[0] == DHCP_OPTION_MESSAGE_TYPE) {
			update_dhcp_stats(dhcph->options[2]);
			if (dhcph->options[2] == DHCPACK) {
				netinfo_pr_info(
					"Got DHCPACK, unregister all sniff callback.\n");
				send_session_cancel_rx_handler_msg(skb->dev);
			}
		}
	}
	return NF_ACCEPT;
}

/* for tx, eth_hdr(skb) gives wrong offset, it's because of skb is from the beginning of dev_queue_xmit,
 * some fields are not init yet. So fill the ethhdr by skb->data;
 * for rx, eth_hdr(skb) return right offset and ethhdr can be NULL;
*/
int update_eapol_message_counter(struct sk_buff *skb, unsigned char *ethhdr)
{
	struct ethhdr *eth;
	struct ieee802_1x_hdr *eapol;
	struct wpa_eapol_key *key;
	u16 key_info;
	int ret = -1;

	if (!skb)
		return ret;

	if (skb->len < sizeof(struct ethhdr) + sizeof(struct ieee802_1x_hdr)) {
		return ret;
	}

	if (ethhdr != NULL) {
		eth = (struct ethhdr *)ethhdr;
	} else {
		eth = eth_hdr(skb);
	}

	if (ntohs(eth->h_proto) != ETH_P_PAE)
		return ret;

	// EAPOL payload begins after Ethernet header
	eapol = (struct ieee802_1x_hdr *)(eth + 1);
	if (eapol->type != IEEE802_1X_TYPE_EAPOL_KEY)
		return ret;

	key = (struct wpa_eapol_key *)(eapol + 1);
	key_info = be16_to_cpu(key->key_info);

	if ((key_info & WPA_KEY_INFO_KEY_TYPE) &&
	    !(key_info & WPA_KEY_INFO_KEY_MIC) &&
	    (key_info & WPA_KEY_INFO_KEY_ACK)) {
		ret = EAPOL_KEY_M1;
		eapol_counters[ret] = 1;
	}

	if ((key_info & WPA_KEY_INFO_KEY_TYPE) &&
	    (key_info & WPA_KEY_INFO_KEY_MIC) &&
	    !(key_info & WPA_KEY_INFO_KEY_ACK) &&
	    !(key_info & WPA_KEY_INFO_INSTALL)) {
		ret = EAPOL_KEY_M2;
		eapol_counters[ret] = 1;
	}

	if ((key_info & WPA_KEY_INFO_KEY_TYPE) &&
	    (key_info & WPA_KEY_INFO_KEY_MIC) &&
	    (key_info & WPA_KEY_INFO_KEY_ACK) &&
	    (key_info & WPA_KEY_INFO_INSTALL)) {
		ret = EAPOL_KEY_M3;
		eapol_counters[ret] = 1;
	}

	if ((key_info & WPA_KEY_INFO_KEY_TYPE) &&
	    (key_info & WPA_KEY_INFO_KEY_MIC) &&
	    !(key_info & WPA_KEY_INFO_KEY_ACK) &&
	    (key_info & WPA_KEY_INFO_SECURE)) {
		ret = EAPOL_KEY_M4;
		eapol_counters[ret] = 1;
	}

	return ret;
}

int handler_eapol_tx(struct kprobe *p, struct pt_regs *regs)
{
	struct sk_buff *skb = NULL;
	struct ethhdr *eth = NULL;

	skb = (struct sk_buff *)regs->regs[0];
	if (!skb || !skb->dev)
		return 0;

	if (skb->len < (sizeof(struct ethhdr) + sizeof(struct ieee802_1x_hdr) +
			sizeof(struct wpa_eapol_key)))
		return 0;

	eth = (struct ethhdr *)skb->data;

	if (ntohs(eth->h_proto) == ETH_P_PAE) {
		update_eapol_message_counter(skb, (unsigned char *)eth);
	}
	return 0;
}

static struct nf_hook_ops nfho_dhcp_parse_in, nfho_dhcp_parse_out;
void register_dhcp_nf_callback(void)
{
	nfho_dhcp_parse_out.hook = nf_dhcp_parse_callback;
	nfho_dhcp_parse_out.hooknum = NF_INET_PRE_ROUTING;
	nfho_dhcp_parse_out.pf = PF_INET;
	nfho_dhcp_parse_out.priority = NF_IP_PRI_FIRST;
	nf_register_net_hook(&init_net, &nfho_dhcp_parse_out);

	nfho_dhcp_parse_in.hook = nf_dhcp_parse_callback;
	nfho_dhcp_parse_in.hooknum = NF_INET_POST_ROUTING;
	nfho_dhcp_parse_in.pf = PF_INET;
	nfho_dhcp_parse_in.priority = NF_IP_PRI_FIRST;
	nf_register_net_hook(&init_net, &nfho_dhcp_parse_in);
}

void unregister_dhcp_nf_callback(void)
{
	nf_unregister_net_hook(&init_net, &nfho_dhcp_parse_in);
	nf_unregister_net_hook(&init_net, &nfho_dhcp_parse_out);
}

// sysfs read function: "<garp> <discover> <offer> <request> <decline> <ack> <nack>"
ssize_t dhcp_counters_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	int val = sprintf(buf, "%d %d %d %d %d %d %d %d %d %d\n%d %d %d %d\n",
			  dhcp_counters[0], dhcp_counters[DHCPDISCOVER],
			  dhcp_counters[DHCPOFFER], dhcp_counters[DHCPREQUEST],
			  dhcp_counters[DHCPDECLINE], dhcp_counters[DHCPACK],
			  dhcp_counters[DHCPNACK], dhcp_counters[DHCPRELEASE],
			  dhcp_counters[DHCPINFORM], dhcp_counters[DHCPRENEW],
			  eapol_counters[0], eapol_counters[1],
			  eapol_counters[2], eapol_counters[3]);
	return val;
}

// sysfs write function: not support to write
ssize_t dhcp_counters_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	netinfo_pr_info("not support to write\n");
	return 0;
}

static struct kprobe kp;
int register_dev_queue_xmit_kp(void)
{
	int val = 0;
	kp.symbol_name = "dev_queue_xmit";
	kp.pre_handler = handler_eapol_tx;
	val = register_kprobe(&kp);
	if (val != 0) {
		netinfo_pr_info(
			"Failed to register kprobe on dev_queue_xmit %d\n",
			val);
	}
	return val;
}

void unregister_dev_queue_xmit_kp(void)
{
	unregister_kprobe(&kp);
}

void enable_dev_queue_xmit_kp(void)
{
	enable_kprobe(&kp);
}

void disable_dev_queue_xmit_kp(void)
{
	disable_kprobe(&kp);
}
