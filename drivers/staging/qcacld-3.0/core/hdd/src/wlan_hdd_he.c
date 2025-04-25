/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_he.c
 *
 * WLAN Host Device Driver file for 802.11ax (High Efficiency) support.
 *
 */

#include "wlan_hdd_main.h"
#include "wlan_hdd_he.h"
#include "osif_sync.h"
#include "wma_he.h"
#include "wlan_utility.h"
#include "wlan_mlme_ucfg_api.h"
#include "spatial_reuse_ucfg_api.h"
#include "cdp_txrx_host_stats.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_hdd_object_manager.h"

const struct nla_policy
wlan_hdd_sr_policy[QCA_WLAN_VENDOR_ATTR_SR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SR_OPERATION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_SR_STATS] = {.type = NLA_NESTED},
};

static const struct nla_policy
qca_wlan_vendor_srp_param_policy[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE] = {
							.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_DISALLOW] = {
							.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET] = {
							.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET] = {
							.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET] = {
							.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_PD_THRESHOLD] = {
							.type = NLA_S32},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_PD_THRESHOLD] = {
							.type = NLA_S32},
	[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_REASON_CODE] = {.type = NLA_U32},

};

void hdd_update_tgt_he_cap(struct hdd_context *hdd_ctx,
			   struct wma_tgt_cfg *cfg)
{
	QDF_STATUS status;
	tDot11fIEhe_cap he_cap_ini = {0};
	uint8_t value = 0;

	status = ucfg_mlme_cfg_get_vht_tx_bfee_ant_supp(hdd_ctx->psoc,
							&value);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get tx_bfee_ant_supp");

	he_cap_ini.bfee_sts_lt_80 = value;
	sme_update_tgt_he_cap(hdd_ctx->mac_handle, cfg, &he_cap_ini);

	ucfg_mlme_update_tgt_he_cap(hdd_ctx->psoc, cfg);
}

void wlan_hdd_check_11ax_support(struct hdd_beacon_data *beacon,
				 struct sap_config *config)
{
	const uint8_t *ie;

	ie = wlan_get_ext_ie_ptr_from_ext_id(HE_CAP_OUI_TYPE, HE_CAP_OUI_SIZE,
					    beacon->tail, beacon->tail_len);
	if (ie)
		config->SapHw_mode = eCSR_DOT11_MODE_11ax;
}

int hdd_update_he_cap_in_cfg(struct hdd_context *hdd_ctx)
{
	uint32_t val;
	uint32_t val1 = 0;
	QDF_STATUS status;
	int ret;
	uint8_t enable_ul_ofdma, enable_ul_mimo;

	status = ucfg_mlme_cfg_get_he_ul_mumimo(hdd_ctx->psoc, &val);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get CFG_HE_UL_MUMIMO");
		return qdf_status_to_os_return(status);
	}

	/* In val,
	 * Bit 1 - corresponds to UL MIMO
	 * Bit 2 - corresponds to UL OFDMA
	 */
	ret = ucfg_mlme_cfg_get_enable_ul_mimo(hdd_ctx->psoc,
					       &enable_ul_mimo);
	if (ret)
		return ret;
	ret = ucfg_mlme_cfg_get_enable_ul_ofdm(hdd_ctx->psoc,
					       &enable_ul_ofdma);
	if (ret)
		return ret;
	if (val & 0x1 || (val >> 1) & 0x1)
		val1 = enable_ul_mimo & 0x1;

	if ((val >> 1) & 0x1)
		val1 |= ((enable_ul_ofdma & 0x1) << 1);

	ret = ucfg_mlme_cfg_set_he_ul_mumimo(hdd_ctx->psoc, val1);

	return ret;
}

