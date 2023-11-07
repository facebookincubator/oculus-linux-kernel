/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_tdls.c
 *
 * WLAN Host Device Driver implementation for TDLS
 */

#include <wlan_hdd_includes.h>
#include <ani_global.h>
#include "osif_sync.h"
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_trace.h>
#include <net/cfg80211.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/etherdevice.h>
#include <net/ieee80211_radiotap.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_assoc.h"
#include "sme_api.h"
#include "cds_sched.h"
#include "wma_types.h"
#include "wlan_policy_mgr_api.h"
#include <qca_vendor.h>
#include "wlan_tdls_cfg_api.h"
#include "wlan_hdd_object_manager.h"
#include <wlan_reg_ucfg_api.h>

/**
 * enum qca_wlan_vendor_tdls_trigger_mode_hdd_map: Maps the user space TDLS
 *	trigger mode in the host driver.
 * @WLAN_HDD_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT: TDLS Connection and
 *	disconnection handled by user space.
 * @WLAN_HDD_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT: TDLS connection and
 *	disconnection controlled by host driver based on data traffic.
 * @WLAN_HDD_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL: TDLS connection and
 *	disconnection jointly controlled by user space and host driver.
 */
enum qca_wlan_vendor_tdls_trigger_mode_hdd_map {
	WLAN_HDD_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT =
		QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT,
	WLAN_HDD_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT =
		QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT,
	WLAN_HDD_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL =
		((QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT |
		  QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT) << 1),
};

/**
 * wlan_hdd_tdls_get_all_peers() - dump all TDLS peer info into output string
 * @adapter: HDD adapter
 * @buf: output string buffer to hold the peer info
 * @buflen: the size of output string buffer
 *
 * Return: The size (in bytes) of the valid peer info in the output buffer
 */
int wlan_hdd_tdls_get_all_peers(struct hdd_adapter *adapter,
				char *buf, int buflen)
{
	int len;
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;
	int ret;

	hdd_enter();

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (0 != (wlan_hdd_validate_context(hdd_ctx))) {
		len = scnprintf(buf, buflen,
				"\nHDD context is not valid\n");
		return len;
	}

	if ((QDF_STA_MODE != adapter->device_mode) &&
	    (QDF_P2P_CLIENT_MODE != adapter->device_mode)) {
		len = scnprintf(buf, buflen,
				"\nNo TDLS support for this adapter\n");
		return len;
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
	if (!vdev) {
		len = scnprintf(buf, buflen, "\nVDEV is NULL\n");
		return len;
	}
	ret = wlan_cfg80211_tdls_get_all_peers(vdev, buf, buflen);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);

	return ret;
}

static const struct nla_policy
	wlan_hdd_tdls_config_enable_policy[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX +
					   1] = {
	[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS] = {.type =
								NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS] = {.type =
								NLA_U32},
};
static const struct nla_policy
	wlan_hdd_tdls_config_disable_policy[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX +
					    1] = {
	[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR] =
		VENDOR_NLA_POLICY_MAC_ADDR,
};
static const struct nla_policy
	wlan_hdd_tdls_config_state_change_policy[QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAX
						 + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAC_ADDR] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_TDLS_NEW_STATE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_STATE_REASON] = {.type = NLA_S32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_STATE_CHANNEL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_STATE_GLOBAL_OPERATING_CLASS] = {.type =
								NLA_U32},
};
static const struct nla_policy
	wlan_hdd_tdls_config_get_status_policy
[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_REASON] = {.type = NLA_S32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_CHANNEL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_GLOBAL_OPERATING_CLASS] = {
							.type = NLA_U32},
};

const struct nla_policy
	wlan_hdd_tdls_mode_configuration_policy
	[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_MAX + 1] = {
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TRIGGER_MODE] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_STATS_PERIOD] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_THRESHOLD] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_DISCOVERY_PERIOD] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_MAX_DISCOVERY_ATTEMPT] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_TIMEOUT] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_PACKET_THRESHOLD] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_SETUP_RSSI_THRESHOLD] = {
						.type = NLA_S32},
		[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TEARDOWN_RSSI_THRESHOLD] = {
						.type = NLA_S32},
};

