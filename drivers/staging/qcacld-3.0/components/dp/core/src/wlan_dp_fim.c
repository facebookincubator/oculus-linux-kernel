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

#include <dp_types.h>
#include <dp_internal.h>
#include <qdf_status.h>
#include <qdf_nbuf.h>
#include <qdf_hashtable.h>
#include "queue.h"
#include "wlan_dp_fim.h"

/**
 * dp_fim_get_hash_idx() - Convert 32 bit to 8 bit hash index.
 * @hash32: 32-bit hash value
 *
 * Return: 8-bit hash index
 */
static uint8_t dp_fim_get_hash_idx(uint32_t hash32)
{
	return ((hash32 & 0xFF000000) >> 24) ^ ((hash32 & 0x00FF0000) >> 16) ^
	       ((hash32 & 0x0000FF00) >> 8) ^ (hash32 & 0xFF);
}

/**
 * dp_fim_hash_table_init() - Initialize hash table
 * @ht: fim hash table
 * @len: hash table length
 *
 * Return: none
 */
static void dp_fim_hash_table_init(struct fim_hash_table *ht, uint32_t len)
{
	struct qdf_ht *hash_list;
	uint32_t idx;

	hash_list = ht->hlist_hash_table_head;

	for (idx = 0; idx < len; idx++)
		qdf_hl_init(&hash_list[idx]);
}

/**
 * dp_fim_parse_skb_flow_info() - Parse flow info from skb
 * @skb: network buffer
 * @flow: pointer to flow tuple info
 *
 * Return: none
 */
static void dp_fim_parse_skb_flow_info(struct sk_buff *skb,
				       struct flow_info *flow)
{
	struct qdf_flow_info flow_info;

	if (qdf_nbuf_sock_is_ipv4_pkt(skb)) {
		if (!qdf_nbuf_is_ipv4_first_fragment(skb)) {
			flow->flags |= FLOW_INFO_PRESENT_IP_FRAGMENT;
			return;
		}

		if (qdf_nbuf_get_ipv4_flow_info(skb, &flow_info))
			return;

		flow->src_port = flow_info.src_port;
		flow->dst_port = flow_info.dst_port;
		flow->src_ip.ipv4_addr = flow_info.src_ip.ipv4_addr;
		flow->dst_ip.ipv4_addr = flow_info.dst_ip.ipv4_addr;
		flow->proto = flow_info.proto;
		flow->flags |= FLOW_INFO_IPV4_PARSE_SUCCESS;

	} else if (qdf_nbuf_sock_is_ipv6_pkt(skb)) {
		if (qdf_nbuf_get_ipv6_flow_info(skb, &flow_info))
			return;

		flow->src_port = flow_info.src_port;
		flow->dst_port = flow_info.dst_port;
		qdf_mem_copy(&flow->src_ip.ipv6_addr, &flow_info.src_ip.ipv6_addr,
			     sizeof(flow_info.src_ip.ipv6_addr));
		qdf_mem_copy(&flow->dst_ip.ipv6_addr, &flow_info.dst_ip.ipv6_addr,
			     sizeof(flow_info.dst_ip.ipv6_addr));
		flow->proto = flow_info.proto;
		flow->flags |= FLOW_INFO_IPV6_PARSE_SUCCESS;
	}
}

/**
 * dp_fim_flow_exact_match() - Check if two flows are matching.
 * @fi: pointer to 1st flow tuple info
 * @flow: pointer to 2nd flow tuple info to match with 1st
 *
 * Return: true when both flow tuble matches exactly
 */
static inline
bool dp_fim_flow_exact_match(struct flow_info *fi, struct flow_info *flow)
{
	if (fi->proto == htons(ETH_P_IP)) {
		if (flow->src_ip.ipv4_addr == fi->src_ip.ipv4_addr &&
		    flow->dst_ip.ipv4_addr == fi->dst_ip.ipv4_addr &&
		    flow->src_port == fi->src_port &&
		    flow->dst_port == fi->dst_port &&
		    flow->proto == fi->proto) {
			return true;
		}
	} else if (fi->proto == htons(ETH_P_IPV6)) {
		if (qdf_mem_cmp(&flow->dst_ip.ipv6_addr, &fi->dst_ip.ipv6_addr,
				sizeof(struct in6_addr)) == 0 &&
		    qdf_mem_cmp(&flow->dst_ip.ipv6_addr, &fi->dst_ip.ipv6_addr,
				sizeof(struct in6_addr)) == 0 &&
		    flow->flow_label == fi->flow_label) {
			return true;
		}
	}
	return false;
}

