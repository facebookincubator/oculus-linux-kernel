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

#include "os_if_dp_svc.h"
#include "wlan_cfg80211.h"
#include "qdf_trace.h"
#include "wlan_dp_ucfg_api.h"
#include "dp_types.h"

/* Buffer latency tolence range in ms */
#define MIN_BUF_LAT_TOLERENCE 1
#define MAX_BUF_LAT_TOLERENCE 1000

const struct nla_policy
set_service_class_cmd_policy[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS] = { .type = NLA_NESTED },
};

const struct nla_policy
set_service_class_params_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BUFFER_LATENCY_TOLERANCE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_TRIGGER_DSCP] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_REPLACE_DSCP] = { .type = NLA_U8 },
};

static bool is_lapb_enabled(void)
{
	struct dp_soc *soc = (struct dp_soc *)cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_cfg_dp_soc_ctxt *cfg;

	if (!soc)
		return false;

	cfg = soc->wlan_cfg_ctx;
	return wlan_cfg_is_lapb_enabled(cfg);
}

static QDF_STATUS svc_validate_data(struct dp_svc_data *svc_data)
{
	if (!(svc_data->flags & (DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE |
				 DP_SVC_FLAGS_APP_IND_DEF_DSCP |
				 DP_SVC_FLAGS_APP_IND_SPL_DSCP)))
		return QDF_STATUS_E_INVAL;

	if (svc_data->flags & DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE) {
		if (!is_lapb_enabled())
			return QDF_STATUS_E_INVAL;

		if ((svc_data->buffer_latency_tolerance < MIN_BUF_LAT_TOLERENCE) ||
		    (svc_data->buffer_latency_tolerance > MAX_BUF_LAT_TOLERENCE))
			return QDF_STATUS_E_INVAL;
	}

	if (svc_data->flags & DP_SVC_FLAGS_APP_IND_DEF_DSCP) {
		if (!is_lapb_enabled())
			return QDF_STATUS_E_INVAL;
		if (!(svc_data->flags & DP_SVC_FLAGS_APP_IND_SPL_DSCP) ||
		    !(svc_data->flags & DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE))
			return QDF_STATUS_E_INVAL;
		if (svc_data->app_ind_default_dscp >= 64)
			return QDF_STATUS_E_INVAL;
	}

	if (svc_data->flags & DP_SVC_FLAGS_APP_IND_SPL_DSCP) {
		if (!is_lapb_enabled())
			return QDF_STATUS_E_INVAL;

		if (!(svc_data->flags & DP_SVC_FLAGS_APP_IND_DEF_DSCP) ||
		    !(svc_data->flags & DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE))
			return QDF_STATUS_E_INVAL;

		if (svc_data->app_ind_special_dscp >= 64)
			return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
os_if_dp_service_class_set_cmd(struct nlattr *svc_params[])
{
	uint32_t cmd_id;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	struct dp_svc_data svc_data;
	int ret;

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS;
	if (!svc_params[cmd_id]) {
		osif_err("service parameters not available");
		return QDF_STATUS_E_INVAL;
	}

	ret = wlan_cfg80211_nla_parse_nested(tb,
					     QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
					     svc_params[cmd_id],
					     set_service_class_params_policy);
	if (ret) {
		osif_err("nla parse failed");
		return QDF_STATUS_E_INVAL;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID;
	if (!tb[cmd_id]) {
		osif_err("service class id is missing");
		return QDF_STATUS_E_INVAL;
	}

	svc_data.svc_id = nla_get_u8(tb[cmd_id]);
	svc_data.flags |= DP_SVC_FLAGS_SVC_ID;

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BUFFER_LATENCY_TOLERANCE;
	if (tb[cmd_id]) {
		svc_data.buffer_latency_tolerance = nla_get_u32(tb[cmd_id]);
		svc_data.flags |= DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_TRIGGER_DSCP;
	if (tb[cmd_id]) {
		svc_data.app_ind_default_dscp =	nla_get_u8(tb[cmd_id]);
		svc_data.flags |= DP_SVC_FLAGS_APP_IND_DEF_DSCP;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_REPLACE_DSCP;
	if (tb[cmd_id]) {
		svc_data.app_ind_special_dscp =	nla_get_u8(tb[cmd_id]);
		svc_data.flags |= DP_SVC_FLAGS_APP_IND_SPL_DSCP;
	}

	status = svc_validate_data(&svc_data);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Invalid input data");
		return status;
	}

	return ucfg_dp_svc_add(&svc_data);
}

static inline QDF_STATUS
os_if_dp_service_class_del_cmd(struct nlattr *svc_params[])
{
	uint32_t cmd_id;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	uint8_t svc_id = DP_SVC_INVALID_ID;
	int ret;

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS;
	if (!svc_params[cmd_id]) {
		osif_err("service parameters not available");
		return QDF_STATUS_E_INVAL;
	}

	ret = wlan_cfg80211_nla_parse_nested(tb,
					     QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
					     svc_params[cmd_id],
					     set_service_class_params_policy);
	if (ret) {
		osif_err("nla parse failed");
		return QDF_STATUS_E_INVAL;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID;
	if (!tb[cmd_id]) {
		osif_err("service_id not present");
		return QDF_STATUS_E_INVAL;
	}

	svc_id = nla_get_u8(tb[cmd_id]);

	return ucfg_dp_svc_remove(svc_id);
}

static inline QDF_STATUS
os_if_dp_service_class_get_cmd(struct wiphy *wiphy, struct nlattr *svc_params[])
{
	uint32_t cmd_id;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	uint8_t svc_id = DP_SVC_INVALID_ID;
	struct dp_svc_data svc_table[DP_SVC_ARRAY_SIZE] = {0};
	uint8_t i, count;
	struct sk_buff *skb;
	struct nlattr *svc_info;
	struct nlattr *svc_params_resp;
	int ret;

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS;
	if (svc_params[cmd_id]) {
		ret = wlan_cfg80211_nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
						     svc_params[cmd_id],
						     set_service_class_params_policy);
		if (ret) {
			osif_err("nla parse failed");
			return QDF_STATUS_E_INVAL;
		}

		cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID;
		if (tb[cmd_id])
			svc_id = nla_get_u8(tb[cmd_id]);
	}

	count = ucfg_dp_svc_get(svc_id, svc_table, DP_SVC_ARRAY_SIZE);
	if (!count && (svc_id != DP_SVC_INVALID_ID)) {
		osif_err("No service class found");
		return QDF_STATUS_E_INVAL;
	}

	skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
						       NLMSG_DEFAULT_SIZE);
	if (!skb) {
		osif_err("alloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	svc_params_resp = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS);
	if (!svc_params_resp) {
		wlan_cfg80211_vendor_free_skb(skb);
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < count; i++) {
		svc_info = nla_nest_start(skb, i);

		nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID, svc_table[i].svc_id);
		if (svc_table[i].flags & DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE)
			nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BUFFER_LATENCY_TOLERANCE,
				    svc_table[i].buffer_latency_tolerance);
		if (svc_table[i].flags & DP_SVC_FLAGS_APP_IND_DEF_DSCP)
			nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_TRIGGER_DSCP,
				   svc_table[i].app_ind_default_dscp);
		if (svc_table[i].flags & DP_SVC_FLAGS_APP_IND_SPL_DSCP)
			nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_REPLACE_DSCP,
				   svc_table[i].app_ind_special_dscp);

		nla_nest_end(skb, svc_info);
	}

	nla_nest_end(skb, svc_params_resp);
	wlan_cfg80211_vendor_cmd_reply(skb);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
os_if_dp_service_class_cmd(struct wiphy *wiphy, const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1];
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t cmd_id;
	uint8_t op_type;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX,
				    data, data_len,
				    set_service_class_cmd_policy)) {
		osif_err("Invalid service class attr");
		return QDF_STATUS_E_INVAL;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION;
	if (!tb[cmd_id]) {
		osif_err("operation not specified");
		return QDF_STATUS_E_INVAL;
	}

	op_type = nla_get_u8(tb[cmd_id]);
	switch (op_type) {
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_SET:
		status = os_if_dp_service_class_set_cmd(tb);
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_DEL:
		status = os_if_dp_service_class_del_cmd(tb);
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_GET:
		status = os_if_dp_service_class_get_cmd(wiphy, tb);
		break;
	default:
		osif_err("Invalid operation type");
		return QDF_STATUS_E_INVAL;
	}

	return status;
}