/*
 * __wlan_hdd_cfg80211_get_he_cap() - get HE Capabilities
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
static int
__wlan_hdd_cfg80211_get_he_cap(struct wiphy *wiphy,
			       struct wireless_dev *wdev,
			       const void *data,
			       int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int ret;
	QDF_STATUS status;
	struct sk_buff *reply_skb;
	uint32_t nl_buf_len;
	struct he_capability he_cap;
	uint8_t he_supported = 0;

	hdd_enter();
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	nl_buf_len = NLMSG_HDRLEN;
	if (sme_is_feature_supported_by_fw(DOT11AX)) {
		he_supported = 1;

		status = wma_get_he_capabilities(&he_cap);
		if (QDF_STATUS_SUCCESS != status)
			return -EINVAL;
	} else {
		hdd_info("11AX: HE not supported, send only QCA_WLAN_VENDOR_ATTR_HE_SUPPORTED");
	}

	if (he_supported) {
		nl_buf_len += NLA_HDRLEN + sizeof(he_supported) +
			      NLA_HDRLEN + sizeof(he_cap.phy_cap) +
			      NLA_HDRLEN + sizeof(he_cap.mac_cap) +
			      NLA_HDRLEN + sizeof(he_cap.mcs) +
			      NLA_HDRLEN + sizeof(he_cap.ppet.numss_m1) +
			      NLA_HDRLEN + sizeof(he_cap.ppet.ru_bit_mask) +
			      NLA_HDRLEN +
				sizeof(he_cap.ppet.ppet16_ppet8_ru3_ru0);
	} else {
		nl_buf_len += NLA_HDRLEN + sizeof(he_supported);
	}

	hdd_info("11AX: he_supported: %d", he_supported);

	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy, nl_buf_len);
	if (!reply_skb) {
		hdd_err("Allocate reply_skb failed");
		return -EINVAL;
	}

	if (nla_put_u8(reply_skb,
		       QCA_WLAN_VENDOR_ATTR_HE_SUPPORTED, he_supported))
		goto nla_put_failure;

	/* No need to populate other attributes if HE is not supported */
	if (0 == he_supported)
		goto end;

	if (nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_MAC_CAPAB, he_cap.mac_cap) ||
	    nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_HE_MCS, he_cap.mcs) ||
	    nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_NUM_SS, he_cap.ppet.numss_m1) ||
	    nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_RU_IDX_MASK,
			he_cap.ppet.ru_bit_mask) ||
	    nla_put(reply_skb,
		    QCA_WLAN_VENDOR_ATTR_PHY_CAPAB,
		    sizeof(u32) * HE_MAX_PHY_CAP_SIZE, he_cap.phy_cap) ||
	    nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_PPE_THRESHOLD,
		    sizeof(u32) * PSOC_HOST_MAX_NUM_SS,
		    he_cap.ppet.ppet16_ppet8_ru3_ru0))
		goto nla_put_failure;
end:
	ret = wlan_cfg80211_vendor_cmd_reply(reply_skb);
	hdd_exit();
	return ret;

nla_put_failure:
	hdd_err("nla put fail");
	wlan_cfg80211_vendor_free_skb(reply_skb);
	return -EINVAL;
}

