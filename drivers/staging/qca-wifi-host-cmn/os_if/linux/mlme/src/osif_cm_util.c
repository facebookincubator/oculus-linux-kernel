/*
 * Copyright (c) 2012-2015, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: osif_cm_util.c
 *
 * This file maintains definitaions of connect, disconnect, roam
 * common apis.
 */
#include <include/wlan_mlme_cmn.h>
#include "osif_cm_util.h"
#include "wlan_osif_priv.h"
#include "wlan_cfg80211.h"
#include "osif_cm_rsp.h"
#include "wlan_cfg80211_scan.h"

enum qca_sta_connect_fail_reason_codes
osif_cm_mac_to_qca_connect_fail_reason(enum wlan_status_code internal_reason)
{
	enum qca_sta_connect_fail_reason_codes reason = 0;

	if (internal_reason < STATUS_PROP_START)
		return reason;

	switch (internal_reason) {
	case STATUS_NO_NETWORK_FOUND:
		reason = QCA_STA_CONNECT_FAIL_REASON_NO_BSS_FOUND;
		break;
	case STATUS_AUTH_TX_FAIL:
		reason = QCA_STA_CONNECT_FAIL_REASON_AUTH_TX_FAIL;
		break;
	case STATUS_AUTH_NO_ACK_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_ACK_RECEIVED;
		break;
	case STATUS_AUTH_NO_RESP_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_RESP_RECEIVED;
		break;
	case STATUS_ASSOC_TX_FAIL:
		reason = QCA_STA_CONNECT_FAIL_REASON_ASSOC_REQ_TX_FAIL;
		break;
	case STATUS_ASSOC_NO_ACK_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_ACK_RECEIVED;
		break;
	case STATUS_ASSOC_NO_RESP_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_RESP_RECEIVED;
		break;
	default:
		osif_debug("QCA code not present for internal status code %d",
			   internal_reason);
	}

	return reason;
}

const char *
osif_cm_qca_reason_to_str(enum qca_disconnect_reason_codes reason)
{
	switch (reason) {
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_INTERNAL_ROAM_FAILURE);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_EXTERNAL_ROAM_FAILURE);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_GATEWAY_REACHABILITY_FAILURE);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_UNSUPPORTED_CHANNEL_CSA);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_OPER_CHANNEL_DISABLED_INDOOR);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_OPER_CHANNEL_USER_DISABLED);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_DEVICE_RECOVERY);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_KEY_TIMEOUT);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_OPER_CHANNEL_BAND_CHANGE);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_IFACE_DOWN);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_PEER_XRETRY_FAIL);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_PEER_INACTIVITY);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_SA_QUERY_TIMEOUT);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_BEACON_MISS_FAILURE);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_CHANNEL_SWITCH_FAILURE);
	CASE_RETURN_STRING(QCA_DISCONNECT_REASON_USER_TRIGGERED);
	case QCA_DISCONNECT_REASON_UNSPECIFIED:
		return "";
	default:
		return "Unknown";
	}
}