/**
 * __wlan_hdd_cfg80211_exttdls_get_status() - handle get status cfg80211 command
 * @wiphy: wiphy
 * @wdev: wireless dev
 * @data: netlink buffer with the mac address of the peer to get the status for
 * @data_len: length of data in bytes
 */
static int
__wlan_hdd_cfg80211_exttdls_get_status(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	/* TODO */
	return 0;
}

/**
 * __wlan_hdd_cfg80211_configure_tdls_mode() - configure the tdls mode
 * @wiphy: wiphy
 * @wdev: wireless dev
 * @data: netlink buffer
 * @data_len: length of data in bytes
 *
 * Return 0 for success and error code for failure
 */
static int
__wlan_hdd_cfg80211_configure_tdls_mode(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_MAX + 1];
	int ret;
	uint32_t trigger_mode;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return -EINVAL;

	if (!adapter)
		return -EINVAL;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_MAX,
				    data, data_len,
				    wlan_hdd_tdls_mode_configuration_policy)) {
		hdd_err("Invalid attribute");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TRIGGER_MODE]) {
		hdd_err("attr tdls trigger mode failed");
		return -EINVAL;
	}
	trigger_mode = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TRIGGER_MODE]);
	hdd_debug("TDLS trigger mode %d", trigger_mode);

	if (hdd_ctx->tdls_umac_comp_active) {
		struct wlan_objmgr_vdev *vdev;

		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
		if (!vdev)
			return -EINVAL;
		ret = wlan_cfg80211_tdls_configure_mode(vdev,
							trigger_mode);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);
		return ret;
	}

	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_configure_tdls_mode() - configure tdls mode
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
int wlan_hdd_cfg80211_configure_tdls_mode(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_configure_tdls_mode(wiphy, wdev, data,
							data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * wlan_hdd_cfg80211_exttdls_get_status() - get ext tdls status
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
int wlan_hdd_cfg80211_exttdls_get_status(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_exttdls_get_status(wiphy, wdev,
						       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_exttdls_enable() - enable an externally controllable
 *                                      TDLS peer and set parameters
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: wireless dev pointer
 * @data: netlink buffer with peer MAC address and configuration parameters
 * @data_len: size of data in bytes
 *
 * This function sets channel, operation class, maximum latency and minimal
 * bandwidth parameters on a TDLS peer that's externally controllable.
 *
 * Return: 0 for success; negative errno otherwise
 */
static int
__wlan_hdd_cfg80211_exttdls_enable(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	/* TODO */
	return 0;
}

/**
 * wlan_hdd_cfg80211_exttdls_enable() - enable ext tdls
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
int wlan_hdd_cfg80211_exttdls_enable(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_exttdls_enable(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_exttdls_disable() - disable an externally controllable
 *                                       TDLS peer
 * @wiphy: wiphy
 * @wdev: wireless dev pointer
 * @data: netlink buffer with peer MAC address
 * @data_len: size of data in bytes
 *
 * This function disables an externally controllable TDLS peer
 *
 * Return: 0 for success; negative errno otherwise
 */
static int __wlan_hdd_cfg80211_exttdls_disable(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)
{
	/* TODO */
	return 0;
}

/**
 * wlan_hdd_cfg80211_exttdls_disable() - disable ext tdls
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
int wlan_hdd_cfg80211_exttdls_disable(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_exttdls_disable(wiphy, wdev,
						    data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

#ifdef TDLS_MGMT_VERSION5
/**
 * __wlan_hdd_cfg80211_tdls_mgmt() - handle management actions on a given peer
 * @wiphy: wiphy
 * @dev: net device
 * @peer: MAC address of the TDLS peer
 * @action_code: action code
 * @dialog_token: dialog token
 * @status_code: status code
 * @peer_capability: peer capability
 * @initiator: tdls initiator flag
 * @buf: additional IE to include
 * @len: length of buf in bytes
 * @link_id: link id for mld device
 *
 * Return: 0 if success; negative errno otherwise
 */
static int __wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
				struct net_device *dev, const uint8_t *peer,
				uint8_t action_code, uint8_t dialog_token,
				uint16_t status_code, uint32_t peer_capability,
				bool initiator, const uint8_t *buf,
				size_t len, int link_id)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
static int __wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
				struct net_device *dev, const uint8_t *peer,
				uint8_t action_code, uint8_t dialog_token,
				uint16_t status_code, uint32_t peer_capability,
				bool initiator, const uint8_t *buf,
				size_t len)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
static int __wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
				struct net_device *dev, const uint8_t *peer,
				uint8_t action_code, uint8_t dialog_token,
				uint16_t status_code, uint32_t peer_capability,
				const uint8_t *buf, size_t len)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)) || defined(TDLS_MGMT_VERSION2)
