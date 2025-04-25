/*
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
 * DOC: wlan_hdd_wifi_pos_pasn.c
 *
 * WLAN Host Device Driver WIFI POSITION PASN authentication APIs implementation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <net/cfg80211.h>
#include "qdf_trace.h"
#include "qdf_types.h"
#include "wlan_hdd_wifi_pos_pasn.h"
#include "wifi_pos_pasn_api.h"
#include "wifi_pos_ucfg_i.h"
#include "wlan_crypto_global_api.h"
#include "wifi_pos_ucfg_i.h"
#include "wlan_nl_to_crypto_params.h"
#include "wlan_mlo_mgr_sta.h"

const struct nla_policy
wifi_pos_pasn_auth_status_policy[QCA_WLAN_VENDOR_ATTR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PASN_ACTION] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PASN_PEERS] = {.type = NLA_NESTED},
};

const struct nla_policy
wifi_pos_pasn_auth_policy[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR] = VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR] = VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_STATUS_SUCCESS] = {.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_LTF_KEYSEED_REQUIRED] = {
							.type = NLA_FLAG},
};

const struct nla_policy
wifi_pos_pasn_set_ranging_ctx_policy[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION] = {
					.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR] =
					VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR] =
					VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SHA_TYPE] = {
					.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK] = {
					.type = NLA_BINARY, .len = MAX_PMK_LEN},
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER] = {
					.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED] = {
					.type = NLA_BINARY, .len = MAX_PMK_LEN},
};

static int
wlan_hdd_cfg80211_send_pasn_auth_status(struct wiphy *wiphy,
					struct net_device *dev,
					const void *data, int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct wlan_pasn_auth_status *pasn_data;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX + 1];
	struct nlattr *curr_attr;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool is_pasn_success = false;
	int ret, i = 0, rem;

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_hdd_validate_vdev_id(adapter->deflink->vdev_id))
		return -EINVAL;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MAX,
				    data, data_len,
				    wifi_pos_pasn_auth_status_policy)) {
		hdd_err_rl("Invalid PASN auth status attributes");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_PASN_PEERS]) {
		hdd_err_rl("No PASN peer");
		return -EINVAL;
	}

	pasn_data = qdf_mem_malloc(sizeof(*pasn_data));
	if (!pasn_data)
		return -ENOMEM;

	pasn_data->vdev_id = adapter->deflink->vdev_id;
	nla_for_each_nested(curr_attr, tb[QCA_WLAN_VENDOR_ATTR_PASN_PEERS],
			    rem) {
		if (wlan_cfg80211_nla_parse_nested(
			tb2, QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX, curr_attr,
			wifi_pos_pasn_auth_policy)) {
			hdd_err_rl("nla_parse failed");
			qdf_mem_free(pasn_data);
			return -EINVAL;
		}

		is_pasn_success = nla_get_flag(
			tb2[QCA_WLAN_VENDOR_ATTR_PASN_PEER_STATUS_SUCCESS]);
		if (!is_pasn_success)
			pasn_data->auth_status[i].status =
					WLAN_PASN_AUTH_STATUS_PASN_FAILED;

		hdd_debug("PASN auth status:%d",
			  pasn_data->auth_status[i].status);

		if (tb2[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR]) {
			nla_memcpy(pasn_data->auth_status[i].peer_mac.bytes,
				   tb2[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR],
				   QDF_MAC_ADDR_SIZE);
			hdd_debug("Peer mac[%d]: " QDF_MAC_ADDR_FMT, i,
				  QDF_MAC_ADDR_REF(
				  pasn_data->auth_status[i].peer_mac.bytes));
		}

		if (tb2[QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR]) {
			nla_memcpy(pasn_data->auth_status[i].self_mac.bytes,
				   tb2[QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR],
				   QDF_MAC_ADDR_SIZE);
			hdd_debug("Src addr[%d]: " QDF_MAC_ADDR_FMT, i,
				  QDF_MAC_ADDR_REF(
				  pasn_data->auth_status[i].self_mac.bytes));
		}

		i++;
		pasn_data->num_peers++;
		if (pasn_data->num_peers >= WLAN_MAX_11AZ_PEERS) {
			hdd_err_rl("Invalid num_peers:%d",
				   pasn_data->num_peers);
			qdf_mem_free(pasn_data);
			return -EINVAL;
		}
	}

	status = wifi_pos_send_pasn_auth_status(hdd_ctx->psoc, pasn_data);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Send pasn auth status failed");

	qdf_mem_free(pasn_data);
	ret = qdf_status_to_os_return(status);

	return ret;
}

int wlan_hdd_wifi_pos_send_pasn_auth_status(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_cfg80211_send_pasn_auth_status(wiphy, wdev->netdev,
							data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

#define WLAN_PASN_AUTH_KEY_INDEX 0

static int wlan_cfg80211_set_pasn_key(struct hdd_adapter *adapter,
				      struct nlattr **tb)
{
	struct wlan_crypto_key *crypto_key;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc =
			adapter->hdd_ctx->psoc;
	struct qdf_mac_addr peer_mac = {0};
	struct wlan_pasn_auth_status *pasn_status;
	bool is_ltf_keyseed_required;
	QDF_STATUS status;
	int ret = 0;
	int cipher_len;
	uint32_t cipher;

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink,
					   WLAN_WIFI_POS_CORE_ID);
	if (!vdev) {
		hdd_err("Key params is NULL");
		return -EINVAL;
	}

	crypto_key = qdf_mem_malloc(sizeof(*crypto_key));
	if (!crypto_key) {
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_WIFI_POS_CORE_ID);
		return -ENOMEM;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER]) {
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_WIFI_POS_CORE_ID);
		qdf_mem_free(crypto_key);
		return -EINVAL;
	}

	cipher = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER]);
	crypto_key->cipher_type = osif_nl_to_crypto_cipher_type(cipher);

	cipher_len = osif_nl_to_crypto_cipher_len(cipher);
	crypto_key->keylen =
		nla_len(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK]);
	if (cipher_len < 0 || crypto_key->keylen < cipher_len ||
	    (crypto_key->keylen >
	     (WLAN_CRYPTO_KEYBUF_SIZE + WLAN_CRYPTO_MICBUF_SIZE))) {
		hdd_err_rl("Invalid key length %d", crypto_key->keylen);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_WIFI_POS_CORE_ID);
		qdf_mem_free(crypto_key);
		return -EINVAL;
	}

	crypto_key->keyix = WLAN_PASN_AUTH_KEY_INDEX;
	qdf_mem_copy(&crypto_key->keyval[0],
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK]),
		     crypto_key->keylen);

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]) {
		hdd_err_rl("BSSID is not present");
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_WIFI_POS_CORE_ID);
		qdf_mem_free(crypto_key);
		return -EINVAL;
	}

	qdf_mem_copy(crypto_key->macaddr,
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]),
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(peer_mac.bytes,
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]),
		     QDF_MAC_ADDR_SIZE);

	hdd_debug("PASN unicast key opmode %d, key_len %d",
		  vdev->vdev_mlme.vdev_opmode,
		  crypto_key->keylen);

	status = ucfg_crypto_set_key_req(vdev, crypto_key,
					 WLAN_CRYPTO_KEY_TYPE_UNICAST);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("PASN set_key failed");
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_WIFI_POS_CORE_ID);
		qdf_mem_free(crypto_key);
		return -EFAULT;
	}
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_WIFI_POS_CORE_ID);
	qdf_mem_free(crypto_key);

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac.bytes,
					   WLAN_WIFI_POS_CORE_ID);
	if (!peer) {
		hdd_err("PASN peer is not found");
		return -EFAULT;
	}

	/*
	 * If LTF key seed is not required for the peer, then update
	 * the source mac address for that peer by sending PASN auth
	 * status command.
	 * If LTF keyseed is required, then PASN Auth status command
	 * will be sent after LTF keyseed command.
	 */
	is_ltf_keyseed_required =
			ucfg_wifi_pos_is_ltf_keyseed_required_for_peer(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_WIFI_POS_CORE_ID);
	if (is_ltf_keyseed_required)
		return 0;

	pasn_status = qdf_mem_malloc(sizeof(*pasn_status));
	if (!pasn_status)
		return -ENOMEM;

	pasn_status->vdev_id = adapter->deflink->vdev_id;
	pasn_status->num_peers = 1;

	qdf_mem_copy(pasn_status->auth_status[0].peer_mac.bytes,
		     peer_mac.bytes, QDF_MAC_ADDR_SIZE);

	if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR])
		qdf_mem_copy(pasn_status->auth_status[0].self_mac.bytes,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR]),
			     QDF_MAC_ADDR_SIZE);

	status = wifi_pos_send_pasn_auth_status(psoc, pasn_status);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Send PASN auth status failed");

	ret = qdf_status_to_os_return(status);

	qdf_mem_free(pasn_status);

	return ret;
}