int wlan_hdd_cfg80211_get_he_cap(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_get_he_cap(wiphy, wdev, data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

#ifdef WLAN_FEATURE_SR
static QDF_STATUS
hdd_sr_event_convert_reason_code(enum sr_osif_reason_code sr_osif_rc,
				 enum qca_wlan_sr_reason_code *sr_nl_rc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	switch (sr_osif_rc) {
	case SR_REASON_CODE_ROAMING:
		*sr_nl_rc = QCA_WLAN_SR_REASON_CODE_ROAMING;
		break;
	case SR_REASON_CODE_CONCURRENCY:
		*sr_nl_rc = QCA_WLAN_SR_REASON_CODE_CONCURRENCY;
		break;
	default:
		status = QDF_STATUS_E_INVAL;
	}

	return status;
}

static QDF_STATUS
hdd_sr_event_convert_operation(enum sr_osif_operation sr_osif_oper,
			       enum qca_wlan_sr_operation *sr_nl_oper)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	switch (sr_osif_oper) {
	case SR_OPERATION_SUSPEND:
		*sr_nl_oper = QCA_WLAN_SR_OPERATION_SR_SUSPEND;
		break;
	case SR_OPERATION_RESUME:
		*sr_nl_oper = QCA_WLAN_SR_OPERATION_SR_RESUME;
		break;
	case SR_OPERATION_UPDATE_PARAMS:
		*sr_nl_oper = QCA_WLAN_SR_OPERATION_UPDATE_PARAMS;
		break;
	default:
		status = QDF_STATUS_E_INVAL;
	}

	return status;
}

static QDF_STATUS hdd_sr_pack_suspend_resume_event(
					 struct sk_buff *skb,
					 enum qca_wlan_sr_operation sr_nl_oper,
					 enum qca_wlan_sr_reason_code sr_nl_rc,
					 uint8_t srg_max_pd_offset,
					 uint8_t srg_min_pd_offset,
					 uint8_t non_srg_max_pd_offset)
{
	struct nlattr *attr;
	QDF_STATUS status = QDF_STATUS_E_FAULT;

	if (((sr_nl_rc != QCA_WLAN_SR_REASON_CODE_CONCURRENCY) &&
	     (sr_nl_rc != QCA_WLAN_SR_REASON_CODE_ROAMING)) ||
	    ((sr_nl_oper != QCA_WLAN_SR_OPERATION_SR_SUSPEND) &&
	     (sr_nl_oper != QCA_WLAN_SR_OPERATION_SR_RESUME))) {
		hdd_err("SR operation is invalid");
		status = QDF_STATUS_E_INVAL;
		goto sr_events_end;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SR_OPERATION, sr_nl_oper)) {
		hdd_err("failed to put attr SR Operation");
		goto sr_events_end;
	}

	attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_SR_PARAMS);
	if (!attr) {
		hdd_err("nesting failed");
		goto sr_events_end;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SR_PARAMS_REASON_CODE,
			sr_nl_rc)) {
		hdd_err("failed to put attr SR Reascon Code");
		goto sr_events_end;
	}
	if (sr_nl_rc == QCA_WLAN_SR_REASON_CODE_ROAMING &&
	    sr_nl_oper == QCA_WLAN_SR_OPERATION_SR_RESUME) {
		if (nla_put_u32(
			skb,
			QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET,
			srg_min_pd_offset)) {
			hdd_err("srg_pd_min_offset put fail");
			goto sr_events_end;
		}
		if (nla_put_u32(
			skb,
			QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET,
			srg_max_pd_offset)) {
			hdd_err("srg_pd_min_offset put fail");
			goto sr_events_end;
		}
		if (nla_put_u32(
		      skb,
		      QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET,
		      non_srg_max_pd_offset)) {
			hdd_err("non_srg_pd_offset put fail");
			goto sr_events_end;
		}
	}
	status = QDF_STATUS_SUCCESS;
	nla_nest_end(skb, attr);

sr_events_end:
	return status;
}

static void hdd_sr_osif_events(struct wlan_objmgr_vdev *vdev,
			       enum sr_osif_operation sr_osif_oper,
			       enum sr_osif_reason_code sr_osif_rc)
{
	struct hdd_adapter *adapter;
	struct wireless_dev *wdev;
	struct wiphy *wiphy;
	struct sk_buff *skb;
	uint32_t idx = QCA_NL80211_VENDOR_SUBCMD_SR_INDEX;
	uint32_t len = NLMSG_HDRLEN;
	uint8_t non_srg_max_pd_offset = 0;
	uint8_t srg_max_pd_offset = 0;
	uint8_t srg_min_pd_offset = 0;
	QDF_STATUS status;
	enum qca_wlan_sr_operation sr_nl_oper;
	enum qca_wlan_sr_reason_code sr_nl_rc;
	struct wlan_hdd_link_info *link_info;

	if (!vdev) {
		hdd_err("Null VDEV");
		return;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("Null adapter");
		return;
	}