/**
 * dp_fim_hash_table_match() - Get flow node that matches.
 * @ht: fim hash table
 * @skb: network buffer
 * @hash: 32-bit hash value
 * @flags: flow tuple flags
 * @flow: pointer to flow tuple info
 *
 * Return: fim node upon successful flow match in hash table
 */
static inline
struct fim_node *dp_fim_hash_table_match(struct fim_hash_table *ht,
					 struct sk_buff *skb,
					 uint32_t hash, uint32_t flags,
					 struct flow_info *flow)
{
	struct fim_node *fn;
	struct qdf_ht *lhead;
	uint8_t hash_idx;
	bool found = false;

	hash_idx = dp_fim_get_hash_idx(hash);

	lhead = &ht->hlist_hash_table_head[hash_idx];
	qdf_hl_for_each_entry_rcu(fn, lhead, hnode) {
		if (flags & FIM_SOCK_FLAG_BIT) {
			if (fn->sk == skb->sk && fn->hash == hash) {
				fn->last_timestamp = qdf_system_ticks();
				found = true;
				break;
			}
		} else if (dp_fim_flow_exact_match(flow, &fn->flow)) {
			fn->last_timestamp = qdf_system_ticks();
			found = true;
			break;
		}
	}

	if (found)
		return fn;

	return NULL;
}

/*
 * dp_fim_hash_table_insert_node() - Insert a connection match into the hash.
 * @fim_ctx: pointer to fim context of vdev
 * @hash: 32-bit hash value
 * @fn: pointer to fim node
 *
 * Return: none
 */
static inline
void dp_fim_hash_table_insert_node(struct fim_vdev_ctx *fim_ctx,
				   uint32_t hash, struct fim_node *fn)
{
	uint8_t hash_idx = dp_fim_get_hash_idx(hash);

	qdf_spin_lock_bh(&fim_ctx->ht.lock);
	qdf_hl_add_head_rcu(&fn->hnode,
			    &fim_ctx->ht.hlist_hash_table_head[hash_idx]);
	DP_STATS_INC(fim_ctx, num_flow_node, 1);
	qdf_spin_unlock_bh(&fim_ctx->ht.lock);
}

static void dp_fim_free_cb(qdf_rcu_head_t *rp)
{
	struct fim_node *fn = container_of(rp, struct fim_node, rcu);

	dp_info("FIM: reclaim node for policy_id:%x metadata:%x",
		fn->policy_id, fn->metadata);
	qdf_mem_free(fn);
}

static inline
bool dp_fim_is_delete_node(struct flow_info *flow,
			   struct policy_notifier_data *policy,
			   enum fim_delete_type type, struct fim_node *fn)
{
	bool ret = false;

	if (!fn)
		goto invalid;

	switch (type) {
	case FIM_DELETE_ALL:
		ret = true;
		break;
	case FIM_TIMEOUT:
		if (qdf_system_time_after(qdf_system_ticks(),
					  fn->last_timestamp +
					  qdf_system_msecs_to_ticks
					  (FIM_EXPIRY_TIMEOUT_MS))) {
			ret = true;
		}
		break;
	case FIM_POLICY_ID:
		if (!policy || !flow)
			goto invalid;

		if (fn->policy_id == policy->policy_id)
			ret = true;
		break;
	case FIM_PRIO:
		if (!policy || !flow)
			goto invalid;

		if (policy->prio >= fn->prio)
			if (fpm_flow_regex_match(flow, &fn->flow))
				ret = true;
		break;
	case FIM_REGEX:
		if (!flow)
			goto invalid;

		if (fpm_flow_regex_match(flow, &fn->flow))
			ret = true;
		break;
	default:
		break;
	}

invalid:
	return ret;
}

static inline
void dp_fim_hash_table_delete_node(struct fim_vdev_ctx *fim_ctx,
				   struct flow_info *flow,
				   struct policy_notifier_data *policy,
				   enum fim_delete_type type)
{
	struct fim_node *fn = NULL;
	struct qdf_ht *lhead;
	struct qdf_ht_entry *tmp;
	uint16_t hash_idx;