enum qca_disconnect_reason_codes
osif_cm_mac_to_qca_reason(enum wlan_reason_code internal_reason)
{
	enum qca_disconnect_reason_codes reason =
					QCA_DISCONNECT_REASON_UNSPECIFIED;

	if (internal_reason < REASON_PROP_START)
		return reason;

	switch (internal_reason) {
	case REASON_HOST_TRIGGERED_ROAM_FAILURE:
	case REASON_FW_TRIGGERED_ROAM_FAILURE:
		reason = QCA_DISCONNECT_REASON_INTERNAL_ROAM_FAILURE;
		break;
	case REASON_USER_TRIGGERED_ROAM_FAILURE:
		reason = QCA_DISCONNECT_REASON_EXTERNAL_ROAM_FAILURE;
		break;
	case REASON_GATEWAY_REACHABILITY_FAILURE:
		reason =
		QCA_DISCONNECT_REASON_GATEWAY_REACHABILITY_FAILURE;
		break;
	case REASON_UNSUPPORTED_CHANNEL_CSA:
		reason = QCA_DISCONNECT_REASON_UNSUPPORTED_CHANNEL_CSA;
		break;
	case REASON_OPER_CHANNEL_DISABLED_INDOOR:
		reason =
		QCA_DISCONNECT_REASON_OPER_CHANNEL_DISABLED_INDOOR;
		break;
	case REASON_OPER_CHANNEL_USER_DISABLED:
		reason =
		QCA_DISCONNECT_REASON_OPER_CHANNEL_USER_DISABLED;
		break;
	case REASON_DEVICE_RECOVERY:
		reason = QCA_DISCONNECT_REASON_DEVICE_RECOVERY;
		break;
	case REASON_KEY_TIMEOUT:
		reason = QCA_DISCONNECT_REASON_KEY_TIMEOUT;
		break;
	case REASON_OPER_CHANNEL_BAND_CHANGE:
		reason = QCA_DISCONNECT_REASON_OPER_CHANNEL_BAND_CHANGE;
		break;
	case REASON_IFACE_DOWN:
		reason = QCA_DISCONNECT_REASON_IFACE_DOWN;
		break;
	case REASON_PEER_XRETRY_FAIL:
		reason = QCA_DISCONNECT_REASON_PEER_XRETRY_FAIL;
		break;
	case REASON_PEER_INACTIVITY:
		reason = QCA_DISCONNECT_REASON_PEER_INACTIVITY;
		break;
	case REASON_SA_QUERY_TIMEOUT:
		reason = QCA_DISCONNECT_REASON_SA_QUERY_TIMEOUT;
		break;
	case REASON_CHANNEL_SWITCH_FAILED:
		reason = QCA_DISCONNECT_REASON_CHANNEL_SWITCH_FAILURE;
		break;
	case REASON_BEACON_MISSED:
		reason = QCA_DISCONNECT_REASON_BEACON_MISS_FAILURE;
		break;
	default:
		osif_debug("No QCA reason code for mac reason: %u",
			   internal_reason);
		/* Unspecified reason by default */
	}

	return reason;
}

static struct osif_cm_ops *osif_cm_legacy_ops;

void osif_cm_reset_id_and_src_no_lock(struct vdev_osif_priv *osif_priv)
{
	osif_priv->cm_info.last_id = CM_ID_INVALID;
	osif_priv->cm_info.last_source = CM_SOURCE_INVALID;
}

QDF_STATUS osif_cm_reset_id_and_src(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_osif_priv *osif_priv = wlan_vdev_get_ospriv(vdev);

	if (!osif_priv) {
		osif_err("Invalid vdev osif priv");
		return QDF_STATUS_E_INVAL;
	}
	qdf_spinlock_acquire(&osif_priv->cm_info.cmd_id_lock);
	osif_cm_reset_id_and_src_no_lock(osif_priv);
	qdf_spinlock_release(&osif_priv->cm_info.cmd_id_lock);

	return QDF_STATUS_SUCCESS;
}

/**
 * osif_cm_connect_complete_cb() - Connect complete callback
 * @vdev: vdev pointer
 * @rsp: connect response
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_connect_complete_cb(struct wlan_objmgr_vdev *vdev,
			    struct wlan_cm_connect_resp *rsp)
{
	return osif_connect_handler(vdev, rsp);
}

/**
 * osif_cm_failed_candidate_cb() - Callback to indicate failed candidate
 * @vdev: vdev pointer
 * @rsp: connect response
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_failed_candidate_cb(struct wlan_objmgr_vdev *vdev,
			    struct wlan_cm_connect_resp *rsp)
{
	return osif_failed_candidate_handler(vdev, rsp);
}

/**
 * osif_cm_update_id_and_src_cb() - Callback to update id and
 * source of the connect/disconnect request
 * @vdev: vdev pointer
 * @Source: Source of the connect req
 * @id: Connect/disconnect id
 *
 * Context: Any context. Takes and releases cmd id spinlock
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_update_id_and_src_cb(struct wlan_objmgr_vdev *vdev,
			     enum wlan_cm_source source, wlan_cm_id cm_id)
{
	struct vdev_osif_priv *osif_priv = wlan_vdev_get_ospriv(vdev);

	if (!osif_priv) {
		osif_err("Invalid vdev osif priv");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spinlock_acquire(&osif_priv->cm_info.cmd_id_lock);
	osif_priv->cm_info.last_id = cm_id;
	osif_priv->cm_info.last_source = source;
	qdf_spinlock_release(&osif_priv->cm_info.cmd_id_lock);

	return QDF_STATUS_SUCCESS;
}

/**
 * osif_cm_disconnect_complete_cb() - Disconnect done callback
 * @vdev: vdev pointer
 * @disconnect_rsp: Disconnect response
 *
 * Context: Any context
 * Return: QDF_STATUS
 */

