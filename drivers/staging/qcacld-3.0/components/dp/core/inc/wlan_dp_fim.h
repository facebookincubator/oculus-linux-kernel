/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_DP_FIM_H_
#define _WLAN_DP_FIM_H_

#include "qdf_notifier.h"
#include "wlan_dp_priv.h"
#include "wlan_fpm_table.h"

#define FIM_HASH_SIZE		256
#define FIM_INVALID_METADATA    0x0F000000
#define FIM_INVALID_POLICY_ID   0xDEADBEEF
#define FIM_SOCK_FLAG_BIT	BIT(0)
#define FIM_EXPIRY_TIMEOUT_MS	(120 * 1000)

#define FLOW_INFO_PRESENT_PROTO			BIT(0)
#define FLOW_INFO_PRESENT_SRC_PORT		BIT(1)
#define FLOW_INFO_PRESENT_DST_PORT		BIT(2)
#define FLOW_INFO_PRESENT_IPV4_SRC_IP		BIT(3)
#define FLOW_INFO_PRESENT_IPV4_DST_IP		BIT(4)
#define FLOW_INFO_PRESENT_IPV6_SRC_IP		BIT(5)
#define FLOW_INFO_PRESENT_IPV6_DST_IP		BIT(6)
#define FLOW_INFO_PRESENT_IP_FRAGMENT		BIT(7)
#define FLOW_INFO_IPV4_PARSE_SUCCESS		(FLOW_INFO_PRESENT_PROTO |\
						FLOW_INFO_PRESENT_SRC_PORT |\
						FLOW_INFO_PRESENT_DST_PORT |\
						FLOW_INFO_PRESENT_IPV4_SRC_IP |\
						FLOW_INFO_PRESENT_IPV4_DST_IP)
#define FLOW_INFO_IPV6_PARSE_SUCCESS		(FLOW_INFO_PRESENT_PROTO |\
						FLOW_INFO_PRESENT_SRC_PORT |\
						FLOW_INFO_PRESENT_DST_PORT |\
						FLOW_INFO_PRESENT_IPV6_SRC_IP |\
						FLOW_INFO_PRESENT_IPV6_DST_IP)

enum fim_delete_type {
	FIM_DELETE_ALL,
	FIM_TIMEOUT,
	FIM_POLICY_ID,
	FIM_REGEX,
	FIM_PRIO,
};

struct fim_per_flow_stats {
	uint32_t num_pkt;
};

struct fim_stats {
	uint32_t policy_added;
	uint32_t policy_removed;
	uint32_t policy_update;
	uint32_t num_flow_node;
	uint32_t pkt_not_support;
	uint32_t sk_valid;
	uint32_t sk_invalid;
};

struct fim_hash_table {
	qdf_spinlock_t lock;
	struct hlist_head hlist_hash_table_head[FIM_HASH_SIZE];
};

struct fim_node {
	struct hlist_node hnode;
	qdf_rcu_head_t rcu;
	struct flow_info flow;
	struct sock *sk;
	uint32_t metadata;
	uint32_t hash;
	unsigned long last_timestamp;
	uint64_t policy_id;
	uint8_t prio;
	struct fim_per_flow_stats stats;
};

struct fim_vdev_ctx {
	bool fim_enable;
	struct fim_hash_table ht;
	qdf_notif_block fim_policy_update_notifier;
	struct fim_stats stats;
	qdf_timer_t flow_expiry_timer;
};

/**
 * dp_fim_init() - FIM init function
 * @dp_intf: dp context of interface
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS dp_fim_init(struct wlan_dp_intf *dp_intf);

/**
 * dp_fim_deinit() - FIM de-init function
 * @dp_intf: dp context of interface
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS dp_fim_deinit(struct wlan_dp_intf *dp_intf);

/**
 * dp_fim_update_metadata() - Update metadata for received skb flow.
 * @dp_intf: dp context of interface
 * @nbuf: Pointer to struct sk_buff
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS dp_fim_update_metadata(struct wlan_dp_intf *dp_intf,
				  qdf_nbuf_t nbuf);

/**
 * dp_fim_display_hash_table() - Display fim nodes from hash table
 * @dp_intf: dp context of interface
 *
 * Return: void
 */
void dp_fim_display_hash_table(struct wlan_dp_intf *dp_intf);

/**
 * dp_fim_clear_hash_table() - delete fim nodes from hash table
 * @dp_intf: dp context of interface
 *
 * Return: void
 */
void dp_fim_clear_hash_table(struct wlan_dp_intf *dp_intf);

/**
 * dp_fim_display_stats() - Display fim stats
 * @dp_intf: dp context of interface
 *
 * Return: void
 */
void dp_fim_display_stats(struct wlan_dp_intf *dp_intf);

/**
 * dp_fim_clear_stats() - Clear fim stats
 * @dp_intf: dp context of interface
 *
 * Return: void
 */
void dp_fim_clear_stats(struct wlan_dp_intf *dp_intf);
#endif