	qdf_spin_lock_bh(&fim_ctx->ht.lock);
	for (hash_idx = 0; hash_idx < FIM_HASH_SIZE; hash_idx++) {
		lhead = &fim_ctx->ht.hlist_hash_table_head[hash_idx];
		qdf_hl_for_each_entry_safe(fn, tmp, lhead, hnode) {
			if (dp_fim_is_delete_node(flow, policy, type, fn)) {
				qdf_hl_del_rcu(&fn->hnode);
				DP_STATS_DEC(fim_ctx, num_flow_node, 1);
				qdf_call_rcu(&fn->rcu, dp_fim_free_cb);
			}
		}
	}
	qdf_spin_unlock_bh(&fim_ctx->ht.lock);
}

static inline bool dp_fim_is_proto_supported(struct sk_buff *skb)
{
	if (skb->sk &&
	    (qdf_nbuf_sock_is_ipv4_pkt(skb) ||
	    qdf_nbuf_sock_is_ipv6_pkt(skb)) &&
	    (qdf_nbuf_sock_is_udp_pkt(skb) ||
	     qdf_nbuf_sock_is_tcp_pkt(skb)))
		return true;
	return false;
}

static void dp_fim_flow_expiry_timer_cb(void *arg)
{
	struct fim_vdev_ctx *fim_ctx = (struct fim_vdev_ctx *)arg;

	if (!fim_ctx)
		return;

	dp_info("FIM timer expiry");
	dp_fim_hash_table_delete_node(fim_ctx, NULL, NULL, FIM_TIMEOUT);
	if (fim_ctx->fim_enable)
		qdf_timer_mod(&fim_ctx->flow_expiry_timer,
			      FIM_EXPIRY_TIMEOUT_MS);
}

QDF_STATUS dp_fim_update_metadata(struct wlan_dp_intf *dp_intf, qdf_nbuf_t skb)
{
	struct fim_vdev_ctx *fim_ctx = dp_intf->fim_ctx;
	struct fim_hash_table *ht;
	uint32_t hash;
	uint32_t metadata = 0;
	uint32_t policy_id = 0;
	uint8_t match_flags = 0;
	struct fim_node *fn = NULL;
	struct flow_info flow = {0};
	struct sock *sk = NULL;
	enum dp_fpm_status status;

	if (qdf_unlikely(!skb || !fim_ctx))
		return QDF_STATUS_E_INVAL;

	if (!fim_ctx->fim_enable) {
		skb->mark = FIM_INVALID_METADATA;
		return QDF_STATUS_E_INVAL;
	}

	sk = skb->sk;
	if (qdf_unlikely(!sk || !sk->sk_txhash)) {
		DP_STATS_INC(fim_ctx, sk_invalid, 1);

		dp_fim_parse_skb_flow_info(skb, &flow);
		if (!flow.flags || flow.flags & FLOW_INFO_PRESENT_IP_FRAGMENT) {
			DP_STATS_INC(fim_ctx, pkt_not_support, 1);
			skb->mark = FIM_INVALID_METADATA;
			return QDF_STATUS_E_INVAL;
		}

		hash = skb_get_hash(skb);
	} else {
		DP_STATS_INC(fim_ctx, sk_valid, 1);
		if (!dp_fim_is_proto_supported(skb)) {
			DP_STATS_INC(fim_ctx, pkt_not_support, 1);
			skb->mark = FIM_INVALID_METADATA;
			return QDF_STATUS_E_INVAL;
		}
		hash = sk->sk_txhash;
		match_flags |= FIM_SOCK_FLAG_BIT;
	}

	ht = &fim_ctx->ht;
	qdf_rcu_read_lock_bh();
	fn = dp_fim_hash_table_match(ht, skb, hash, match_flags, &flow);
	if (qdf_likely(fn)) {
		skb->mark = fn->metadata;
		DP_STATS_INC(fn, num_pkt, 1);
		qdf_rcu_read_unlock_bh();
	} else {
		qdf_rcu_read_unlock_bh();
		if (match_flags & FIM_SOCK_FLAG_BIT)
			dp_fim_parse_skb_flow_info(skb, &flow);

		if (!flow.flags || flow.flags & FLOW_INFO_PRESENT_IP_FRAGMENT) {
			DP_STATS_INC(fim_ctx, pkt_not_support, 1);
			return QDF_STATUS_E_INVAL;
		}

		status = fpm_policy_flow_match(dp_intf, skb, &metadata, &flow,
					       &policy_id);
		if (status == FLOW_MATCH_DO_NOT_FOUND) {
			metadata = FIM_INVALID_METADATA;
			policy_id = FIM_INVALID_POLICY_ID;
		}

		fn = (struct fim_node *)qdf_mem_malloc(sizeof(struct fim_node));
		if (!fn)
			return QDF_STATUS_E_NOMEM;

		qdf_mem_copy(&fn->flow, &flow, sizeof(struct flow_info));
		fn->metadata = metadata;
		fn->sk = sk;
		fn->hash = hash;
		fn->last_timestamp = qdf_system_ticks();
		fn->policy_id = policy_id;
		skb->mark = fn->metadata;
		DP_STATS_INC(fn, num_pkt, 1);
		dp_info("New FIM node policy_id:0x%x metadata:%x "
			"srcport:%d dstport:%d proto:%d flags:0x%x "
			"timestamp:%u num_pkt:%d",
			fn->policy_id, skb->mark,
			fn->flow.src_port, fn->flow.dst_port,
			fn->flow.proto, fn->flow.flags,
			fn->last_timestamp, fn->stats.num_pkt);

		if (skb->protocol == htons(ETH_P_IPV6))
			dp_info("src_ip:%pI6 dst_ip:%pI6",
				fn->flow.src_ip.ipv6_addr,
				fn->flow.dst_ip.ipv6_addr);
		else
			dp_info("src_ip:%pI4 dst_ip:%pI4",
				&fn->flow.src_ip.ipv4_addr,
				&fn->flow.dst_ip.ipv4_addr);

		dp_fim_hash_table_insert_node(fim_ctx, hash, fn);
	}

	return QDF_STATUS_SUCCESS;
}