static QDF_STATUS
osif_cm_disconnect_complete_cb(struct wlan_objmgr_vdev *vdev,
			       struct wlan_cm_discon_rsp *rsp)
{
	return osif_disconnect_handler(vdev, rsp);
}

#ifdef CONN_MGR_ADV_FEATURE
void osif_cm_unlink_bss(struct wlan_objmgr_vdev *vdev,
			struct qdf_mac_addr *bssid)
{
	struct scan_filter *filter;

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		return;

	filter->num_of_bssid = 1;
	qdf_copy_macaddr(&filter->bssid_list[0], bssid);
	ucfg_scan_flush_results(wlan_vdev_get_pdev(vdev), filter);
	qdf_mem_free(filter);
}

static QDF_STATUS
osif_cm_disable_netif_queue(struct wlan_objmgr_vdev *vdev)
{
	return osif_cm_netif_queue_ind(vdev,
				       WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				       WLAN_CONTROL_PATH);
}

/**
 * osif_cm_roam_sync_cb() - Roam sync callback
 * @vdev: vdev pointer
 *
 * This callback indicates os_if that roam sync ind received
 * so that os_if can stop all the activity on this connection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_roam_sync_cb(struct wlan_objmgr_vdev *vdev)
{
	osif_cm_napi_serialize(true);
	return osif_cm_netif_queue_ind(vdev,
				       WLAN_STOP_ALL_NETIF_QUEUE,
				       WLAN_CONTROL_PATH);
}

/**
 * @osif_pmksa_candidate_notify_cb: Roam pmksa candidate notify callback
 * @vdev: vdev pointer
 * @bssid: bssid
 * @index: index
 * @preauth: preauth flag
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_pmksa_candidate_notify_cb(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr *bssid,
			       int index, bool preauth)
{
	return osif_pmksa_candidate_notify(vdev, bssid, index, preauth);
}

/**
 * osif_cm_send_keys_cb() - Send keys callback
 * @vdev: vdev pointer
 *
 * This callback indicates os_if that
 * so that os_if can stop all the activity on this connection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_send_keys_cb(struct wlan_objmgr_vdev *vdev, uint8_t key_index,
		     bool pairwise, enum wlan_crypto_cipher_type cipher_type)
{
	return osif_cm_send_vdev_keys(vdev,
				       key_index,
				       pairwise,
				       cipher_type);
}
#else
static inline QDF_STATUS
osif_cm_disable_netif_queue(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(CONN_MGR_ADV_FEATURE) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * osif_link_reconfig_notify_cb() - Link reconfig notify callback
 * @vdev: vdev pointer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_link_reconfig_notify_cb(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_osif_priv *osif_priv = wlan_vdev_get_ospriv(vdev);
	struct wireless_dev *wdev;
	uint8_t link_id;
	uint16_t link_mask;
	struct pdev_osif_priv *pdev_osif_priv;
	struct wlan_objmgr_pdev *pdev;
	uint32_t data_len;
	struct sk_buff *vendor_event;
	struct qdf_mac_addr ap_mld_mac;
	QDF_STATUS status;

	if (!osif_priv) {
		osif_err("Invalid vdev osif priv");
		return QDF_STATUS_E_INVAL;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wdev is null");
		return QDF_STATUS_E_INVAL;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		osif_debug("null pdev");
		return QDF_STATUS_E_INVAL;
	}
	pdev_osif_priv = wlan_pdev_get_ospriv(pdev);
	if (!pdev_osif_priv || !pdev_osif_priv->wiphy) {
		osif_debug("null wiphy");
		return QDF_STATUS_E_INVAL;
	}

	link_id = wlan_vdev_get_link_id(vdev);
	link_mask = 1 << link_id;
	osif_debug("link reconfig on vdev %d with link id %d mask 0x%x",
		   wlan_vdev_get_id(vdev), link_id, link_mask);

	status = wlan_vdev_get_bss_peer_mld_mac(vdev, &ap_mld_mac);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_debug("get peer mld failed, vdev %d",
			   wlan_vdev_get_id(vdev));
		return status;
	}
	osif_debug("ap mld addr: "QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(ap_mld_mac.bytes));

	data_len = nla_total_size(QDF_MAC_ADDR_SIZE) +
		   nla_total_size(sizeof(uint16_t)) +
		   NLMSG_HDRLEN;

	vendor_event =
	wlan_cfg80211_vendor_event_alloc(pdev_osif_priv->wiphy,
					 wdev, data_len,
					 QCA_NL80211_VENDOR_SUBCMD_LINK_RECONFIG_INDEX,
					 GFP_KERNEL);
	if (!vendor_event) {
		osif_debug("wlan_cfg80211_vendor_event_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_AP_MLD_ADDR,
		    QDF_MAC_ADDR_SIZE, &ap_mld_mac.bytes[0]) ||
	    nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_REMOVED_LINKS,
			link_mask)) {
		osif_debug("QCA_WLAN_VENDOR_ATTR put fail");
		wlan_cfg80211_vendor_free_skb(vendor_event);
		return QDF_STATUS_E_INVAL;
	}

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
osif_link_reconfig_notify_cb(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * osif_cm_disconnect_start_cb() - Disconnect start callback
 * @vdev: vdev pointer
 *
 * This callback indicates os_if that disconnection is started
 * so that os_if can stop all the activity on this connection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_disconnect_start_cb(struct wlan_objmgr_vdev *vdev)
{
	/* Disable netif queue on disconnect start */
	return osif_cm_disable_netif_queue(vdev);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * osif_cm_roam_start_cb() - Roam start callback
 * @vdev: vdev pointer
 *
 * This callback indicates os_if that roaming has started
 * so that os_if can stop all the activity on this connection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_roam_start_cb(struct wlan_objmgr_vdev *vdev)
{
	return osif_cm_netif_queue_ind(vdev,
				       WLAN_STOP_ALL_NETIF_QUEUE,
				       WLAN_CONTROL_PATH);
}