static int __wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
				struct net_device *dev, uint8_t *peer,
				uint8_t action_code, uint8_t dialog_token,
				uint16_t status_code, uint32_t peer_capability,
				const uint8_t *buf, size_t len)
#else
static int __wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
				struct net_device *dev, uint8_t *peer,
				uint8_t action_code, uint8_t dialog_token,
				uint16_t status_code, const uint8_t *buf,
				size_t len)
#endif
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	bool tdls_support;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0))
#if !(TDLS_MGMT_VERSION2)
	u32 peer_capability;

	peer_capability = 0;
#endif
#endif

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_TDLS_MGMT,
		   adapter->vdev_id, action_code);

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	cfg_tdls_get_support_enable(hdd_ctx->psoc, &tdls_support);
	if (!tdls_support) {
		hdd_debug("TDLS Disabled in INI OR not enabled in FW. "
			"Cannot process TDLS commands");
		return -ENOTSUPP;
	}

	if (hdd_ctx->tdls_umac_comp_active) {
		struct wlan_objmgr_vdev *vdev;
		int ret;

		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
		if (!vdev)
			return -EINVAL;
		ret = wlan_cfg80211_tdls_mgmt(vdev, peer,
					      action_code, dialog_token,
					      status_code, peer_capability,
					      buf, len);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);
		return ret;
	}

	return -EINVAL;
}

#ifdef TDLS_MGMT_VERSION5
/**
 * wlan_hdd_cfg80211_tdls_mgmt() - cfg80211 tdls mgmt handler function
 * @wiphy: Pointer to wiphy structure.
 * @dev: Pointer to net_device structure.
 * @peer: peer address
 * @action_code: action code
 * @dialog_token: dialog token
 * @status_code: status code
 * @peer_capability: peer capability
 * @initiator: tdls initiator flag
 * @buf: buffer
 * @len: Length of @buf
 * @link_id: link id for mld device
 *
 * This is the cfg80211 tdls mgmt handler function which invokes
 * the internal function @__wlan_hdd_cfg80211_tdls_mgmt with
 * SSR protection.
 *
 * Return: 0 for success, error number on failure.
 */
int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
					struct net_device *dev,
					const u8 *peer, u8 action_code,
					u8 dialog_token, u16 status_code,
					u32 peer_capability, bool initiator,
					const u8 *buf, size_t len, int link_id)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) || defined(WITH_BACKPORTS)
int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
					struct net_device *dev,
					const u8 *peer, u8 action_code,
					u8 dialog_token, u16 status_code,
					u32 peer_capability, bool initiator,
					const u8 *buf, size_t len)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
					struct net_device *dev,
					const u8 *peer, u8 action_code,
					u8 dialog_token, u16 status_code,
					u32 peer_capability, const u8 *buf,
					size_t len)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)) || defined(TDLS_MGMT_VERSION2)
int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
					struct net_device *dev,
					u8 *peer, u8 action_code,
					u8 dialog_token,
					u16 status_code, u32 peer_capability,
					const u8 *buf, size_t len)
#else
int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy,
					struct net_device *dev,
					u8 *peer, u8 action_code,
					u8 dialog_token,
					u16 status_code, const u8 *buf,
					size_t len)
#endif
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

#ifdef TDLS_MGMT_VERSION5
	errno = __wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
					      dialog_token, status_code,
					      peer_capability, initiator,
					      buf, len, link_id);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) || defined(WITH_BACKPORTS)
	errno = __wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
					      dialog_token, status_code,
					      peer_capability, initiator,
					      buf, len);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
	errno = __wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
					      dialog_token, status_code,
					      peer_capability, buf, len);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)) || defined(TDLS_MGMT_VERSION2)
	errno = __wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
					      dialog_token, status_code,
					      peer_capability, buf, len);