void dp_fim_display_hash_table(struct wlan_dp_intf *dp_intf)
{
	struct fim_vdev_ctx *fim_ctx = dp_intf->fim_ctx;
	struct fim_hash_table *ht = &fim_ctx->ht;
	uint16_t hash_idx;
	struct qdf_ht *lhead;
	struct fim_node *fn;

	dp_info("fim hash table - current_timestamp %d", qdf_system_ticks());
	qdf_rcu_read_lock_bh();
	for (hash_idx = 0; hash_idx < FIM_HASH_SIZE; hash_idx++) {
		lhead = &ht->hlist_hash_table_head[hash_idx];
		qdf_hl_for_each_entry_rcu(fn, lhead, hnode) {
			dp_info("hash_id:%d src_ip:%x dst_ip:%x "
				"srcport:%d dstport:%d proto:%d flags:0x%x "
				"policy_id:%x timestamp:%d "
				"num_pkt:%d metadata:%x",
				hash_idx, fn->flow.src_ip.ipv4_addr,
				fn->flow.dst_ip.ipv4_addr, fn->flow.src_port,
				fn->flow.dst_port, fn->flow.proto,
				fn->flow.flags, fn->policy_id,
				fn->last_timestamp, fn->stats.num_pkt,
				fn->metadata);
		}
	}
	qdf_rcu_read_unlock_bh();
}

void dp_fim_clear_hash_table(struct wlan_dp_intf *dp_intf)
{
	struct fim_vdev_ctx *fim_ctx = dp_intf->fim_ctx;

	dp_fim_hash_table_delete_node(fim_ctx, NULL, NULL, FIM_DELETE_ALL);
}

void dp_fim_display_stats(struct wlan_dp_intf *dp_intf)
{
	struct fim_vdev_ctx *fim_ctx = dp_intf->fim_ctx;

	dp_info("FIM Stats:");
	dp_info("fim_enable:%d", fim_ctx->fim_enable);
	dp_info("policy_added:%d", fim_ctx->stats.policy_added);
	dp_info("policy_removed:%d", fim_ctx->stats.policy_removed);
	dp_info("policy_update:%d", fim_ctx->stats.policy_update);
	dp_info("flow_node:%d", fim_ctx->stats.num_flow_node);
	dp_info("pkt_not_support:%d", fim_ctx->stats.pkt_not_support);
	dp_info("sk_valid_cnt:%d", fim_ctx->stats.sk_valid);
	dp_info("sk_invalid_cnt:%d", fim_ctx->stats.sk_invalid);
}

void dp_fim_clear_stats(struct wlan_dp_intf *dp_intf)
{
	struct fim_vdev_ctx *fim_ctx = dp_intf->fim_ctx;

	qdf_mem_zero(&fim_ctx->stats, sizeof(fim_ctx->stats));
}

/**
 * dp_fim_policy_update_notifier() - Removed conn hash node for all the
 *				     matched flows
 * @block: notifier block
 * @state: notifier event type
 * @data: notifier data
 *
 * Return: QDF_STATUS enumeration
 */