/**
 * osif_cm_roam_abort_cb() - Roam abort callback
 * @vdev: vdev pointer
 *
 * This callback indicates os_if that roaming has been aborted
 * so that os_if can resume all the activity on this connection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_roam_abort_cb(struct wlan_objmgr_vdev *vdev)
{
	osif_cm_napi_serialize(false);
	return osif_cm_netif_queue_ind(vdev,
				       WLAN_WAKE_ALL_NETIF_QUEUE,
				       WLAN_CONTROL_PATH);
}

/**
 * osif_cm_roam_cmpl_cb() - Roam sync complete callback
 * @vdev: vdev pointer
 * @rsp: connect rsp
 *
 * This callback indicates os_if that roam sync is complete
 * so that os_if can stop all the activity on this connection
 *
 * Return: QDF_STATUS
 */

static QDF_STATUS
osif_cm_roam_cmpl_cb(struct wlan_objmgr_vdev *vdev)
{
	return osif_cm_napi_serialize(false);
}

/**
 * osif_cm_get_scan_ie_params() - Function to get scan ie params
 * @vdev: vdev pointer
 * @scan_ie: Pointer to scan_ie
 * @dot11mode_filter: Pointer to dot11mode_filter
 *
 * Get scan IE params from adapter corresponds to given vdev
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
osif_cm_get_scan_ie_params(struct wlan_objmgr_vdev *vdev,
			   struct element_info *scan_ie,
			   enum dot11_mode_filter *dot11mode_filter)
{
	osif_cm_get_scan_ie_params_cb cb = NULL;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->get_scan_ie_params_cb;
	if (cb)
		return cb(vdev, scan_ie, dot11mode_filter);

	return QDF_STATUS_E_FAILURE;
}

/**
 * osif_cm_get_scan_ie_info_cb() - Roam get scan ie params callback
 * @vdev: vdev pointer
 * @scan_ie: pointer to scan ie
 * @dot11mode_filter: pointer to dot11 mode filter
 *
 * This callback gets scan ie params from os_if
 *
 * Return: QDF_STATUS
 */