	adapter = link_info->adapter;
	wlan_vdev_mlme_get_srg_pd_offset(vdev, &srg_max_pd_offset,
					 &srg_min_pd_offset);
	non_srg_max_pd_offset = wlan_vdev_mlme_get_non_srg_pd_offset(vdev);
	status = hdd_sr_event_convert_operation(sr_osif_oper, &sr_nl_oper);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Invalid SR Operation: %d", sr_osif_oper);
		return;
	}
	status = hdd_sr_event_convert_reason_code(sr_osif_rc, &sr_nl_rc);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Invalid SR Reason Code: %d", sr_osif_rc);
		return;
	}

	hdd_debug("SR Operation: %u SR Reason Code: %u",
		  sr_nl_oper, sr_nl_rc);
	switch (sr_nl_oper) {
	case QCA_WLAN_SR_OPERATION_SR_SUSPEND:
	case QCA_WLAN_SR_OPERATION_SR_RESUME:
		if (sr_nl_rc == QCA_WLAN_SR_REASON_CODE_CONCURRENCY ||
		    sr_nl_rc == QCA_WLAN_SR_REASON_CODE_ROAMING) {
			wiphy = adapter->hdd_ctx->wiphy;
			wdev = &adapter->wdev;
			/* QCA_WLAN_VENDOR_ATTR_SR_OPERATION */
			len += nla_total_size(sizeof(uint8_t));
			/* QCA_WLAN_VENDOR_ATTR_SR_PARAMS_REASON_CODE */
			len += nla_total_size(sizeof(uint32_t));
			/* QCA_WLAN_VENDOR_ATTR_SR_PARAMS */
			len += nla_total_size(0);
			/*
			 * In case of resume due to roaming additional config
			 * params are required to be sent.
			 */
			if (sr_nl_rc == QCA_WLAN_SR_REASON_CODE_ROAMING &&
			    sr_nl_oper == QCA_WLAN_SR_OPERATION_SR_RESUME) {
				/* SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET */
				len += nla_total_size(sizeof(int32_t));
				/* SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET */
				len += nla_total_size(sizeof(int32_t));
				/* SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET */
				len += nla_total_size(sizeof(int32_t));
			}
			skb = wlan_cfg80211_vendor_event_alloc(wiphy, wdev,
							       len, idx,
							       GFP_KERNEL);
			if (!skb) {
				hdd_err("wlan_cfg80211_vendor_event_alloc failed");
				return;
			}
			status = hdd_sr_pack_suspend_resume_event(
					skb, sr_nl_oper, sr_nl_rc,
					srg_max_pd_offset, srg_min_pd_offset,
					non_srg_max_pd_offset);
			if (QDF_IS_STATUS_ERROR(status)) {
				wlan_cfg80211_vendor_free_skb(skb);
				return;
			}

			wlan_cfg80211_vendor_event(skb, GFP_KERNEL);
			hdd_debug("SR cfg80211 event is sent");
		} else {
			hdd_debug("SR Reason code not supported");
		}
		break;
	default:
		hdd_debug("SR Operation not supported");
		break;
	}
}

void hdd_sr_register_callbacks(struct hdd_context *hdd_ctx)
{
	ucfg_spatial_reuse_register_cb(hdd_ctx->psoc, hdd_sr_osif_events);
}

static int hdd_get_srp_stats_len(void)
{
	struct cdp_pdev_obss_pd_stats_tlv stats;
	uint32_t len = NLMSG_HDRLEN;

	len += nla_total_size(sizeof(stats.num_srg_ppdu_success)) +
		nla_total_size(sizeof(stats.num_srg_ppdu_tried)) +
		nla_total_size(sizeof(stats.num_srg_opportunities)) +
		nla_total_size(sizeof(stats.num_non_srg_ppdu_success)) +
		nla_total_size(sizeof(stats.num_non_srg_ppdu_tried)) +
		nla_total_size(sizeof(stats.num_non_srg_opportunities));

	return len;
}

static int hdd_get_srp_param_len(void)
{
	uint32_t len = NLMSG_HDRLEN;

	len += nla_total_size(sizeof(bool)) +
	       nla_total_size(sizeof(bool))+
	       nla_total_size(sizeof(uint8_t))+
	       nla_total_size(sizeof(uint8_t))+
	       nla_total_size(sizeof(uint8_t));

	return len;
}