#else
	errno = __wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
					      dialog_token, status_code,
					      buf, len);
#endif

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static bool
hdd_is_sta_legacy(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!sta_ctx)
		return false;

	if ((sta_ctx->conn_info.dot11mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (sta_ctx->conn_info.dot11mode == eCSR_CFG_DOT11_MODE_11N) ||
	    (sta_ctx->conn_info.dot11mode == eCSR_CFG_DOT11_MODE_11AC) ||
	    (sta_ctx->conn_info.dot11mode == eCSR_CFG_DOT11_MODE_11N_ONLY) ||
	    (sta_ctx->conn_info.dot11mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) ||
	    (sta_ctx->conn_info.dot11mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (sta_ctx->conn_info.dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY))
		return false;

	return true;
}

uint16_t
hdd_get_tdls_connected_peer_count(struct hdd_adapter *adapter)
{
	struct wlan_objmgr_vdev *vdev;
	uint16_t peer_count;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);

	peer_count = ucfg_get_tdls_conn_peer_count(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_TDLS_ID);

	return peer_count;
}

void
hdd_check_and_set_tdls_conn_params(struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_id;
	enum hdd_dot11_mode selfdot11mode;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_psoc *psoc;
	struct hdd_context *hdd_ctx;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;

	vdev_id = wlan_vdev_get_id(vdev);
	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (!adapter)
		return;

	/*
	 * Only need to set this if STA link is in legacy mode
	 */
	if (!hdd_is_sta_legacy(adapter))
		return;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx)
		return;

	selfdot11mode = hdd_ctx->config->dot11Mode;
	/*
	 * When STA connection is made in legacy mode (11a, 11b and 11g) and
	 * selfdot11Mode is either 11ax, 11ac or 11n, TDLS connection can be
	 * made upto supporting selfdot11mode. Since, TDLS shares same netdev
	 * that of STA, checksum/TSO will be disabled during STA connection.
	 * For better TDLS throughput, enable checksum/TSO which were already
	 * disabled during STA connection.
	 */
	if (selfdot11mode == eHDD_DOT11_MODE_AUTO ||
	    selfdot11mode == eHDD_DOT11_MODE_11ax ||
	    selfdot11mode == eHDD_DOT11_MODE_11ax_ONLY ||
	    selfdot11mode == eHDD_DOT11_MODE_11ac_ONLY ||
	    selfdot11mode == eHDD_DOT11_MODE_11ac ||
	    selfdot11mode == eHDD_DOT11_MODE_11n ||
	    selfdot11mode == eHDD_DOT11_MODE_11n_ONLY)
		hdd_cm_netif_queue_enable(adapter);
}

void
hdd_check_and_set_tdls_disconn_params(struct wlan_objmgr_vdev *vdev)
{
	struct hdd_adapter *adapter;
	uint8_t vdev_id;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;

	vdev_id = wlan_vdev_get_id(vdev);
	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (!adapter)
		return;

	/*
	 * Only need to set this if STA link is in legacy mode
	 */
	if (!hdd_is_sta_legacy(adapter))
		return;

	hdd_cm_netif_queue_enable(adapter);
}

/**
 * __wlan_hdd_cfg80211_tdls_oper() - helper function to handle cfg80211 operation
 *                                   on an TDLS peer
 * @wiphy: wiphy
 * @dev: net device
 * @peer: MAC address of the TDLS peer
 * @oper: cfg80211 TDLS operation
 *
 * Return: 0 on success; negative errno otherwise
 */
