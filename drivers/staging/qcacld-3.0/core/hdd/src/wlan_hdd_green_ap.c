/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *  DOC: wlan_hdd_green_ap.c
 *
 *  WLAN Host Device Driver Green AP implementation
 *
 */

#include <net/cfg80211.h>
#include <wlan_hdd_green_ap.h>
#include <wlan_hdd_main.h>
#include <wlan_policy_mgr_api.h>
#include <wlan_green_ap_ucfg_api.h>
#include "wlan_mlme_ucfg_api.h"
#include <osif_vdev_sync.h>
#include "wlan_osif_priv.h"
#include "wlan_lmac_if_def.h"

/**
 * hdd_green_ap_check_enable() - to check whether to enable green ap or not
 * @hdd_ctx: hdd context
 * @enable_green_ap: true - enable green ap, false - disable green ap
 *
 * Return: 0 - success, < 0 - failure
 */
static int hdd_green_ap_check_enable(struct hdd_context *hdd_ctx,
				     bool *enable_green_ap)
{
	uint8_t num_sessions, mode;
	QDF_STATUS status;

	for (mode = 0;
	     mode < QDF_MAX_NO_OF_MODE;
	     mode++) {
		if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE)
			continue;

		status = policy_mgr_mode_specific_num_active_sessions(
					hdd_ctx->psoc, mode, &num_sessions);
		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("Failed to get num sessions for mode: %d",
				mode);
			return -EINVAL;
		} else if (num_sessions) {
			*enable_green_ap = false;
			hdd_debug("active sessions for mode: %d is %d disable green AP",
				  mode, num_sessions);
			return 0;
		}
	}
	*enable_green_ap = true;
	return 0;
}

void hdd_green_ap_add_sta(struct hdd_context *hdd_ctx)
{
	wlan_green_ap_add_sta(hdd_ctx->pdev);
}

void hdd_green_ap_del_sta(struct hdd_context *hdd_ctx)
{
	wlan_green_ap_del_sta(hdd_ctx->pdev);
}

int hdd_green_ap_enable_egap(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	status = ucfg_green_ap_enable_egap(hdd_ctx->pdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("enhance green ap is not enabled, status %d",
			  status);
		return qdf_status_to_os_return(status);
	}

	return 0;
}

int hdd_green_ap_start_state_mc(struct hdd_context *hdd_ctx,
				enum QDF_OPMODE mode, bool is_session_start)
{
	struct hdd_config *cfg;
	bool enable_green_ap = false;
	uint8_t num_sap_sessions = 0, num_p2p_go_sessions = 0, ret = 0;
	QDF_STATUS status;
	bool bval = false;
	uint8_t ps_enable;

	cfg = hdd_ctx->config;
	if (!cfg) {
		hdd_err("NULL hdd config");
		return -EINVAL;
	}

	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &bval);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("unable to get vht_enable2x2");
		return -EINVAL;
	}

	if (!bval) {
		hdd_debug(" 2x2 not enabled");
	}

	if (QDF_IS_STATUS_ERROR(ucfg_green_ap_get_ps_config(hdd_ctx->pdev,
							     &ps_enable)))
		return 0;

	if (!ps_enable) {
		hdd_debug("Green AP not enabled");
		return 0;
	}

	policy_mgr_mode_specific_num_active_sessions(hdd_ctx->psoc,
						     QDF_SAP_MODE,
						     &num_sap_sessions);
	policy_mgr_mode_specific_num_active_sessions(hdd_ctx->psoc,
						     QDF_P2P_GO_MODE,
						     &num_p2p_go_sessions);

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
		if (!num_sap_sessions && !num_p2p_go_sessions)
			return 0;

		if (is_session_start) {
			hdd_debug("Disabling Green AP");
			ucfg_green_ap_set_ps_config(hdd_ctx->pdev,
						    false);
			wlan_green_ap_stop(hdd_ctx->pdev);
		} else {
			ret = hdd_green_ap_check_enable(hdd_ctx,
							&enable_green_ap);
			if (!ret) {
				if (enable_green_ap) {
					hdd_debug("Enabling Green AP");
					ucfg_green_ap_set_ps_config(
						hdd_ctx->pdev, true);
					wlan_green_ap_start(hdd_ctx->pdev);
				}
			} else {
				hdd_err("Failed to check Green AP enable status");
			}
		}
		break;
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
		if (is_session_start) {
			ret = hdd_green_ap_check_enable(hdd_ctx,
							&enable_green_ap);
			if (!ret) {
				if (enable_green_ap) {
					hdd_debug("Enabling Green AP");
					ucfg_green_ap_set_ps_config(
						hdd_ctx->pdev, true);
					wlan_green_ap_start(hdd_ctx->pdev);
				}
			} else {
				hdd_err("Failed to check Green AP enable status");
			}
		} else {
			if (!num_sap_sessions && !num_p2p_go_sessions) {
				hdd_debug("Disabling Green AP");
				ucfg_green_ap_set_ps_config(hdd_ctx->pdev,
							    false);
				wlan_green_ap_stop(hdd_ctx->pdev);
			}
		}
		break;
	default:
		break;
	}
	return ret;
}

#ifdef WLAN_SUPPORT_GAP_LL_PS_MODE
const struct nla_policy
wlan_hdd_sap_low_pwr_mode[QCA_WLAN_VENDOR_ATTR_DOZED_AP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_DOZED_AP_STATE] = {.type = NLA_U8},
};

/**
 * __wlan_hdd_enter_sap_low_pwr_mode() - Green AP low latency power
 * save mode
 * vendor command
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Return: 0 for Success and negative value for failure
 */