#define MLO_ALL_VDEV_LINK_ID -1

#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS
wlan_hdd_cfg80211_send_set_ltf_keyseed_mlo_vdev(struct hdd_context *hdd_ctx,
						struct wlan_objmgr_vdev *vdev,
						struct hdd_adapter *adapter,
						struct wlan_crypto_ltf_keyseed_data *data,
						int link_id)
{
	struct wlan_objmgr_vdev *link_vdev;
	struct wlan_objmgr_peer *peer;
	uint16_t link, vdev_count = 0;
	struct qdf_mac_addr peer_link_mac;
	struct qdf_mac_addr original_mac;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	QDF_STATUS status;
	uint8_t vdev_id;
	struct wlan_hdd_link_info *link_info;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev))
		return QDF_STATUS_SUCCESS;

	qdf_copy_macaddr(&peer_link_mac, &data->peer_mac_addr);
	qdf_copy_macaddr(&original_mac, &data->peer_mac_addr);
	mlo_sta_get_vdev_list(vdev, &vdev_count, wlan_vdev_list);

	for (link = 0; link < vdev_count; link++) {
		link_vdev = wlan_vdev_list[link];
		vdev_id = wlan_vdev_get_id(link_vdev);

		link_info = hdd_get_link_info_by_vdev(hdd_ctx, vdev_id);
		if (!link_info) {
			mlo_release_vdev_ref(link_vdev);
			continue;
		}

		peer = NULL;
		switch (adapter->device_mode) {
		case QDF_SAP_MODE:
			if (wlan_vdev_mlme_is_mlo_vdev(link_vdev))
				peer = wlan_hdd_ml_sap_get_peer(
						link_vdev,
						peer_link_mac.bytes);
			break;
		case QDF_STA_MODE:
		default:
			peer = wlan_objmgr_vdev_try_get_bsspeer(link_vdev,
								WLAN_OSIF_ID);
			break;
		}

		if (peer) {
			qdf_mem_copy(peer_link_mac.bytes,
				     wlan_peer_get_macaddr(peer),
				     QDF_MAC_ADDR_SIZE);
			wlan_objmgr_peer_release_ref(peer, WLAN_OSIF_ID);

		} else if (wlan_vdev_mlme_is_mlo_link_vdev(link_vdev) &&
			   adapter->device_mode == QDF_STA_MODE) {
			status = wlan_hdd_mlo_copy_partner_addr_from_mlie(
						link_vdev, &peer_link_mac);
			if (QDF_IS_STATUS_ERROR(status)) {
				hdd_err("Failed to get peer address from ML IEs");
				mlo_release_vdev_ref(link_vdev);
				continue;
			}
		} else {
			hdd_err("Peer is null");
			mlo_release_vdev_ref(link_vdev);
			continue;
		}

		qdf_copy_macaddr(&data->peer_mac_addr, &peer_link_mac);
		data->vdev_id = wlan_vdev_get_id(link_vdev);

		status = wlan_crypto_set_ltf_keyseed(hdd_ctx->psoc, data);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Set LTF Keyseed failed vdev:%d for peer: "
				QDF_MAC_ADDR_FMT, data->vdev_id,
				QDF_MAC_ADDR_REF(data->peer_mac_addr.bytes));
			mlo_release_vdev_ref(link_vdev);
			continue;
		}

		mlo_release_vdev_ref(link_vdev);
	}
	qdf_copy_macaddr(&data->peer_mac_addr, &original_mac);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