static int
hdd_add_param_info(struct sk_buff *skb, uint8_t srg_max_pd_offset,
		   uint8_t srg_min_pd_offset, uint8_t non_srg_pd_offset,
		   uint8_t sr_ctrl, int idx)
{
	struct nlattr *nla_attr;
	bool non_srg_obss_pd_disallow = sr_ctrl & NON_SRG_PD_SR_DISALLOWED;
	bool hesega_val_15_enable = sr_ctrl & HE_SIG_VAL_15_ALLOWED;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;
	hdd_debug("SR params of connected AP srg_max_pd_offset %d srg_min_pd_offset %d non_srg_pd_offset %d non_srg_obss_pd_disallow %d hesega_val_15_enable %d",
		  srg_max_pd_offset, srg_min_pd_offset, non_srg_pd_offset,
		  non_srg_obss_pd_disallow, hesega_val_15_enable);

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET,
			srg_min_pd_offset)) {
		hdd_err("srg_pd_min_offset put fail");
		goto fail;
	}
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET,
			srg_max_pd_offset)) {
		hdd_err("srg_pd_min_offset put fail");
		goto fail;
	}
	if (nla_put_u32(
		skb,
		QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET,
		non_srg_pd_offset)) {
		hdd_err("non_srg_pd_offset put fail");
		goto fail;
	}
	if (non_srg_obss_pd_disallow && nla_put_flag(
		skb,
		QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_DISALLOW)) {
		hdd_err("non_srg_obss_pd_disallow put fail or enabled");
		goto fail;
	}
	if (hesega_val_15_enable && nla_put_flag(
			 skb,
			 QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE)) {
		hdd_err("hesega_val_15_enable put fail or disabled");
		goto fail;
	}

	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}
static int
hdd_add_stats_info(struct sk_buff *skb,
		   struct cdp_pdev_obss_pd_stats_tlv *stats)
{
	struct nlattr *nla_attr;

	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_SR_STATS);
	if (!nla_attr)
		goto fail;

	hdd_debug("SR stats - srg: ppdu_success %d tried %d opportunities %d non-srg: ppdu_success %d tried %d opportunities %d",
		  stats->num_srg_ppdu_success, stats->num_srg_ppdu_tried,
		  stats->num_srg_opportunities, stats->num_non_srg_ppdu_success,
		  stats->num_non_srg_ppdu_tried,
		  stats->num_non_srg_opportunities);
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_PPDU_SUCCESS_COUNT,
			stats->num_srg_ppdu_success)) {
		hdd_err("num_srg_ppdu_success put fail");
		goto fail;
	}
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_PPDU_TRIED_COUNT,
			stats->num_srg_ppdu_tried)) {
		hdd_err("num_srg_ppdu_tried put fail");
		goto fail;
	}
	if (nla_put_u32(
		skb,
		QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_OPPORTUNITIES_COUNT,
		stats->num_srg_opportunities)) {
		hdd_err("num_srg_opportunities put fail");
		goto fail;
	}
	if (nla_put_u32(
		skb,
		QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_PPDU_SUCCESS_COUNT,
		stats->num_non_srg_ppdu_success)) {
		hdd_err("num_non_srg_ppdu_success put fail");
		goto fail;
	}
	if (nla_put_u32(
		skb,
		QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_PPDU_TRIED_COUNT,
		stats->num_non_srg_ppdu_tried)) {
		hdd_err("num_non_srg_ppdu_tried put fail");
		goto fail;
	}
	if (nla_put_u32(
		skb,
		QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_OPPORTUNITIES_COUNT,
		stats->num_non_srg_opportunities)) {
		hdd_err("num_non_srg_opportunities put fail");
		goto fail;
	}
	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}

static int hdd_get_sr_stats(struct hdd_context *hdd_ctx, uint8_t mac_id,
			    struct cdp_pdev_obss_pd_stats_tlv *stats)
{
	ol_txrx_soc_handle soc;
	uint8_t pdev_id;
	struct cdp_txrx_stats_req req = {0};

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc) {
		hdd_err("invalid soc");
		return -EINVAL;
	}

	req.mac_id = mac_id;
	pdev_id = wlan_objmgr_pdev_get_pdev_id(hdd_ctx->pdev);
	cdp_get_pdev_obss_pd_stats(soc, pdev_id, stats, &req);
	if (!stats) {
		hdd_err("invalid stats");
		return -EINVAL;
	}
	return 0;
}

