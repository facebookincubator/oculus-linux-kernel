/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains ll_lt_sap_definitions specific to the ll_lt_sap module
 */

#include "wlan_hdd_ll_lt_sap.h"
#include "wlan_ll_sap_ucfg_api.h"
#include "osif_sync.h"
#include "wlan_hdd_cfg80211.h"
#include "os_if_ll_sap.h"

const struct nla_policy
	wlan_hdd_ll_lt_sap_transport_switch_policy
	[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_MAX + 1] = {
		[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_TYPE] = {
						.type = NLA_U8},
		[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_STATUS] = {
						.type = NLA_U8},
};

/**
 * __wlan_hdd_cfg80211_ll_lt_sap_transport_switch() - Request to switch the
 * transport
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int
__wlan_hdd_cfg80211_ll_lt_sap_transport_switch(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct wlan_objmgr_vdev *vdev;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_MAX + 1];
	enum qca_wlan_audio_transport_switch_type transport_switch_type;
	enum qca_wlan_audio_transport_switch_status transport_switch_status;
	QDF_STATUS status;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	if (wlan_hdd_validate_vdev_id(adapter->deflink->vdev_id))
		return -EINVAL;

	if (!policy_mgr_is_vdev_ll_lt_sap(hdd_ctx->psoc,
					  adapter->deflink->vdev_id)) {
		hdd_err("Command not allowed on vdev %d",
			adapter->deflink->vdev_id);
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(
			tb, QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_MAX,
			data, data_len,
			wlan_hdd_ll_lt_sap_transport_switch_policy)) {
		hdd_err("vdev %d Invalid attribute", adapter->deflink->vdev_id);
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_TYPE]) {
		hdd_err("Vdev %d attr transport switch type failed",
			adapter->deflink->vdev_id);
		return -EINVAL;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(hdd_ctx->psoc,
						    adapter->deflink->vdev_id,
						    WLAN_LL_SAP_ID);
	if (!vdev) {
		hdd_err("vdev %d not found", adapter->deflink->vdev_id);
		return -EINVAL;
	}

	transport_switch_type = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_TYPE]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_STATUS]) {
		status = osif_ll_lt_sap_request_for_audio_transport_switch(
						vdev,
						transport_switch_type);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LL_SAP_ID);
		hdd_debug("Transport switch request type %d status %d vdev %d",
			  transport_switch_type, status,
			  adapter->deflink->vdev_id);
		return qdf_status_to_os_return(status);
	}

	transport_switch_status = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_STATUS]);

	/* Deliver the switch response */
	status = osif_ll_lt_sap_deliver_audio_transport_switch_resp(
						vdev,
						transport_switch_type,
						transport_switch_status);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LL_SAP_ID);

	return qdf_status_to_os_return(status);
}

int wlan_hdd_cfg80211_ll_lt_sap_transport_switch(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_ll_lt_sap_transport_switch(wiphy, wdev,
							       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
