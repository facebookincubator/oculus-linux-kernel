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

#include "wlan_dp_priv.h"
#include "wlan_fpm_table.h"
#include "wlan_dp_svc.h"
#include "wlan_dp_metadata.h"

static void fpm_policy_event_notifier_init(struct fpm_table *fpm)
{
	ATOMIC_INIT_NOTIFIER_HEAD(&fpm->fpm_policy_event_notif_head);
}

QDF_STATUS fpm_policy_event_register_notifier(struct wlan_dp_intf *dp_intf,
					      qdf_notif_block *nb)
{
	struct fpm_table *fpm_ctx = dp_intf->fpm_ctx;
	qdf_atomic_notif_head *head = &fpm_ctx->fpm_policy_event_notif_head;

	return qdf_register_atomic_notifier_chain(head, nb);
}

QDF_STATUS fpm_policy_event_unregister_notifier(struct wlan_dp_intf *dp_intf,
						qdf_notif_block *nb)
{
	struct fpm_table *fpm_ctx = dp_intf->fpm_ctx;
	qdf_atomic_notif_head *head = &fpm_ctx->fpm_policy_event_notif_head;

	return qdf_unregister_atomic_notifier_chain(head, nb);
}

bool fpm_is_tid_override(qdf_nbuf_t nbuf, uint8_t *tid)
{
	if (DP_IS_TID_OVERRIDE_TAG(nbuf->mark)) {
		*tid = DP_EXTRACT_TID(nbuf->mark);
		return true;
	}

	return false;
}

bool fpm_check_tid_override_tagged(qdf_nbuf_t nbuf)
{
	return DP_IS_TID_OVERRIDE_TAG(nbuf->mark);
}

static QDF_STATUS fpm_policy_event_notifier_call(struct fpm_table *fpm,
						 unsigned long v, void *data)
{
	return qdf_atomic_notfier_call(&fpm->fpm_policy_event_notif_head, v,
				       data);
}

static QDF_STATUS
fpm_policy_node_delete(struct fpm_table *fpm, struct dp_policy *policy)
{
	struct policy_notifier_data data = {0};

	data.flow = policy->flow;
	data.policy_id = policy->policy_id;
	data.prio = policy->prio;
	fpm_policy_event_notifier_call(fpm, FPM_POLICY_DEL, &data);

	qdf_hl_del_rcu(&policy->node);
	fpm->policy_id_bitmap &= ~BIT(policy->policy_id);
	fpm->policy_count--;

	if (policy->flags & DP_POLICY_TO_SVC_MAP)
		dp_svc_dec_policy_ref_cnt_by_id(policy->svc_id);

	return QDF_STATUS_SUCCESS;
}

static void fpm_free_cb(qdf_rcu_head_t *rp)
{
	struct dp_policy *fn = container_of(rp, struct dp_policy, rcu);

	qdf_mem_free(fn);
}

static inline QDF_STATUS
fpm_hash_table_delete_node(struct fpm_table *fpm, uint64_t policy_id)
{
	uint16_t hash_idx;
	struct dp_policy *policy = NULL;
	struct qdf_ht *lhead;
	int ret = QDF_STATUS_E_INVAL;
	struct qdf_ht_entry *tmp;

	qdf_spin_lock_bh(&fpm->lock);
	for (hash_idx = 0; hash_idx < DP_FLOW_PRIO_MAX; hash_idx++) {
		lhead = &fpm->policy_tab[hash_idx];
		qdf_hl_for_each_entry_safe(policy, tmp, lhead, node) {
			if (policy_id == policy->policy_id) {
				ret = fpm_policy_node_delete(fpm, policy);
				qdf_call_rcu(&policy->rcu, fpm_free_cb);
			}
		}
	}
	qdf_spin_unlock_bh(&fpm->lock);

	return ret;
}