static int dp_fim_policy_update_notifier(struct notifier_block *block,
					 unsigned long state,
					 void *data)
{
	struct policy_notifier_data *policy = (struct policy_notifier_data *)
					       data;
	struct wlan_dp_intf *dp_intf;
	struct flow_info *flow = &policy->flow;
	qdf_notif_block *notif_block;
	struct fim_vdev_ctx *fim_ctx;
	struct fim_stats *stats;

	if (!data || !block) {
		dp_err("notifier called with invalid data");
		return -EINVAL;
	}

	notif_block = qdf_container_of(block, qdf_notif_block, notif_block);
	dp_intf = (struct wlan_dp_intf *)notif_block->priv_data;
	if (!dp_intf) {
		dp_err("dp_intf is null");
		return -EINVAL;
	}

	fim_ctx = dp_intf->fim_ctx;
	if (!fim_ctx) {
		dp_err("fim_ctx is null");
		return -EINVAL;
	}

	dp_info("policy notifier called state:%d id:%d prio:%d", state,
		policy->policy_id, policy->prio);

	switch (state) {
	case FPM_POLICY_ADD:
		dp_fim_hash_table_delete_node(fim_ctx, flow, policy, FIM_PRIO);
		DP_STATS_INC(fim_ctx, policy_added, 1);
		break;
	case FPM_POLICY_DEL:
		dp_fim_hash_table_delete_node(fim_ctx, flow, policy,
					      FIM_POLICY_ID);
		DP_STATS_INC(fim_ctx, policy_removed, 1);
		break;
	case FPM_POLICY_UPDATE:
		dp_fim_hash_table_delete_node(fim_ctx, flow, policy,
					      FIM_POLICY_ID);
		DP_STATS_INC(fim_ctx, policy_update, 1);
		break;
	default:
		dp_err("not supported state:%d", state);
	}

	stats = &fim_ctx->stats;
	if (stats->policy_added != stats->policy_removed) {
		fim_ctx->fim_enable = true;
		dp_info("Enable FIM for dp_intf:" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(dp_intf->mac_addr.bytes));
		qdf_timer_mod(&fim_ctx->flow_expiry_timer,
			      FIM_EXPIRY_TIMEOUT_MS);
	} else if (stats->policy_added == stats->policy_removed) {
		fim_ctx->fim_enable = false;
		dp_info("Disable FIM for dp_intf:" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(dp_intf->mac_addr.bytes));
	}

	return 0;
}

QDF_STATUS dp_fim_init(struct wlan_dp_intf *dp_intf)
{
	struct fim_vdev_ctx *fim_ctx;
	struct fim_hash_table *ht;
	qdf_notif_block *nb;

	fim_ctx = (struct fim_vdev_ctx *)qdf_mem_malloc(sizeof(*fim_ctx));
	if (!fim_ctx)
		return QDF_STATUS_E_NOMEM;

	/* hash table init */
	ht = &fim_ctx->ht;
	qdf_spinlock_create(&ht->lock);
	dp_fim_hash_table_init(ht, ARRAY_SIZE(ht->hlist_hash_table_head));

	/* Register for policy notifier */
	nb = &fim_ctx->fim_policy_update_notifier;
	nb->notif_block.notifier_call = dp_fim_policy_update_notifier;
	nb->priv_data = dp_intf;
	fpm_policy_event_register_notifier(dp_intf, nb);
	qdf_timer_init(NULL, &fim_ctx->flow_expiry_timer,
		       dp_fim_flow_expiry_timer_cb, (void *)fim_ctx,
		       QDF_TIMER_TYPE_SW);
	dp_intf->fim_ctx = fim_ctx;
	dp_info("FIM module initialized for dp_intf:" QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(dp_intf->mac_addr.bytes));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_fim_deinit(struct wlan_dp_intf *dp_intf)
{
	struct fim_vdev_ctx *fim_ctx = dp_intf->fim_ctx;
	qdf_notif_block *nb = &fim_ctx->fim_policy_update_notifier;

	fim_ctx->fim_enable = false;
	qdf_timer_stop(&fim_ctx->flow_expiry_timer);
	qdf_timer_free(&fim_ctx->flow_expiry_timer);

	fpm_policy_event_unregister_notifier(dp_intf, nb);
	dp_fim_hash_table_delete_node(fim_ctx, NULL, NULL, FIM_DELETE_ALL);
	qdf_mem_free(dp_intf->fim_ctx);
	dp_info("FIM module uninitialized for dp_intf:" QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(dp_intf->mac_addr.bytes));

	return QDF_STATUS_SUCCESS;
}
