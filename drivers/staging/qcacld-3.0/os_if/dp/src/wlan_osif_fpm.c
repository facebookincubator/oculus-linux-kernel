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

/**
 * DOC: wlan_osif_fpm.c
 *
 * WLAN Host Device Driver fpm implementation
 */

#include "wlan_hdd_main.h"
#include "osif_vdev_sync.h"
#include "wlan_osif_fpm.h"
#include "wlan_dp_ucfg_api.h"

const
struct nla_policy fpm_policy[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_OPERATION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_ID] = {.type = NLA_U64},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_LIST] = {.type = NLA_NESTED},
};

const
struct nla_policy
fpm_policy_param[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_POLICY_TYPE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_PRIORITY] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV4_SRC_IP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV6_SRC_IP] =
		NLA_POLICY_EXACT_LEN(sizeof(struct in6_addr)),
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_SRC_PORT] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV4_DST_IP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV6_DST_IP] =
		NLA_POLICY_EXACT_LEN(sizeof(struct in6_addr)),
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_DST_PORT] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_PROTO] = {.type = NLA_U8},
};

const
struct nla_policy
fpm_policy_config[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_TID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_SERVICE_CLASS_ID] = {.type = NLA_U8},
};

static QDF_STATUS
osif_fpm_convert_proto_uton(enum qca_wlan_vendor_flow_policy_proto qca_proto,
			    uint8_t *proto)
{
	switch (qca_proto) {
	case QCA_WLAN_VENDOR_FLOW_POLICY_PROTO_UDP:
		*proto = IPPROTO_UDP;
		break;
	case QCA_WLAN_VENDOR_FLOW_POLICY_PROTO_TCP:
		*proto = IPPROTO_TCP;
		break;
	default:
		osif_err("invalid user proto");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
osif_fpm_convert_proto_ntou(uint8_t proto, uint8_t *qca_proto)
{
	switch (proto) {
	case IPPROTO_UDP:
		*qca_proto = QCA_WLAN_VENDOR_FLOW_POLICY_PROTO_UDP;
		break;
	case IPPROTO_TCP:
		*qca_proto = QCA_WLAN_VENDOR_FLOW_POLICY_PROTO_TCP;
		break;
	default:
		osif_err("invalid user proto");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static int osif_fpm_parse_policy_params(struct nlattr **tb,
					struct dp_policy *policy)
{
	enum qca_wlan_vendor_flow_policy_type policy_type;
	uint32_t attr_id;
	QDF_STATUS status;

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_PRIORITY;
	if (tb[attr_id])
		policy->prio = nla_get_u8(tb[attr_id]);
	else
		policy->prio = DP_FLOW_PRIO_DEF;

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_POLICY_TYPE;
	if (tb[attr_id]) {
		policy_type = nla_get_u8(tb[attr_id]);
	} else {
		osif_err("fpm: invalid policy type");
		return -EINVAL;
	}

	switch (policy_type) {
	case QCA_WLAN_VENDOR_FLOW_POLICY_TYPE_IPV4:
		policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_IPV4;
		attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV4_SRC_IP;
		if (tb[attr_id]) {
			policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_SRC_IP;
			policy->flow.src_ip.ipv4_addr =
					 nla_get_u32(tb[attr_id]);
		}

		attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV4_DST_IP;
		if (tb[attr_id]) {
			policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_DST_IP;
			policy->flow.dst_ip.ipv4_addr =
					 nla_get_u32(tb[attr_id]);
		}
		break;
	case QCA_WLAN_VENDOR_FLOW_POLICY_TYPE_IPV6:
		policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_IPV6;
		attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV6_SRC_IP;
		if (tb[attr_id]) {
			struct in6_addr saddr;

			saddr = nla_get_in6_addr(tb[attr_id]);
			policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_SRC_IP;
			memcpy(policy->flow.src_ip.ipv6_addr,
			       saddr.s6_addr32,
			       sizeof(struct in6_addr));
		}

		attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV6_DST_IP;
		if (tb[attr_id]) {
			struct in6_addr saddr;

			saddr = nla_get_in6_addr(tb[attr_id]);
			policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_DST_IP;
			memcpy(policy->flow.dst_ip.ipv6_addr,
			       saddr.s6_addr32,
			       sizeof(struct in6_addr));
		}
		break;
	default:
		osif_err("fpm: invalid policy type");
		return -EINVAL;
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_SRC_PORT;
	if (tb[attr_id]) {
		policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_SRC_PORT;
		policy->flow.src_port = nla_get_u16(tb[attr_id]);
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_DST_PORT;
	if (tb[attr_id]) {
		policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_DST_PORT;
		policy->flow.dst_port = nla_get_u16(tb[attr_id]);
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_PROTO;
	if (tb[attr_id]) {
		status = osif_fpm_convert_proto_uton(nla_get_u8(tb[attr_id]),
						     &policy->flow.proto);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			policy->flow.flags |= DP_FLOW_TUPLE_FLAGS_PROTO;
		} else {
			osif_err("fpm: invalid proto");
			return -EINVAL;
		}
	}
	return 0;
}

static int osif_fpm_update(struct fpm_table *fpm,
			   struct nlattr *fpm_policy_id_attr,
			   struct nlattr *fpm_config_attr)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_MAX + 1];
	struct dp_policy policy = {0};
	uint32_t attr_id;
	int ret;
	QDF_STATUS status;

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_ID;
	if (fpm_policy_id_attr) {
		policy.policy_id = nla_get_u64(fpm_policy_id_attr);
	} else {
		osif_err("fpm: policy_id is mandatory for update");
		return -EINVAL;
	}

	ret = wlan_cfg80211_nla_parse_nested(tb,
					     QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_MAX,
					     fpm_config_attr,
					     fpm_policy_config);
	if (ret) {
		osif_err("fpm: nla_parse_nested failed");
		return ret;
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_TID;
	if (tb[attr_id]) {
		policy.flags |= DP_POLICY_TO_TID_MAP;
		policy.target_tid = nla_get_u8(tb[attr_id]);
		if (policy.target_tid >= MAX_TID) {
			osif_err("fpm: invalid tid value");
			return -EINVAL;
		}
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_SERVICE_CLASS_ID;
	if (tb[attr_id]) {
		policy.flags |= DP_POLICY_TO_SVC_MAP;
		policy.svc_id = nla_get_u8(tb[attr_id]);
	}

	if (!policy.flags) {
		osif_err("fpm: nothing to update");
		return -EINVAL;
	}

	status = ucfg_fpm_policy_update(fpm, &policy);
	if (status != QDF_STATUS_SUCCESS)
		return qdf_status_to_os_return(status);

	osif_info("fpm: policy-update: [flags:0x%x][prio:%d][policy_id:0x%lx][src_ip:%d][dst_ip:%d][src_port:%d][dst_port:%d][proto:%d][tid:%d][svc:%d]",
		  policy.flags, policy.prio, policy.policy_id,
		  policy.flow.src_ip.ipv4_addr,
		  policy.flow.dst_ip.ipv4_addr,
		  policy.flow.src_port, policy.flow.dst_port,
		  policy.flow.proto, policy.target_tid, policy.svc_id);

	return 0;
}

static int osif_fpm_add(struct fpm_table *fpm, struct nlattr *fpm_param_attr,
			struct nlattr *fpm_config_attr, uint64_t *policy_id)
{
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_MAX + 1];
	struct nlattr *tb3[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_MAX + 1];
	struct dp_policy policy = {0};
	uint32_t attr_id;
	int ret;
	QDF_STATUS status;

	ret = wlan_cfg80211_nla_parse_nested(tb2,
					     QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_MAX,
					     fpm_param_attr,
					     fpm_policy_param);
	if (ret) {
		osif_err("fpm: nla_parse_nested failed");
		return ret;
	}

	ret = osif_fpm_parse_policy_params(tb2, &policy);
	if (ret)
		return ret;

	ret = wlan_cfg80211_nla_parse_nested(tb3,
					     QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_MAX,
					     fpm_config_attr,
					     fpm_policy_config);
	if (ret) {
		osif_err("fpm: nla_parse_nested failed");
		return ret;
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_TID;
	if (tb3[attr_id]) {
		policy.flags |= DP_POLICY_TO_TID_MAP;
		policy.target_tid = nla_get_u8(tb3[attr_id]);
		if (policy.target_tid >= MAX_TID) {
			osif_err("fpm: invalid tid value");
			return -EINVAL;
		}
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_SERVICE_CLASS_ID;
	if (tb3[attr_id]) {
		policy.flags |= DP_POLICY_TO_SVC_MAP;
		policy.svc_id = nla_get_u8(tb3[attr_id]);
	}

	if (!policy.flags) {
		osif_err("fpm: no configs associated for given flow");
		return -EINVAL;
	}

	status = ucfg_fpm_policy_add(fpm, &policy);
	if (status != QDF_STATUS_SUCCESS)
		return qdf_status_to_os_return(status);

	osif_info("fpm: policy-add: [flags:0x%x][prio:%d][policy_id:0x%lx][src_ip:%d][dst_ip:%d][src_port:%d][dst_port:%d][proto:%d][tid:%d][svc:%d]",
		  policy.flags, policy.prio, policy.policy_id,
		  policy.flow.src_ip.ipv4_addr, policy.flow.dst_ip.ipv4_addr,
		  policy.flow.src_port, policy.flow.dst_port, policy.flow.proto,
		  policy.target_tid, policy.svc_id);
	*policy_id = policy.policy_id;

	return 0;
}

static int osif_send_policy_id(struct wiphy *wiphy, uint64_t policy_id)
{
	struct sk_buff *skb;
	uint32_t attr;

	skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
						       NLMSG_DEFAULT_SIZE);
	if (!skb) {
		osif_err("alloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_ID;
	if (wlan_cfg80211_nla_put_u64(skb, attr, policy_id)) {
		osif_err("fpm: nla_put_u8 failed");
		goto put_failure;
	}
	return wlan_cfg80211_vendor_cmd_reply(skb);

put_failure:
	wlan_cfg80211_vendor_free_skb(skb);
	return -EMSGSIZE;
}

static int osif_fpm_del(struct fpm_table *fpm, struct nlattr **fpm_attr)
{
	uint64_t policy_id;
	uint8_t attr_id;
	QDF_STATUS status;

	attr_id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_ID;
	if (!fpm_attr[attr_id]) {
		osif_err("fpm: policy_id not present");
		return -EINVAL;
	}

	policy_id = nla_get_u64(fpm_attr[attr_id]);

	status = ucfg_fpm_policy_rem(fpm, policy_id);
	return qdf_status_to_os_return(status);
}

static int osif_fpm_query(struct fpm_table *fpm, struct wiphy *wiphy)
{
	struct dp_policy *policy;
	struct nlattr *tab_attr, *entry_attr, *config_attr;
	uint8_t proto;
	struct sk_buff *skb = NULL;
	uint32_t attr;
	int count, i;

	skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
						       NLMSG_DEFAULT_SIZE);
	if (!skb) {
		osif_err("nl reply skb alloc failed");
		return -ENOMEM;
	}

	policy = qdf_mem_malloc(sizeof(struct dp_policy) * DP_MAX_POLICY);
	if (!policy) {
		osif_err("policy memory alloc failed");
		wlan_cfg80211_vendor_free_skb(skb);
		return -ENOMEM;
	}

	count = ucfg_fpm_policy_get(fpm, policy, DP_MAX_POLICY);
	if (!count)
		goto nl_done;

	attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_LIST;
	tab_attr = nla_nest_start(skb, attr);
	if (!tab_attr) {
		osif_err("fpm: nla_nest_start error");
		goto free_skb;
	}

	for (i = 0; i < count; i++) {
		entry_attr = nla_nest_start(skb, i + 1);
		if (!entry_attr)
			goto free_skb;

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_ID;
		if (wlan_cfg80211_nla_put_u64(skb, attr, policy[i].policy_id)) {
			osif_err("fpm: nla_put_u8 failed");
			goto free_skb;
		}

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM;
		config_attr = nla_nest_start(skb, attr);
		if (!config_attr) {
			osif_err("fpm: nla_nest_start error");
			goto free_skb;
		}

		if (policy[i].flow.flags & DP_FLOW_TUPLE_FLAGS_IPV4) {
			attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_POLICY_TYPE;
			if (nla_put_u8(skb, attr, QCA_WLAN_VENDOR_FLOW_POLICY_TYPE_IPV4)) {
				osif_err("fpm: nla_put_u8 failed");
				goto free_skb;
			}
			attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV4_SRC_IP;
			if ((policy[i].flow.flags &
			     DP_FLOW_TUPLE_FLAGS_SRC_IP) &&
			    nla_put_u32(skb, attr,
					policy[i].flow.src_ip.ipv4_addr)) {
				osif_err("fpm: nla_put_u8 failed");
				goto free_skb;
			}
			attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV4_DST_IP;
			if ((policy[i].flow.flags &
			     DP_FLOW_TUPLE_FLAGS_DST_IP) &&
			    nla_put_u32(skb, attr,
					policy[i].flow.dst_ip.ipv4_addr)) {
				osif_err("fpm: nla_put_u8 failed");
				goto free_skb;
			}
		} else if (policy[i].flow.flags & DP_FLOW_TUPLE_FLAGS_IPV6) {
			attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_POLICY_TYPE;
			if (nla_put_u8(skb, attr, QCA_WLAN_VENDOR_FLOW_POLICY_TYPE_IPV6)) {
				osif_err("fpm: nla_put_u8 failed");
				goto free_skb;
			}
			attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV6_SRC_IP;
			if ((policy[i].flow.flags &
			     DP_FLOW_TUPLE_FLAGS_SRC_IP) &&
			    nla_put_in6_addr(skb, attr,
					     (struct in6_addr *)
					     policy[i].flow.src_ip.ipv6_addr)) {
				osif_err("fpm: nla_put_u8 failed");
				goto free_skb;
			}
			attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_IPV6_DST_IP;
			if ((policy[i].flow.flags &
			     DP_FLOW_TUPLE_FLAGS_DST_IP) &&
			    nla_put_in6_addr(skb, attr,
					     (struct in6_addr *)
					     policy[i].flow.dst_ip.ipv6_addr)) {
				osif_err("fpm: nla_put_u8 failed");
				goto free_skb;
			}
		}

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_SRC_PORT;
		if ((policy[i].flow.flags & DP_FLOW_TUPLE_FLAGS_SRC_PORT) &&
		    nla_put_u16(skb, attr, policy[i].flow.src_port)) {
			osif_err("fpm: nla_put_u8 failed");
			goto free_skb;
		}

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_DST_PORT;
		if ((policy[i].flow.flags & DP_FLOW_TUPLE_FLAGS_DST_PORT) &&
		    nla_put_u16(skb, attr, policy[i].flow.dst_port)) {
			osif_err("fpm: nla_put_u8 failed");
			goto free_skb;
		}

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_PRIORITY;
		if (nla_put_u8(skb, attr, policy[i].prio)) {
			osif_err("fpm: nla_put_u8 failed");
			goto free_skb;
		}

		if (policy[i].flow.flags & DP_FLOW_TUPLE_FLAGS_PROTO) {
			attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_PROTO;
			if (QDF_STATUS_SUCCESS ==
			    osif_fpm_convert_proto_ntou(policy[i].flow.proto,
							&proto) &&
					nla_put_u8(skb, attr, proto)) {
				osif_err("fpm: nla_put_u8 failed");
				goto free_skb;
			}
		}
		nla_nest_end(skb, config_attr);

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG;
		config_attr = nla_nest_start(skb, attr);
		if (!config_attr) {
			osif_err("fpm: nla_nest_start error");
			goto free_skb;
		}

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_TID;
		if ((policy[i].flags & DP_POLICY_TO_TID_MAP) &&
		    nla_put_u8(skb, attr, policy[i].target_tid)) {
			osif_err("fpm: nla_put_u8 failed");
			goto free_skb;
		}

		attr = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_SERVICE_CLASS_ID;
		if ((policy[i].flags & DP_POLICY_TO_SVC_MAP) &&
		    nla_put_u8(skb, attr, policy[i].svc_id)) {
			osif_err("fpm: nla_put_u8 failed");
			goto free_skb;
		}
		nla_nest_end(skb, config_attr);

		nla_nest_end(skb, entry_attr);
	}

	nla_nest_end(skb, tab_attr);
nl_done:
	qdf_mem_free(policy);
	return wlan_cfg80211_vendor_cmd_reply(skb);

free_skb:
	qdf_mem_free(policy);
	wlan_cfg80211_vendor_free_skb(skb);
	return -EMSGSIZE;
}

static int hdd_fpm_configure(struct wiphy *wiphy, struct hdd_adapter *adapter,
			     struct nlattr **tb)
{
	enum qca_wlan_vendor_flow_policy_operation fpm_oper;
	struct nlattr *fpm_oper_attr;
	struct nlattr *fpm_param_attr;
	struct nlattr *fpm_config_attr;
	struct fpm_table *fpm_ctx;
	struct nlattr *fpm_policyid_attr;
	uint64_t policy_id;
	uint32_t id;
	int ret;

	id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_OPERATION;
	fpm_oper_attr = tb[id];

	if (!fpm_oper_attr) {
		hdd_err("TWT operation NOT specified");
		return -EINVAL;
	}

	fpm_oper = nla_get_u8(fpm_oper_attr);

	id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM;
	fpm_param_attr = tb[id];

	id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_ID;
	fpm_policyid_attr = tb[id];

	id = QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG;
	fpm_config_attr = tb[id];

	fpm_ctx = ucfg_fpm_policy_get_ctx_by_vdev(adapter->deflink->vdev);
	if (!fpm_ctx) {
		osif_err("fpm: fpm_ctx NULL");
		return -EINVAL;
	}
	osif_debug("fpm Operation 0x%x", fpm_oper);

	switch (fpm_oper) {
	case QCA_WLAN_VENDOR_FLOW_POLICY_OPER_ADD:
		if (!fpm_param_attr || !fpm_config_attr) {
			hdd_err("fpm parameters NOT specified");
			return -EINVAL;
		}
		ret = osif_fpm_add(fpm_ctx, fpm_param_attr, fpm_config_attr,
				   &policy_id);
		if (ret)
			return -EINVAL;
		return osif_send_policy_id(wiphy, policy_id);
	case QCA_WLAN_VENDOR_FLOW_POLICY_OPER_UPDATE:
		if (!fpm_policyid_attr || !fpm_config_attr) {
			hdd_err("fpm parameters NOT specified");
			return -EINVAL;
		}
		return osif_fpm_update(fpm_ctx, fpm_policyid_attr,
				       fpm_config_attr);
	case QCA_WLAN_VENDOR_FLOW_POLICY_OPER_DELETE:
		return osif_fpm_del(fpm_ctx, tb);
	case QCA_WLAN_VENDOR_FLOW_POLICY_OPER_QUERY:
		return osif_fpm_query(fpm_ctx, wiphy);
	default:
		osif_err("Invalid FPM Operation");
		return -EINVAL;
	}
}

/**
 * __wlan_hdd_cfg80211_vendor_fpm() - API to process venor fpm request
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to net device
 * @data : Pointer to the data
 * @data_len : length of the data
 *
 * API to process venor fpm request.
 *
 * Return: return 0 on success and negative error code on failure
 */
static int __wlan_hdd_cfg80211_vendor_fpm(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_MAX + 1];
	int errno;

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_MAX,
				    data,
				    data_len,
				    fpm_policy)) {
		hdd_err("invalid fpm attr");
		return -EINVAL;
	}

	errno = hdd_fpm_configure(wiphy, adapter, tb);

	return errno;
}

/**
 * wlan_hdd_cfg80211_vendor_fpm() -API to process venor fpm request
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wireless device
 * @data: Pointer to the data
 * @data_len: length of the data
 *
 * This is called from userspace to request fpm.
 *
 * Return: Return the Success or Failure code.
 */
int wlan_hdd_cfg80211_vendor_fpm(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_vendor_fpm(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