static int hdd_clear_sr_stats(struct hdd_context *hdd_ctx, uint8_t mac_id)
{
	QDF_STATUS status;
	ol_txrx_soc_handle soc;
	uint8_t pdev_id;
	struct cdp_txrx_stats_req req = {0};

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc) {
		hdd_err("invalid soc");
		return -EINVAL;
	}

	req.mac_id = mac_id;
	pdev_id = wlan_objmgr_pdev_get_pdev_id(hdd_ctx->pdev);
	status = cdp_clear_pdev_obss_pd_stats(soc, pdev_id, &req);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to clear stats");
		return -EAGAIN;
	}
	return 0;
}

/**
 * hdd_check_mode_support_for_sr: Check if SR allowed or not
 * @adapter: hdd adapter
 * @sr_ctrl: sr ctrl ie
 *
 * Return: true if provided mode supports SR else flase
 */
static bool hdd_check_mode_support_for_sr(struct hdd_adapter *adapter,
					  uint8_t sr_ctrl)
{
	if ((adapter->device_mode == QDF_STA_MODE) &&
	    (!hdd_cm_is_vdev_connected(adapter->deflink) ||
	    ((sr_ctrl & NON_SRG_PD_SR_DISALLOWED) &&
	    !(sr_ctrl & SRG_INFO_PRESENT)))) {
		hdd_err("mode %d doesn't supports SR", adapter->device_mode);
		return false;
	}
	return true;
}

/**
 * __wlan_hdd_cfg80211_sr_operations: To handle SR operation
 *
 * @wiphy: wiphy structure
 * @wdev: wireless dev
 * @data: vendor command data
 * @data_len: data len
 *
 * return: success/failure code
 */