static int __wlan_hdd_cfg80211_tdls_oper(struct wiphy *wiphy,
					 struct net_device *dev,
					 const uint8_t *peer,
					 enum nl80211_tdls_operation oper)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int status;
	bool tdls_support;

	hdd_enter();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	cfg_tdls_get_support_enable(hdd_ctx->psoc, &tdls_support);
	if (!tdls_support) {
		hdd_debug("TDLS Disabled in INI OR not enabled in FW. "
			"Cannot process TDLS commands");
		return -ENOTSUPP;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_TDLS_OPER,
		   adapter->vdev_id, oper);

	if (!peer) {
		hdd_err("Invalid arguments");
		return -EINVAL;
	}

	status = wlan_hdd_validate_context(hdd_ctx);

	if (0 != status)
		return status;

	if (hdd_ctx->tdls_umac_comp_active) {
		struct wlan_objmgr_vdev *vdev;

		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
		if (!vdev)
			return -EINVAL;
		status = wlan_cfg80211_tdls_oper(vdev, peer, oper);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);
		hdd_exit();
		return status;
	}

	hdd_exit();
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_tdls_oper() - handle cfg80211 operation on an TDLS peer
 * @wiphy: wiphy
 * @dev: net device
 * @peer: MAC address of the TDLS peer
 * @oper: cfg80211 TDLS operation
 *
 * Return: 0 on success; negative errno otherwise
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_hdd_cfg80211_tdls_oper(struct wiphy *wiphy,
				struct net_device *dev,
				const uint8_t *peer,
				enum nl80211_tdls_operation oper)
#else
int wlan_hdd_cfg80211_tdls_oper(struct wiphy *wiphy,
				struct net_device *dev,
				uint8_t *peer,
				enum nl80211_tdls_operation oper)
#endif
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_tdls_oper(wiphy, dev, peer, oper);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

int hdd_set_tdls_offchannel(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter,
			    int offchannel)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (hdd_ctx->tdls_umac_comp_active) {
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
		if (vdev) {
			status = ucfg_set_tdls_offchannel(vdev,
							  offchannel);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);
		}
	}
	return qdf_status_to_os_return(status);
}

int hdd_set_tdls_secoffchanneloffset(struct hdd_context *hdd_ctx,
				     struct hdd_adapter *adapter,
				     int offchanoffset)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (hdd_ctx->tdls_umac_comp_active) {
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
		if (vdev) {
			status = ucfg_set_tdls_secoffchanneloffset(vdev,
								 offchanoffset);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);
		}
	}
	return qdf_status_to_os_return(status);
}

int hdd_set_tdls_offchannelmode(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter,
				int offchanmode)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	bool tdls_off_ch;

	if (cfg_tdls_get_off_channel_enable(
		hdd_ctx->psoc, &tdls_off_ch) !=
	    QDF_STATUS_SUCCESS) {
		hdd_err("cfg get tdls off ch failed");
		return qdf_status_to_os_return(status);
	}
	if (!tdls_off_ch) {
		hdd_debug("tdls off ch is false, do nothing");
		return qdf_status_to_os_return(status);
	}

	if (hdd_ctx->tdls_umac_comp_active) {
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
		if (vdev) {
			status = ucfg_set_tdls_offchan_mode(vdev,
							    offchanmode);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);
		}
	}
	return qdf_status_to_os_return(status);
}

/**
 * hdd_set_tdls_scan_type - set scan during active tdls session
 * @hdd_ctx: ptr to hdd context.
 * @val: scan type value: 0 or 1.
 *
 * Set scan type during tdls session. If set to 1, that means driver
 * shall maintain tdls link and allow scan regardless if tdls peer is
 * buffer sta capable or not and/or if device is sleep sta capable or
 * not. If tdls peer is not buffer sta capable then during scan there
 * will be loss of Rx packets and Tx would stop when device moves away
 * from tdls channel. If set to 0, then driver shall teardown tdls link
 * before initiating scan if peer is not buffer sta capable and device
 * is not sleep sta capable. By default, scan type is set to 0.
 *
 * Return: success (0) or failure (errno value)
 */
int hdd_set_tdls_scan_type(struct hdd_context *hdd_ctx, int val)
{
	if ((val != 0) && (val != 1)) {
		hdd_err("Incorrect value of tdls scan type: %d", val);
		return -EINVAL;
	}

	cfg_tdls_set_scan_enable(hdd_ctx->psoc, (bool)val);

	return 0;
}

/**
 * wlan_hdd_tdls_antenna_switch() - Dynamic TDLS antenna  switch 1x1 <-> 2x2
 * antenna mode in standalone station
 * @hdd_ctx: Pointer to hdd context
 * @adapter: Pointer to hdd adapter
 * @mode: enum antenna_mode
 *
 * Return: 0 if success else non zero
 */