wlan_hdd_cfg80211_send_set_ltf_keyseed_mlo_vdev(struct hdd_context *hdd_ctx,
						struct wlan_objmgr_vdev *vdev,
						struct hdd_adapter *adapter,
						struct wlan_crypto_ltf_keyseed_data *data,
						int link_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static int
wlan_hdd_cfg80211_send_set_ltf_keyseed(struct wiphy *wiphy,
				       struct net_device *dev,
				       struct nlattr **tb)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct wlan_pasn_auth_status *pasn_auth_status;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_crypto_ltf_keyseed_data *data;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool is_ltf_keyseed_required;
	enum wlan_peer_type peer_type;
	int ret;

	hdd_enter();
	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_hdd_validate_vdev_id(adapter->deflink->vdev_id))
		return -EINVAL;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	data = qdf_mem_malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;

	data->vdev_id = adapter->deflink->vdev_id;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(hdd_ctx->psoc,
						    data->vdev_id,
						    WLAN_WIFI_POS_OSIF_ID);
	if (!vdev) {
		hdd_err_rl("Vdev is not found for id:%d", data->vdev_id);
		ret = -EINVAL;
		goto err;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_OSIF_ID);
		hdd_err_rl("BSSID is not present");
		ret = -EINVAL;
		goto err;
	}

	qdf_mem_copy(data->peer_mac_addr.bytes,
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]),
		     QDF_MAC_ADDR_SIZE);

	if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR])
		qdf_mem_copy(data->src_mac_addr.bytes,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR]),
			     QDF_MAC_ADDR_SIZE);

	data->key_seed_len =
		nla_len(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED]);
	if (!data->key_seed_len ||
	    data->key_seed_len < WLAN_MIN_SECURE_LTF_KEYSEED_LEN ||
	    data->key_seed_len > WLAN_MAX_SECURE_LTF_KEYSEED_LEN) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_OSIF_ID);
		hdd_err_rl("Invalid key seed length:%d", data->key_seed_len);
		ret = -EINVAL;
		goto err;
	}

	qdf_mem_copy(data->key_seed,
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED]),
		     data->key_seed_len);

	/*
	 * For MLO vdev send set LTF keyseed command on each link for the link
	 * peer address similar to install key command
	 */
	if (wlan_vdev_mlme_is_mlo_vdev(vdev))
		status = wlan_hdd_cfg80211_send_set_ltf_keyseed_mlo_vdev(
						hdd_ctx, vdev, adapter,
						data, MLO_ALL_VDEV_LINK_ID);
	else
		status = wlan_crypto_set_ltf_keyseed(hdd_ctx->psoc, data);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_OSIF_ID);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Set LTF Keyseed failed vdev_id:%d", data->vdev_id);
		ret = qdf_status_to_os_return(status);
		goto err;
	}

	peer = wlan_objmgr_get_peer_by_mac(hdd_ctx->psoc,
					   data->peer_mac_addr.bytes,
					   WLAN_WIFI_POS_CORE_ID);
	if (!peer) {
		hdd_err_rl("PASN peer is not found");
		/*
		 * Auth status need not be sent for the BSS PASN
		 * peer. So, return if peer is not found
		 */
		ret = 0;
		goto err;
	}

	/*
	 * PASN auth status command need not be sent for associated peer.
	 * It should be sent only for PASN peer type.
	 */
	peer_type = wlan_peer_get_peer_type(peer);
	if (peer_type != WLAN_PEER_RTT_PASN) {
		wlan_objmgr_peer_release_ref(peer, WLAN_WIFI_POS_CORE_ID);
		ret = 0;
		goto err;
	}

	/*
	 * If LTF key seed is not required for the peer, then update
	 * the source mac address for that peer by sending PASN auth
	 * status command.
	 * If LTF keyseed is required, then PASN Auth status command
	 * will be sent after LTF keyseed command.
	 */
	is_ltf_keyseed_required =
			ucfg_wifi_pos_is_ltf_keyseed_required_for_peer(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_WIFI_POS_CORE_ID);

	if (!is_ltf_keyseed_required) {
		ret = 0;
		goto err;
	}

	/*
	 * Send PASN Auth status followed by SET LTF keyseed command to
	 * set the peer as authorized at firmware and firmware will start
	 * ranging after this.
	 */
	pasn_auth_status = qdf_mem_malloc(sizeof(*pasn_auth_status));
	if (!pasn_auth_status) {
		ret = -ENOMEM;
		goto err;
	}

	pasn_auth_status->vdev_id = adapter->deflink->vdev_id;
	pasn_auth_status->num_peers = 1;
	qdf_mem_copy(pasn_auth_status->auth_status[0].peer_mac.bytes,
		     data->peer_mac_addr.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(pasn_auth_status->auth_status[0].self_mac.bytes,
		     data->src_mac_addr.bytes, QDF_MAC_ADDR_SIZE);

	hdd_debug("vdev:%d Send pasn auth status", pasn_auth_status->vdev_id);
	status = wifi_pos_send_pasn_auth_status(hdd_ctx->psoc,
						pasn_auth_status);
	qdf_mem_free(pasn_auth_status);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Send PASN auth status failed");

	ret = qdf_status_to_os_return(status);
err:
	qdf_mem_free(data);
	hdd_exit();

	return ret;
}