static QDF_STATUS
osif_cm_get_scan_ie_info_cb(struct wlan_objmgr_vdev *vdev,
			    struct element_info *scan_ie,
			    enum dot11_mode_filter *dot11mode_filter)
{
	return osif_cm_get_scan_ie_params(vdev, scan_ie, dot11mode_filter);
}
#endif

#ifdef WLAN_FEATURE_PREAUTH_ENABLE
/**
 * osif_cm_ft_preauth_cmpl_cb() - Roam ft preauth complete callback
 * @vdev: vdev pointer
 * @rsp: preauth response
 *
 * This callback indicates os_if that roam ft preauth is complete
 * so that os_if can send fast transition event
 *
 * Return: QDF_STATUS
 */

static QDF_STATUS
osif_cm_ft_preauth_cmpl_cb(struct wlan_objmgr_vdev *vdev,
			   struct wlan_preauth_rsp *rsp)
{
	osif_cm_ft_preauth_complete_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->ft_preauth_complete_cb;
	if (cb)
		ret = cb(vdev, rsp);

	return ret;
}

#ifdef FEATURE_WLAN_ESE
/**
 * osif_cm_cckm_preauth_cmpl_cb() - Roam cckm preauth complete callback
 * @vdev: vdev pointer
 * @rsp: preauth response
 *
 * This callback indicates os_if that roam cckm preauth is complete
 * so that os_if can send cckm preauth indication to the supplicant
 * via wireless custom event.
 *
 * Return: QDF_STATUS
 */

static QDF_STATUS
osif_cm_cckm_preauth_cmpl_cb(struct wlan_objmgr_vdev *vdev,
			     struct wlan_preauth_rsp *rsp)
{
	osif_cm_cckm_preauth_complete_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->cckm_preauth_complete_cb;
	if (cb)
		ret = cb(vdev, rsp);

	return ret;
}
#endif
#endif

static struct mlme_cm_ops cm_ops = {
	.mlme_cm_connect_complete_cb = osif_cm_connect_complete_cb,
	.mlme_cm_failed_candidate_cb = osif_cm_failed_candidate_cb,
	.mlme_cm_update_id_and_src_cb = osif_cm_update_id_and_src_cb,
	.mlme_cm_disconnect_complete_cb = osif_cm_disconnect_complete_cb,
	.mlme_cm_disconnect_start_cb = osif_cm_disconnect_start_cb,
#ifdef CONN_MGR_ADV_FEATURE
	.mlme_cm_roam_sync_cb = osif_cm_roam_sync_cb,
	.mlme_cm_pmksa_candidate_notify_cb = osif_pmksa_candidate_notify_cb,
	.mlme_cm_send_keys_cb = osif_cm_send_keys_cb,
	.mlme_cm_link_reconfig_notify_cb = osif_link_reconfig_notify_cb,
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	.mlme_cm_roam_start_cb = osif_cm_roam_start_cb,
	.mlme_cm_roam_abort_cb = osif_cm_roam_abort_cb,
	.mlme_cm_roam_cmpl_cb = osif_cm_roam_cmpl_cb,
	.mlme_cm_roam_get_scan_ie_cb = osif_cm_get_scan_ie_info_cb,
#endif
#ifdef WLAN_FEATURE_PREAUTH_ENABLE
	.mlme_cm_ft_preauth_cmpl_cb = osif_cm_ft_preauth_cmpl_cb,
#ifdef FEATURE_WLAN_ESE
	.mlme_cm_cckm_preauth_cmpl_cb = osif_cm_cckm_preauth_cmpl_cb,
#endif
#endif
#ifdef WLAN_VENDOR_HANDOFF_CONTROL
	.mlme_cm_get_vendor_handoff_params_cb =
					osif_cm_vendor_handoff_params_cb,
#endif
};

/**
 * osif_cm_get_global_ops() - Get connection manager global ops
 *
 * Return: Connection manager global ops
 */
static struct mlme_cm_ops *osif_cm_get_global_ops(void)
{
	return &cm_ops;
}