static inline void
fpm_hash_table_delete_all_node(struct fpm_table *fpm)
{
	uint16_t hash_idx;
	struct dp_policy *policy = NULL;
	struct qdf_ht *lhead;
	struct qdf_ht_entry *tmp;

	qdf_spin_lock_bh(&fpm->lock);

	for (hash_idx = 0; hash_idx < DP_FLOW_PRIO_MAX; hash_idx++) {
		lhead = &fpm->policy_tab[hash_idx];
		qdf_hl_for_each_entry_safe(policy, tmp, lhead, node) {
			fpm_policy_node_delete(fpm, policy);
			qdf_call_rcu(&policy->rcu, fpm_free_cb);
		}
	}

	qdf_spin_unlock_bh(&fpm->lock);
}

static void fpm_hash_table_init(struct qdf_ht *hl_arr, uint32_t len)
{
	uint32_t idx;

	for (idx = 0; idx < len; idx++)
		qdf_hl_init(&hl_arr[idx]);
}

static QDF_STATUS fpm_policy_attach(struct fpm_table *fpm)
{
	qdf_spinlock_create(&fpm->lock);
	fpm_hash_table_init(fpm->policy_tab, ARRAY_SIZE(fpm->policy_tab));
	fpm_policy_event_notifier_init(fpm);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_fpm_init(struct wlan_dp_intf *dp_intf)
{
	struct fpm_table *fpm_ctx;

	fpm_ctx = (struct fpm_table *)qdf_mem_malloc(sizeof(*fpm_ctx));
	if (!fpm_ctx)
		return QDF_STATUS_E_NOMEM;
	fpm_policy_attach(fpm_ctx);
	dp_intf->fpm_ctx = fpm_ctx;
	dp_info("init success");

	return QDF_STATUS_SUCCESS;
}

static void fpm_policy_detach(struct fpm_table *fpm)
{
	fpm_hash_table_delete_all_node(fpm);
	qdf_spinlock_destroy(&fpm->lock);
}

void dp_fpm_deinit(struct wlan_dp_intf *dp_intf)
{
	fpm_policy_detach(dp_intf->fpm_ctx);
	qdf_mem_free(dp_intf->fpm_ctx);
	dp_intf->fpm_ctx = NULL;
	dp_info("deinit success");
}

static QDF_STATUS fpm_policy_update_map(struct dp_policy *policy,
					struct dp_policy *new_policy)
{
	if (policy->flags & DP_POLICY_TO_TID_MAP)
		policy->target_tid = new_policy->target_tid;

	if (policy->flags & DP_POLICY_TO_SVC_MAP) {
		if (dp_svc_inc_policy_ref_cnt_by_id(new_policy->svc_id) !=
		    QDF_STATUS_SUCCESS)
			return QDF_STATUS_E_INVAL;
		if (dp_svc_dec_policy_ref_cnt_by_id(policy->svc_id) !=
		    QDF_STATUS_SUCCESS) {
			dp_svc_dec_policy_ref_cnt_by_id(new_policy->svc_id);
			return QDF_STATUS_E_FAILURE;
		}
		policy->svc_id = new_policy->svc_id;
	}
	new_policy->policy_id = policy->policy_id;
	new_policy->prio = policy->prio;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS fpm_policy_update_with_policyid(struct fpm_table *fpm,
						  struct dp_policy *new_policy)
{
	struct dp_policy *policy = NULL;
	struct qdf_ht *lhead;
	uint16_t hash_idx;
	QDF_STATUS status = QDF_STATUS_E_NOENT;

	qdf_rcu_read_lock_bh();
	for (hash_idx = 0; hash_idx < DP_FLOW_PRIO_MAX; hash_idx++) {
		lhead = &fpm->policy_tab[hash_idx];

		qdf_hl_for_each_entry_rcu(policy, lhead, node) {
			if (policy->policy_id == new_policy->policy_id) {
				status = fpm_policy_update_map(policy,
							       new_policy);
				goto out;
			}
		}
	}
out:
	qdf_rcu_read_unlock_bh();
	return status;
}

QDF_STATUS fpm_policy_update(struct fpm_table *fpm, struct dp_policy *policy)
{
	struct policy_notifier_data data = {0};
	QDF_STATUS ret;

	if (!fpm) {
		dp_err("fpm_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!policy) {
		dp_err("policy is NULL");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&fpm->lock);
	ret = fpm_policy_update_with_policyid(fpm, policy);
	switch (ret) {
	case QDF_STATUS_E_NOENT:
		dp_err("no policy found with policy_id:%lx", policy->policy_id);
		break;
	case QDF_STATUS_E_INVAL:
		dp_err("new svc_id:%u is not valid", policy->svc_id);
		break;
	case QDF_STATUS_E_FAILURE:
		dp_err("svc_id update failed");
		break;
	default:
		data.flow = policy->flow;
		data.policy_id = policy->policy_id;
		data.prio = policy->prio;
		fpm_policy_event_notifier_call(fpm, FPM_POLICY_UPDATE, &data);
		break;
	}
	qdf_spin_unlock_bh(&fpm->lock);

	return ret;
}

bool fpm_flow_regex_match(struct flow_info *flow, struct flow_info *tflow)
{
	if (flow->flags & DP_FLOW_TUPLE_FLAGS_IPV4) {
		if (flow->flags & DP_FLOW_TUPLE_FLAGS_SRC_IP)
			if (flow->src_ip.ipv4_addr != tflow->src_ip.ipv4_addr)
				return false;
		if (flow->flags & DP_FLOW_TUPLE_FLAGS_DST_IP)
			if (flow->dst_ip.ipv4_addr != tflow->dst_ip.ipv4_addr)
				return false;
	} else if (flow->flags & DP_FLOW_TUPLE_FLAGS_IPV6) {
		if (flow->flags & DP_FLOW_TUPLE_FLAGS_SRC_IP)
			if (qdf_mem_cmp(flow->src_ip.ipv6_addr,
					tflow->src_ip.ipv6_addr,
					sizeof(struct in6_addr)))
				return false;
		if (flow->flags & DP_FLOW_TUPLE_FLAGS_DST_IP)
			if (qdf_mem_cmp(flow->dst_ip.ipv6_addr,
					tflow->dst_ip.ipv6_addr,
					sizeof(struct in6_addr)))
				return false;
	}

	if (flow->flags & DP_FLOW_TUPLE_FLAGS_SRC_PORT)
		if (flow->src_port != tflow->src_port)
			return false;
	if (flow->flags & DP_FLOW_TUPLE_FLAGS_DST_PORT)
		if (flow->dst_port != tflow->dst_port)
			return false;
	if (flow->flags & DP_FLOW_TUPLE_FLAGS_PROTO)
		if (flow->proto != tflow->proto)
			return false;

	return true;
}

static bool fpm_is_duplicate_policy(struct fpm_table *fpm,
				    struct dp_policy *tpolicy)
{
	struct dp_policy *policy = NULL;
	struct qdf_ht *lhead;
	uint16_t hash_idx;

	qdf_rcu_read_lock_bh();
	for (hash_idx = 0; hash_idx < DP_FLOW_PRIO_MAX; hash_idx++) {
		lhead = &fpm->policy_tab[hash_idx];

		qdf_hl_for_each_entry_rcu(policy, lhead, node) {
			if (tpolicy->flow.flags == policy->flow.flags &&
			    fpm_flow_regex_match(&tpolicy->flow,
						 &policy->flow)) {
				qdf_rcu_read_unlock_bh();
				return true;
			}
		}
	}
	qdf_rcu_read_unlock_bh();

	return false;
}

static inline void fpm_hash_table_insert_node(struct fpm_table *fpm,
					      uint8_t hash_idx,
					      struct dp_policy *policy)
{
	qdf_hl_add_head_rcu(&policy->node,
			    &fpm->policy_tab[hash_idx]);
}

QDF_STATUS fpm_policy_add(struct fpm_table *fpm, struct dp_policy *policy)
{
	struct dp_policy *new_policy;
	struct policy_notifier_data data = {0};
	uint64_t policy_id = DP_INVALID_ID;
	uint8_t i;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (!fpm) {
		dp_err("fpm_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!policy) {
		dp_err("policy is NULL");
		return QDF_STATUS_E_INVAL;
	}

	dp_info("new_policy:[flags:0x%x][prio:%u][policy_id:0x%lx]"
		"[src_ip:%x][dst_ip:%x][src_port:%u][dst_port:%u][proto:%u]"
		"[tid:%u][svc:%u]",
		policy->flags, policy->prio, policy->policy_id,
		policy->flow.src_ip.ipv4_addr, policy->flow.dst_ip.ipv4_addr,
		policy->flow.src_port, policy->flow.dst_port,
		policy->flow.proto, policy->target_tid, policy->svc_id);

	if (fpm->policy_count == DP_MAX_POLICY) {
		dp_err("count reached to max:%u", DP_MAX_POLICY);
		return QDF_STATUS_E_INVAL;
	}

	if (fpm_is_duplicate_policy(fpm, policy)) {
		dp_err("duplicate entry is not allowed");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&fpm->lock);
	for (i = 0; i < DP_MAX_POLICY; i++) {
		if (!(fpm->policy_id_bitmap & BIT(i))) {
			policy_id = i;
			fpm->policy_id_bitmap |= BIT(i);
			break;
		}
	}

	if (policy_id == DP_INVALID_ID) {
		ret = QDF_STATUS_E_INVAL;
		goto exit;
	}

	if (policy->flags & DP_POLICY_TO_SVC_MAP) {
		if (dp_svc_inc_policy_ref_cnt_by_id(policy->svc_id) !=
		    QDF_STATUS_SUCCESS) {
			dp_err("no service class found with svc_id:%d",
			       policy->svc_id);
			fpm->policy_id_bitmap &= ~BIT(policy_id);
			ret = QDF_STATUS_E_INVAL;
			goto exit;
		}
	}
	policy->policy_id = policy_id;

	new_policy = (struct dp_policy *)
				qdf_mem_malloc(sizeof(struct dp_policy));
	if (!new_policy) {
		dp_err("failed to allocate memory for new policy");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_copy(new_policy, policy, sizeof(struct dp_policy));
	fpm_hash_table_insert_node(fpm, policy->prio, new_policy);
	fpm->policy_count++;

	data.flow = new_policy->flow;
	data.policy_id = new_policy->policy_id;
	data.prio = new_policy->prio;
	fpm_policy_event_notifier_call(fpm, FPM_POLICY_ADD, &data);

exit:
	qdf_spin_unlock_bh(&fpm->lock);
	return ret;
}

QDF_STATUS fpm_policy_rem(struct fpm_table *fpm, uint64_t policy_id)
{
	if (!fpm) {
		dp_err("fpm_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return fpm_hash_table_delete_node(fpm, policy_id);
}

uint8_t fpm_policy_get(struct fpm_table *fpm, struct dp_policy *policies,
		       uint8_t max_count)
{
	struct dp_policy *policy = NULL;
	struct qdf_ht *lhead;
	uint16_t hash_idx;
	uint8_t count = 0;

	if (!fpm) {
		dp_err("fpm_ctx is NULL");
		return count;
	}

	if (!policies) {
		dp_err("policies is NULL");
		return count;
	}

	qdf_rcu_read_lock_bh();
	for (hash_idx = 0; hash_idx < DP_FLOW_PRIO_MAX; hash_idx++) {
		lhead = &fpm->policy_tab[hash_idx];

		qdf_hl_for_each_entry_rcu(policy, lhead, node) {
			if (count + 1 < max_count) {
				qdf_mem_copy(&policies[count++], policy,
					     sizeof(struct dp_policy));
			}
		}
	}
	qdf_rcu_read_unlock_bh();

	return count;
}

static enum dp_fpm_status fpm_get_metadata(struct dp_policy *policy,
					   qdf_nbuf_t skb,
					   uint32_t *metadata)
{
	QDF_STATUS status;

	if (policy->flags & DP_POLICY_TO_SVC_MAP) {
		status = dp_svc_get_meta_data_by_id(policy->svc_id, skb,
						    metadata);
		if (status == QDF_STATUS_SUCCESS)
			return FLOW_MATCH_FOUND;
	} else if (policy->flags & DP_POLICY_TO_TID_MAP) {
		*metadata = DP_PREPARE_TID_METADATA(policy->target_tid);
		return FLOW_MATCH_FOUND;
	}

	return FLOW_MATCH_DO_NOT_FOUND;
}

enum dp_fpm_status
fpm_policy_flow_match(struct wlan_dp_intf *dp_intf, qdf_nbuf_t skb,
		      uint32_t *metadata, struct flow_info *flow,
		      uint32_t *policy_id)
{
	struct fpm_table *fpm;
	struct dp_policy *policy = NULL;
	struct qdf_ht *lhead;
	int16_t hash_idx;
	enum dp_fpm_status found = FLOW_MATCH_DO_NOT_FOUND;

	if (!dp_intf) {
		dp_err("dp_intf is null");
		return FLOW_MATCH_FAIL;
	}

	if (!dp_intf->fpm_ctx) {
		dp_err("fpm_ctx is null");
		return FLOW_MATCH_FAIL;
	}

	if (!flow) {
		dp_err("flow is null");
		return FLOW_MATCH_FAIL;
	}

	fpm = dp_intf->fpm_ctx;
	qdf_rcu_read_lock_bh();
	for (hash_idx = DP_FLOW_PRIO_MAX - 1; hash_idx >= 0; hash_idx--) {
		lhead = &fpm->policy_tab[hash_idx];

		qdf_hl_for_each_entry_rcu(policy, lhead, node) {
			if (fpm_flow_regex_match(&policy->flow, flow)) {
				dp_info("flow matched policy_id:%d",
					policy->policy_id);
				*policy_id = policy->policy_id;
				found = fpm_get_metadata(policy, skb, metadata);
				goto out;
			}
		}
	}
out:
	qdf_rcu_read_unlock_bh();
	return found;
}

void dp_fpm_display_policy(struct wlan_dp_intf *dp_intf)
{
	struct fpm_table *fpm;
	struct dp_policy *policy = NULL;
	struct qdf_ht *lhead;
	uint16_t hash_idx;

	if (!dp_intf) {
		dp_err("dp_intf is null");
		return;
	}

	if (!dp_intf->fpm_ctx) {
		dp_err("fpm_ctx is null");
		return;
	}

	fpm = dp_intf->fpm_ctx;
	qdf_rcu_read_lock_bh();
	for (hash_idx = 0; hash_idx < DP_FLOW_PRIO_MAX; hash_idx++) {
		lhead = &fpm->policy_tab[hash_idx];
		qdf_hl_for_each_entry_rcu(policy, lhead, node) {
			dp_info("policy_id:0x%llx prio:%u",
				policy->policy_id, policy->prio);
			dp_info("pflags:0x%x flow_flags:0x%x svc_id:%u tid:%u",
				policy->flags, policy->flow.flags,
				policy->svc_id, policy->target_tid);
			if (policy->flow.flags & DP_FLOW_TUPLE_FLAGS_IPV4) {
				if (policy->flow.flags &
				    DP_FLOW_TUPLE_FLAGS_SRC_IP)
					dp_info("src_ip:0x%x",
						policy->flow.src_ip.ipv4_addr);
				if (policy->flow.flags &
				    DP_FLOW_TUPLE_FLAGS_SRC_IP)
					dp_info("dst_ip:0x%x",
						policy->flow.dst_ip.ipv4_addr);
				if (policy->flow.flags &
				    DP_FLOW_TUPLE_FLAGS_SRC_IP)
					dp_info("srcport:%u",
						policy->flow.src_port);
				if (policy->flow.flags &
				    DP_FLOW_TUPLE_FLAGS_SRC_IP)
					dp_info("dstport:%u",
						policy->flow.dst_port);
			}
		}
	}
	qdf_rcu_read_unlock_bh();
}
