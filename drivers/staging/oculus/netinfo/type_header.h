/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) Meta Platforms, Inc. and affiliates. */

#ifndef _TYPE_HEADER_H
#define _TYPE_HEADER_H

#define MSG_ARRAY_SIZE 255
#define IP_ADDR_LEN sizeof(u32)
#define ICMP_ID_TAG 0xEAEA

typedef enum {
	MSG_SESSION_ADD,
	MSG_SESSION_DEL,
	MSG_SESSION_STATS,
	MSG_SESSION_LATENCY,
	MSG_SESSION_SEND_GARP,
	MSG_SESSION_CANCEL_RX_HANDLER
} MSG_TYPE;

typedef struct {
	u8 dst_macaddr[ETH_ALEN];
	u32 src_ip;
	u32 dst_ip;
} ADDR_INFO_t;

typedef struct {
	u32 tcp_sync_rcv;
	u32 tcp_sync_snd;
	u32 tcp_err;
	u32 ampdu_cnt;
	u32 non_ampdu_cnt;
	u32 amsdu_cnt;
	u32 non_amsdu_cnt;
	u32 rssi;
	u32 noise;
} LINK_INFO_t;

typedef struct {
	atomic_t flag;
	MSG_TYPE type;
	struct net_device *dev;
	struct delayed_work latency_work;
	u8 src_macaddr[ETH_ALEN];
	ADDR_INFO_t addr;
	LINK_INFO_t link_info;
	unsigned long last_snd_ts;
	unsigned long last_rcv_ts;
	struct list_head list;
} MSG_BUF_t;

#define NETINFO_TCPRST_FEATURE 0
#define NETINFO_GARP_FEATURE 1
#define NETINFO_BYPASS_DHCP_FEATURE 2
#define NETINFO_MAX_FEATURE 3

#define NETINFO_SET_FEATURE(feature_flags, bit) \
	atomic_or((1U << (bit)), &(feature_flags))

#define NETINFO_CLEAR_FEATURE(feature_flags, bit) \
	atomic_and(~(1U << (bit)), &(feature_flags))

#define NETINFO_IS_FEATURE_ENABLED(feature_flags, bit) \
	((atomic_read(&(feature_flags)) & (1U << (bit))) != 0)

#define DHCP_OPTION_MESSAGE_TYPE 53
#define DHCP_OPTION_SERVER_ID 54
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE 4
#define DHCPACK 5
#define DHCPNACK 6
#define DHCPRELEASE 7
#define DHCPINFORM 8
#define DHCPRENEW 9
#define DHCPOPTIONREQUESTED_IP 50
#define DHCPOPTIONEND 255
#define MIN_DHCP_TOTAL_LEN 285

struct dhcphdr {
	u8 op; // Message op code / message type
	u8 htype; // Hardware address type
	u8 hlen; // Hardware address length
	u8 hops; // Client sets to zero, optionally used by relay agents
	u32 xid; // Transaction ID
	u16 secs; // Seconds elapsed since client began address acquisition
	u16 flags; // Flags
	u32 ciaddr; // Client IP address (only filled in if client is in BOUND, RENEW or REBINDING state)
	u32 yiaddr; // 'your' (client) IP address
	u32 siaddr; // IP address of next server to use in bootstrap
	u32 giaddr; // Relay agent IP address
	u8 chaddr[16]; // Client hardware address
	u8 sname[64]; // Optional server host name
	u8 file[128]; // Boot file name
	u8 cookie[4]; // Cookie
	u8 options[312]; // Optional parameters field
} __attribute__((packed));

#define SESSION_HASH_BITS 4
#define SESSION_HASH_SIZE (1 << SESSION_HASH_BITS)

// Session timeout in HZ = 250, 120 seconds.
#define SESSION_MAX_TIMEOUT (120 * 250)

struct session_entry {
	struct hlist_node hlist;
	unsigned char addr[6 + 2] __aligned(sizeof(u16));
	MSG_BUF_t *msg;
	unsigned long last_rtt, total_rtt;
	u32 total_sent, total_rcv;
	struct rcu_head rcu;
	struct list_head list;
	struct timer_list timer;
} __aligned(sizeof(u64));

void put_msg(MSG_BUF_t *msg);
int send_session_update_msg(const u8 *mac, ADDR_INFO_t addr,
			    struct net_device *dev);
int send_session_latency_msg(const u8 *mac, unsigned long ts);
int send_session_send_garp_msg(ADDR_INFO_t addr, struct net_device *dev);
int send_session_cancel_rx_handler_msg(struct net_device *dev);

struct session_entry *session_hash_lookup(const unsigned char *addr);
struct session_entry *session_hash_add(const unsigned char *addr,
				       MSG_BUF_t *msg);
bool session_hash_del(struct session_entry *entry);
void session_flush_sources(void);
void session_table_init(void);
void session_table_deinit(void);

int send_icmp_skb(MSG_BUF_t *msg);
int send_arp_reply(struct net_device *dev, u8 *target_mac, __be32 target_ip);
void register_nf_callback(void);
void unregister_nf_callback(void);
void register_tcp_nf_callback(void);
void unregister_tcp_nf_callback(void);
void register_carrier_notifier(void);
void unregister_carrier_notifier(void);
void netdev_state_event_handler(struct notifier_block *nb, unsigned long state,
				void *ptr);
int register_pretcp_rx_handler(struct net_device *dev);
int unregister_pretcp_rx_handler(struct net_device *dev);
void netinfo_disable_all_feature(void);
void dhcp_flag_start(void);
int dhcp_flag_get(void);

int netinfo_tcp_reset_feature_enabled(void);
int netinfo_garp_feature_enabled(void);
int netinfo_garp_debug_feature_enabled(void);
void netinfo_garp_debug_feature_disable(void);
int netinfo_bypass_dhcp_feature_enabled(void);

#define PRINT_MAC_ADDR_FMT "%02x:**:%02x:%02x"
#define PRINT_MAC_ADDR_REF(a) (a)[0], (a)[4], (a)[5]

#define RETAIL_DEMO_INTERFACE_NAME "usb0"
#define SOFTAP_INTERFACE_NAME "p2p0"

#define SOFTAP_NETWORK_MASK 0xFF000000
#define SOFTAP_NETWORK_GATEWAY 0xC0A83101
#define SOFTAP_NETWORK_RANGE 0xC0000000

#define NETINFO_TAG "NETINFO"

#define netinfo_pr_info(fmt, args...)                                        \
	do {                                                                 \
		pr_info_ratelimited("%s %s:%d: " fmt, NETINFO_TAG, __func__, \
				    __LINE__, ##args);                       \
	} while (0)

#define netinfo_pr_err(fmt, args...)                                        \
	do {                                                                \
		pr_err_ratelimited("%s %s %d: " fmt, NETINFO_TAG, __func__, \
				   __LINE__, ##args);                       \
	} while (0)

#define netinfo_pr_debug(fmt, args...)

// Borrow from time_after in jiffies.h. Make sure a is after b.
#define time_diff(a, b) ((long)((a) - (b)))

#endif