QDF_STATUS osif_cm_register_cb(void)
{
	mlme_set_osif_cm_cb(osif_cm_get_global_ops);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS osif_cm_osif_priv_init(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_osif_priv *osif_priv = wlan_vdev_get_ospriv(vdev);
	enum QDF_OPMODE mode = wlan_vdev_mlme_get_opmode(vdev);

	if (mode != QDF_STA_MODE && mode != QDF_P2P_CLIENT_MODE)
		return QDF_STATUS_SUCCESS;

	if (!osif_priv) {
		osif_err("Invalid vdev osif priv");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spinlock_create(&osif_priv->cm_info.cmd_id_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS osif_cm_osif_priv_deinit(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_osif_priv *osif_priv = wlan_vdev_get_ospriv(vdev);
	enum QDF_OPMODE mode = wlan_vdev_mlme_get_opmode(vdev);

	if (mode != QDF_STA_MODE && mode != QDF_P2P_CLIENT_MODE)
		return QDF_STATUS_SUCCESS;

	if (!osif_priv) {
		osif_err("Invalid vdev osif priv");
		return QDF_STATUS_E_INVAL;
	}
	qdf_spinlock_destroy(&osif_priv->cm_info.cmd_id_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS osif_cm_connect_comp_ind(struct wlan_objmgr_vdev *vdev,
				    struct wlan_cm_connect_resp *rsp,
				    enum osif_cb_type type)
{
	osif_cm_connect_comp_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->connect_complete_cb;
	if (cb)
		ret = cb(vdev, rsp, type);

	return ret;
}

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
QDF_STATUS osif_cm_vendor_handoff_params_cb(struct wlan_objmgr_psoc *psoc,
					    void *vendor_handoff_context)
{
	osif_cm_get_vendor_handoff_params_cb cb = NULL;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->vendor_handoff_params_cb;
	if (cb)
		return cb(psoc, vendor_handoff_context);

	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS osif_cm_disconnect_comp_ind(struct wlan_objmgr_vdev *vdev,
				       struct wlan_cm_discon_rsp *rsp,
				       enum osif_cb_type type)
{
	osif_cm_disconnect_comp_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->disconnect_complete_cb;
	if (cb)
		ret = cb(vdev, rsp, type);

	return ret;
}

#ifdef CONN_MGR_ADV_FEATURE
QDF_STATUS osif_cm_netif_queue_ind(struct wlan_objmgr_vdev *vdev,
				   enum netif_action_type action,
				   enum netif_reason_type reason)
{
	osif_cm_netif_queue_ctrl_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->netif_queue_control_cb;
	if (cb)
		ret = cb(vdev, action, reason);

	return ret;
}

QDF_STATUS osif_cm_napi_serialize(bool action)
{
	os_if_cm_napi_serialize_ctrl_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->napi_serialize_control_cb;
	if (cb)
		ret = cb(action);

	return ret;
}

QDF_STATUS osif_cm_save_gtk(struct wlan_objmgr_vdev *vdev,
			    struct wlan_cm_connect_resp *rsp)
{
	osif_cm_save_gtk_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->save_gtk_cb;
	if (cb)
		ret = cb(vdev, rsp);

	return ret;
}

QDF_STATUS
osif_cm_send_vdev_keys(struct wlan_objmgr_vdev *vdev,
		       uint8_t key_index,
		       bool pairwise,
		       enum wlan_crypto_cipher_type cipher_type)
{
	osif_cm_send_vdev_keys_cb cb = NULL;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->send_vdev_keys_cb;
	if (cb)
		return cb(vdev, key_index, pairwise, cipher_type);

	return QDF_STATUS_E_FAILURE;
}
#endif

#ifdef WLAN_FEATURE_FILS_SK
QDF_STATUS osif_cm_set_hlp_data(struct net_device *dev,
				struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_connect_resp *rsp)
{
	osif_cm_set_hlp_data_cb cb = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (osif_cm_legacy_ops)
		cb = osif_cm_legacy_ops->set_hlp_data_cb;
	if (cb)
		ret = cb(dev, vdev, rsp);

	return ret;
}
#endif

void osif_cm_set_legacy_cb(struct osif_cm_ops *osif_legacy_ops)
{
	osif_cm_legacy_ops = osif_legacy_ops;
}

void osif_cm_reset_legacy_cb(void)
{
	osif_cm_legacy_ops = NULL;
}