static int
__wlan_hdd_cfg80211_set_secure_ranging_context(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(wdev->netdev);
	int errno = 0;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX + 1];
	struct qdf_mac_addr peer_mac;

	hdd_enter();

	if (wlan_cfg80211_nla_parse(tb,
				    QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX,
				    data, data_len,
				    wifi_pos_pasn_set_ranging_ctx_policy)) {
		hdd_err_rl("Invalid PASN auth status attributes");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION]) {
		hdd_err_rl("Action attribute is missing");
		return -EINVAL;
	}

	if (nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION]) ==
			QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_ADD) {
		if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK] &&
		    nla_len(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK])) {
			hdd_debug("Sec ranging CTX TK");
			errno = wlan_cfg80211_set_pasn_key(adapter, tb);
			if (errno)
				return errno;
		}

		if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED] &&
		    nla_len(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED])) {
			hdd_debug("Set LTF keyseed");
			errno = wlan_hdd_cfg80211_send_set_ltf_keyseed(wiphy,
								       wdev->netdev, tb);
			if (errno)
				return errno;
		}
	} else if (nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION]) ==
			QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_DELETE) {
		if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]) {
			hdd_err_rl("Peer mac address attribute is missing");
			return -EINVAL;
		}

		qdf_mem_copy(peer_mac.bytes,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]),
			     QDF_MAC_ADDR_SIZE);
		hdd_debug("Delete PASN peer" QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(peer_mac.bytes));
		wifi_pos_send_pasn_peer_deauth(hdd_ctx->psoc, &peer_mac);
	}

	hdd_exit();

	return errno;
}

int
wlan_hdd_cfg80211_set_secure_ranging_context(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_secure_ranging_context(wiphy,
							       wdev,
							       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