static int __wlan_hdd_cfg80211_sr_operations(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	QDF_STATUS status;
	uint32_t id;
	bool is_sr_enable = false;
	bool non_srg_sr_disallowed = false, srg_info_present = false;
	int32_t srg_pd_threshold = 0;
	int32_t non_srg_pd_threshold = 0;
	uint8_t sr_he_siga_val15_allowed = true;
	uint8_t mac_id, sr_ctrl, non_srg_max_pd_offset;
	uint8_t srg_min_pd_offset = 0, srg_max_pd_offset = 0;
	uint32_t nl_buf_len;
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(wdev->netdev);
	struct wlan_objmgr_vdev *vdev;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SR_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_MAX + 1] = {0};
	enum qca_wlan_sr_operation sr_oper;
	struct nlattr *sr_oper_attr;
	struct nlattr *sr_param_attr;
	struct sk_buff *skb;
	struct cdp_pdev_obss_pd_stats_tlv stats;
	uint8_t sr_device_modes;

	hdd_enter_dev(wdev->netdev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam() ||
	    QDF_GLOBAL_MONITOR_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM or Monitor mode");
		return -EPERM;
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink,
					   WLAN_HDD_ID_OBJ_MGR);
	if (!vdev) {
		hdd_err("Null VDEV");
		return -EINVAL;
	}
	/**
	 * Reject command if SR concurrency is not allowed and
	 * only STA mode is set in ini to enable SR.
	 **/
	ucfg_mlme_get_sr_enable_modes(hdd_ctx->psoc, &sr_device_modes);
	if (!(sr_device_modes & (1 << adapter->device_mode))) {
		hdd_debug("SR operation not allowed for mode %d",
			  adapter->device_mode);
		ret = -EINVAL;
		goto exit;
	}

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_err("Driver Modules are closed");
		ret = -EINVAL;
		goto exit;
	}
	if (!sme_is_feature_supported_by_fw(DOT11AX)) {
		hdd_err("11AX is not supported");
		ret = -EINVAL;
		goto exit;
	}
	status = ucfg_spatial_reuse_operation_allowed(hdd_ctx->psoc, vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("SR operations not allowed status: %u", status);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_HDD_ID_OBJ_MGR);
		return qdf_status_to_os_return(status);
	}
	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SR_MAX, data,
				    data_len, wlan_hdd_sr_policy)) {
		hdd_err("invalid attr");
		ret = -EINVAL;
		goto exit;
	}

	id = QCA_WLAN_VENDOR_ATTR_SR_OPERATION;
	sr_oper_attr = tb[id];
	if (!sr_oper_attr) {
		hdd_err("SR operation not specified");
		ret = -EINVAL;
		goto exit;
	}

	sr_oper = nla_get_u8(sr_oper_attr);
	hdd_debug("SR Operation 0x%x", sr_oper);

	ucfg_spatial_reuse_get_sr_config(vdev, &sr_ctrl, &non_srg_max_pd_offset,
					 &is_sr_enable);

	if (!hdd_check_mode_support_for_sr(adapter, sr_ctrl) &&
	    (sr_oper != QCA_WLAN_SR_OPERATION_GET_PARAMS)) {
		hdd_err("SR operation not allowed, sr_ctrl = %x, mode = %d",
			sr_ctrl, adapter->device_mode);
		ret = -EINVAL;
		goto exit;
	}

	if (sr_oper != QCA_WLAN_SR_OPERATION_SR_ENABLE && !is_sr_enable) {
		hdd_err("SR operation not allowed");
		ret = -EINVAL;
		goto exit;
	}

	id = QCA_WLAN_VENDOR_ATTR_SR_PARAMS;
	sr_param_attr = tb[id];
	if (sr_param_attr) {
		ret = wlan_cfg80211_nla_parse_nested(
				tb2, QCA_WLAN_VENDOR_ATTR_SR_PARAMS_MAX,
				sr_param_attr,
				qca_wlan_vendor_srp_param_policy);
		if (ret) {
			hdd_err("sr_param_attr parse failed");
			goto exit;
		}
	}
	switch (sr_oper) {
	case QCA_WLAN_SR_OPERATION_SR_ENABLE:
	case QCA_WLAN_SR_OPERATION_SR_DISABLE:
		non_srg_sr_disallowed = sr_ctrl & NON_SRG_PD_SR_DISALLOWED;
		srg_info_present = sr_ctrl & SRG_INFO_PRESENT;

		if (sr_oper == QCA_WLAN_SR_OPERATION_SR_ENABLE) {
			is_sr_enable = true;
		} else {
			is_sr_enable = false;
			if (!wlan_vdev_mlme_get_he_spr_enabled(vdev)) {
				hdd_debug("SR not enabled, reject disable command");
				ret = -EINVAL;
				goto exit;
			}
		}
		/**
		 * As per currenct implementation from userspace same
		 * PD threshold value is configured for both SRG and
		 * NON-SRG and fw will decide further based on BSS color
		 * So only SRG param is parsed and set as pd threshold
		 */
		if (is_sr_enable &&
		    tb2[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_PD_THRESHOLD]) {
			if (srg_info_present) {
				srg_pd_threshold =
				nla_get_s32(
				tb2[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_PD_THRESHOLD]);
				wlan_vdev_mlme_set_pd_threshold_present(vdev,
									true);
			} else {
				hdd_err("SRG OBSS PD threshold set is disallowed\n");
				ret = -EINVAL;
				goto exit;
			}
		}

		if (is_sr_enable &&
		    tb2[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_PD_THRESHOLD]) {
			if (!non_srg_sr_disallowed) {
				non_srg_pd_threshold =
				nla_get_s32(
				tb2[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_PD_THRESHOLD]);
				wlan_vdev_mlme_set_pd_threshold_present(vdev,
									true);
			} else {
				hdd_err("non-SRG OBSS PD threshold set is disallowed\n");
				ret = -EINVAL;
				goto exit;
			}
		}

		if (non_srg_sr_disallowed && !srg_info_present) {
			hdd_err("Failed to enable SR\n");
			ret = -EINVAL;
			goto exit;
		}

		hdd_debug("setting sr enable %d with pd threshold srg: %d non srg: %d",
			  is_sr_enable, srg_pd_threshold, non_srg_pd_threshold);
		/* Set the variables */
		ucfg_spatial_reuse_set_sr_enable(vdev, is_sr_enable);
		status = ucfg_spatial_reuse_setup_req(vdev, hdd_ctx->pdev,
						      is_sr_enable,
						      srg_pd_threshold,
						      non_srg_pd_threshold);
		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("failed to enable Spatial Reuse feature");
			ret = -EINVAL;
			goto exit;
		}

		break;
	case QCA_WLAN_SR_OPERATION_GET_STATS:
		status = policy_mgr_get_mac_id_by_session_id(
						hdd_ctx->psoc,
						adapter->deflink->vdev_id,
						&mac_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to get mac_id for vdev_id: %u",
				adapter->deflink->vdev_id); {
				ret = -EAGAIN;
				goto exit;
			}
		}
		if (hdd_get_sr_stats(hdd_ctx, mac_id, &stats)) {
			ret = -EINVAL;
			goto exit;
		}
		nl_buf_len = hdd_get_srp_stats_len();
		skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
							       nl_buf_len);
		if (!skb) {
			hdd_err("wlan_cfg80211_vendor_cmd_alloc_reply_skb failed");
			ret = -ENOMEM;
			goto exit;
		}
		if (hdd_add_stats_info(skb, &stats)) {
			wlan_cfg80211_vendor_free_skb(skb);
			ret = -EINVAL;
			goto exit;
		}

		ret = wlan_cfg80211_vendor_cmd_reply(skb);
		break;
	case QCA_WLAN_SR_OPERATION_CLEAR_STATS:
		status = policy_mgr_get_mac_id_by_session_id(
						hdd_ctx->psoc,
						adapter->deflink->vdev_id,
						&mac_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to get mac_id for vdev_id: %u",
				adapter->deflink->vdev_id);
			ret = -EAGAIN;
			goto exit;
		}
		if (hdd_clear_sr_stats(hdd_ctx, mac_id)) {
			ret = -EAGAIN;
			goto exit;
		}
		break;
	case QCA_WLAN_SR_OPERATION_PSR_AND_NON_SRG_OBSS_PD_PROHIBIT:
		if (tb2[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE])
			sr_he_siga_val15_allowed = nla_get_u8(
			tb2[QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE]
			);
		if (!sr_he_siga_val15_allowed) {
			hdd_err("invalid sr_he_siga_val15_enable param");
			ret = -EINVAL;
			goto exit;
		}
		if (!QDF_IS_STATUS_SUCCESS(ucfg_spatial_reuse_send_sr_prohibit(
					   vdev, sr_he_siga_val15_allowed))) {
			hdd_debug("Prohibit command can not be sent");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case QCA_WLAN_SR_OPERATION_PSR_AND_NON_SRG_OBSS_PD_ALLOW:
		if (!QDF_IS_STATUS_SUCCESS(ucfg_spatial_reuse_send_sr_prohibit(
					   vdev, false))) {
			hdd_debug("Prohibit command can not be sent");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case QCA_WLAN_SR_OPERATION_GET_PARAMS:
		wlan_vdev_mlme_get_srg_pd_offset(vdev, &srg_max_pd_offset,
						 &srg_min_pd_offset);
		non_srg_max_pd_offset =
			wlan_vdev_mlme_get_non_srg_pd_offset(vdev);
		sr_ctrl = wlan_vdev_mlme_get_sr_ctrl(vdev);
		nl_buf_len = hdd_get_srp_param_len();
		skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
							       nl_buf_len);
		if (!skb) {
			hdd_err("wlan_cfg80211_vendor_cmd_alloc_reply_skb failed");
			ret = -ENOMEM;
			goto exit;
		}
		if (hdd_add_param_info(skb, srg_max_pd_offset,
				       srg_min_pd_offset, non_srg_max_pd_offset,
				       sr_ctrl,
				       QCA_WLAN_VENDOR_ATTR_SR_PARAMS)) {
			wlan_cfg80211_vendor_free_skb(skb);
			ret = -EINVAL;
			goto exit;
		}

		ret = wlan_cfg80211_vendor_cmd_reply(skb);
		break;
	default:
		hdd_err("Invalid SR Operation");
		ret = -EINVAL;
		break;
	}

	hdd_exit();
exit:
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_HDD_ID_OBJ_MGR);
	return ret;
}

int wlan_hdd_cfg80211_sr_operations(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_sr_operations(wiphy, wdev, data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}
#endif