int wlan_hdd_tdls_antenna_switch(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *adapter,
				 uint32_t mode)
{
	if (hdd_ctx->tdls_umac_comp_active) {
		struct wlan_objmgr_vdev *vdev;
		int ret;

		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_TDLS_ID);
		if (!vdev)
			return -EINVAL;
		ret = wlan_tdls_antenna_switch(vdev, mode);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_TDLS_ID);
		return ret;
	}

	return 0;
}

QDF_STATUS hdd_tdls_register_peer(void *userdata, uint32_t vdev_id,
				  const uint8_t *mac, uint8_t qos)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hddctx;

	hddctx = userdata;
	if (!hddctx) {
		hdd_err("Invalid hddctx");
		return QDF_STATUS_E_INVAL;
	}
	adapter = hdd_get_adapter_by_vdev(hddctx, vdev_id);
	if (!adapter) {
		hdd_err("Invalid adapter");
		return QDF_STATUS_E_FAILURE;
	}

	return hdd_roam_register_tdlssta(adapter, mac, qos);
}

void hdd_init_tdls_config(struct tdls_start_params *tdls_cfg)
{
	tdls_cfg->tdls_send_mgmt_req = eWNI_SME_TDLS_SEND_MGMT_REQ;
	tdls_cfg->tdls_add_sta_req = eWNI_SME_TDLS_ADD_STA_REQ;
	tdls_cfg->tdls_del_sta_req = eWNI_SME_TDLS_DEL_STA_REQ;
	tdls_cfg->tdls_update_peer_state = WMA_UPDATE_TDLS_PEER_STATE;
}

void hdd_config_tdls_with_band_switch(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_vdev *tdls_obj_vdev;
	int offchmode;
	uint32_t current_band;
	bool tdls_off_ch;

	if (!hdd_ctx) {
		hdd_err("Invalid hdd_ctx");
		return;
	}

	if (ucfg_reg_get_band(hdd_ctx->pdev, &current_band) !=
	    QDF_STATUS_SUCCESS) {
		hdd_err("Failed to get current band config");
		return;
	}

	/**
	 * If all bands are supported, in below condition off channel enable
	 * orig is false and nothing is need to do
	 * 1. band switch does not happen.
	 * 2. band switch happens and it already restores
	 * 3. tdls off channel is disabled by default.
	 * If 2g or 5g is not supported. Disable tdls off channel only when
	 * tdls off channel is enabled currently.
	 */
	if ((current_band & BIT(REG_BAND_2G)) &&
	    (current_band & BIT(REG_BAND_5G))) {
		if (cfg_tdls_get_off_channel_enable_orig(
			hdd_ctx->psoc, &tdls_off_ch) !=
		    QDF_STATUS_SUCCESS) {
			hdd_err("cfg get tdls off ch orig failed");
			return;
		}
		if (!tdls_off_ch) {
			hdd_debug("tdls off ch orig is false, do nothing");
			return;
		}
		offchmode = ENABLE_CHANSWITCH;
		cfg_tdls_restore_off_channel_enable(hdd_ctx->psoc);
	} else {
		if (cfg_tdls_get_off_channel_enable(
			hdd_ctx->psoc, &tdls_off_ch) !=
		    QDF_STATUS_SUCCESS) {
			hdd_err("cfg get tdls off ch failed");
			return;
		}
		if (!tdls_off_ch) {
			hdd_debug("tdls off ch is false, do nothing");
			return;
		}
		offchmode = DISABLE_CHANSWITCH;
		cfg_tdls_store_off_channel_enable(hdd_ctx->psoc);
		cfg_tdls_set_off_channel_enable(hdd_ctx->psoc, false);
	}
	tdls_obj_vdev = ucfg_get_tdls_vdev(hdd_ctx->psoc, WLAN_TDLS_NB_ID);
	if (tdls_obj_vdev) {
		ucfg_set_tdls_offchan_mode(tdls_obj_vdev, offchmode);
		wlan_objmgr_vdev_release_ref(tdls_obj_vdev, WLAN_TDLS_NB_ID);
	}
}