static int
__wlan_hdd_enter_sap_low_pwr_mode(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	uint8_t lp_flags, len;
	uint64_t cookie_id;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(wdev->netdev);
	struct hdd_ap_ctx *ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter->deflink);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_DOZED_AP_MAX + 1];
	struct sk_buff *skb;

	hdd_enter_dev(wdev->netdev);

	if (wlan_cfg80211_nla_parse(tb,
				    QCA_WLAN_VENDOR_ATTR_DOZED_AP_MAX,
				    data, data_len,
				    wlan_hdd_sap_low_pwr_mode)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_DOZED_AP_STATE]) {
		hdd_err("low power flag is not present");
		return -EINVAL;
	}

	lp_flags =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_DOZED_AP_STATE]);

	if (lp_flags > QCA_WLAN_DOZED_AP_ENABLE) {
		hdd_err("Invalid state received");
		return -EINVAL;
	}

	hdd_debug("state: %s",
		  lp_flags == QCA_WLAN_DOZED_AP_ENABLE ? "ENABLE" : "DISABLE");

	status = ucfg_green_ap_ll_ps(
			hdd_ctx->pdev, adapter->deflink->vdev, lp_flags,
			ap_ctx->sap_config.beacon_int, &cookie_id);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("unable to send low latency power save cmd");
		return -EINVAL;
	}

	hdd_debug("Cookie id received : %llu", cookie_id);

	len = NLMSG_HDRLEN;
	/*QCA_WLAN_VENDOR_ATTR_DOZED_AP_COOKIE*/
	len += nla_total_size(sizeof(u64));

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
	if (!skb) {
		hdd_err("skb allocation failed");
		return -ENOMEM;
	}

	if (wlan_cfg80211_nla_put_u64(skb, QCA_WLAN_VENDOR_ATTR_DOZED_AP_COOKIE,
				      cookie_id)) {
		hdd_err("nla_put for cookie id failed");
		goto fail;
	}

	cfg80211_vendor_cmd_reply(skb);

	hdd_exit();

	return 0;
fail:
	wlan_cfg80211_vendor_free_skb(skb);
	return 0;
}

int
wlan_hdd_enter_sap_low_pwr_mode(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_enter_sap_low_pwr_mode(wiphy, wdev,
						  data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return 0;
}

QDF_STATUS wlan_hdd_send_green_ap_ll_ps_event(
					struct wlan_objmgr_vdev *vdev,
					struct wlan_green_ap_ll_ps_event_param
					*ll_ps_param)
{
	int index = QCA_NL80211_VENDOR_SUBCMD_DOZED_AP_INDEX;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t len;
	struct vdev_osif_priv *osif_priv;
	struct sk_buff *skb;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		hdd_err("OSIF priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_enter_dev(osif_priv->wdev->netdev);

	len = NLMSG_HDRLEN;
	/*QCA_WLAN_VENDOR_ATTR_DOZED_AP_NEXT_TSF*/
	len += nla_total_size(sizeof(u64));
	/*QCA_WLAN_VENDOR_ATTR_DOZED_AP_COOKIE*/
	len += nla_total_size(sizeof(u64));
	/*QCA_WLAN_VENDOR_ATTR_DOZED_AP_BI_MULTIPLIER*/
	len += nla_total_size(sizeof(u16));

	skb = wlan_cfg80211_vendor_event_alloc(osif_priv->wdev->wiphy,
					       osif_priv->wdev, len,
					       index, qdf_mem_malloc_flags());
	if (!skb) {
		hdd_err("skb allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	if (wlan_cfg80211_nla_put_u64(skb, QCA_WLAN_VENDOR_ATTR_DOZED_AP_COOKIE,
				      ll_ps_param->dialog_token)) {
		hdd_err("nla_put failed");
		status = QDF_STATUS_E_FAILURE;
		goto nla_put_failure;
	}

	if (wlan_cfg80211_nla_put_u64(skb, QCA_WLAN_VENDOR_ATTR_DOZED_AP_NEXT_TSF,
				      ll_ps_param->next_tsf)) {
		hdd_err("nla_put failed for next tsf");
		status = QDF_STATUS_E_FAILURE;
		goto nla_put_failure;
	}

	if (nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_DOZED_AP_BI_MULTIPLIER,
			ll_ps_param->bcn_mult)) {
		hdd_err("nla_put for BI multiplier failed");
		status = QDF_STATUS_E_FAILURE;
		goto nla_put_failure;
	}

	hdd_debug("next_tsf : %llu, cookie: %u beacon multiplier: %u",
		  ll_ps_param->next_tsf, ll_ps_param->dialog_token,
		  ll_ps_param->bcn_mult);

	wlan_cfg80211_vendor_event(skb, qdf_mem_malloc_flags());

	hdd_exit();

	return status;

nla_put_failure:
	wlan_cfg80211_vendor_free_skb(skb);
	return status;
}

QDF_STATUS green_ap_register_hdd_callback(struct wlan_objmgr_pdev *pdev,
					  struct green_ap_hdd_callback *hdd_cback)
{
	struct wlan_pdev_green_ap_ctx *green_ap_ctx;

	green_ap_ctx = wlan_objmgr_pdev_get_comp_private_obj(
			pdev, WLAN_UMAC_COMP_GREEN_AP);

	if (!green_ap_ctx) {
		hdd_err("green ap context obtained is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	green_ap_ctx->hdd_cback = *hdd_cback;

	return QDF_STATUS_SUCCESS;
}
#endif
