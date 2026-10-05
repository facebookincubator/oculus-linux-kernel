/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) Meta Platforms, Inc. and affiliates. */

#ifndef _STATS_HEADER_H
#define _STATS_HEADER_H

#define ETH_P_PAE               0x888E
#define IEEE802_1X_TYPE_EAPOL_KEY 3

// WPA Key Info Bitmasks
#define WPA_KEY_INFO_KEY_TYPE          (1 << 3)
#define WPA_KEY_INFO_KEY_MIC           (1 << 8)
#define WPA_KEY_INFO_KEY_ACK           (1 << 7)
#define WPA_KEY_INFO_INSTALL           (1 << 6)
#define WPA_KEY_INFO_SECURE            (1 << 9)
#define WPA_KEY_INFO_ENCR_KEY_DATA     (1 << 13)

// Eapol Key Type
#define EAPOL_KEY_M1          0
#define EAPOL_KEY_M2          1
#define EAPOL_KEY_M3          2
#define EAPOL_KEY_M4          3
#define EAPOL_KEY_MAX         4

struct ieee802_1x_hdr {
    u8 version;
    u8 type;
    u16 length;
} __packed;

struct wpa_eapol_key {
    u8 type;
    u16 key_info;
    u16 key_length;
    u64 replay_counter;
    u8 key_nonce[32];
    u8 key_iv[16];
    u8 key_rsc[8];
    u8 key_id[8];
    u8 key_mic[16];
    u16 key_data_length;
    // Followed by key_data[]
} __packed;

void reset_stats_counters(void);
void update_dhcp_stats(int type);
void update_garp_stats(u32 ip);
int update_eapol_message_counter(struct sk_buff *skb, unsigned char* ethhdr);
rx_handler_result_t parse_dhcp_tx_handler(struct sk_buff **pskb);
int handler_eapol_tx(struct kprobe *p, struct pt_regs *regs);

void register_dhcp_nf_callback(void);
void unregister_dhcp_nf_callback(void);

int register_dev_queue_xmit_kp(void);
void unregister_dev_queue_xmit_kp(void);
void enable_dev_queue_xmit_kp(void);
void disable_dev_queue_xmit_kp(void);

ssize_t dhcp_counters_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf);
ssize_t dhcp_counters_store(struct kobject *kobj, struct kobj_attribute *attr,
    const char *buf, size_t count);

#endif
