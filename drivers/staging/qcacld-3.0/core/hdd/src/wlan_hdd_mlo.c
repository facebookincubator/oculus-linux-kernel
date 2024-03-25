/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_mlo.c
 *
 * WLAN Host Device Driver file for 802.11be (Extremely High Throughput)
 * support.
 *
 */
#include "wlan_hdd_main.h"
#include "wlan_hdd_mlo.h"
#include "osif_vdev_sync.h"
#include "wlan_osif_features.h"
#include "wlan_dp_ucfg_api.h"
#include "wlan_psoc_mlme_ucfg_api.h"
#include "wlan_osif_request_manager.h"
#include "wlan_hdd_object_manager.h"

/*max time in ms, caller may wait for link state request get serviced */
#define WLAN_WAIT_TIME_LINK_STATE 800

#if defined(CFG80211_11BE_BASIC)
#ifdef CFG80211_IFTYPE_MLO_LINK_SUPPORT

static
void wlan_hdd_register_ml_link(struct hdd_adapter *sta_adapter,
			       struct hdd_adapter *link_adapter)
{
	int ret;

	link_adapter->wdev.iftype = NL80211_IFTYPE_MLO_LINK;
	mutex_lock(&sta_adapter->wdev.mtx);
	ret = cfg80211_register_sta_mlo_link(&sta_adapter->wdev,
					     &link_adapter->wdev);
	mutex_unlock(&sta_adapter->wdev.mtx);

	if (ret) {
		hdd_err("Failed to register ml link wdev %d", ret);
		return;
	}
}

static
void wlan_hdd_unregister_ml_link(struct hdd_adapter *link_adapter,
				 bool rtnl_held)
{
	if (rtnl_held)
		rtnl_unlock();

	cfg80211_unregister_wdev(&link_adapter->wdev);

	if (rtnl_held)
		rtnl_lock();
}
#else
static
void wlan_hdd_register_ml_link(struct hdd_adapter *sta_adapter,
			       struct hdd_adapter *link_adapter)
{
}

static
void wlan_hdd_unregister_ml_link(struct hdd_adapter *link_adapter,
				 bool rtnl_held)
{
}
#endif

void hdd_register_wdev(struct hdd_adapter *sta_adapter,
		       struct hdd_adapter *link_adapter,
		       struct hdd_adapter_create_param *adapter_params)
{
	int  i;

	hdd_enter_dev(sta_adapter->dev);
	/* Set the relation between adapters*/
	wlan_hdd_register_ml_link(sta_adapter, link_adapter);
	sta_adapter->mlo_adapter_info.is_ml_adapter = true;
	sta_adapter->mlo_adapter_info.is_link_adapter = false;
	link_adapter->mlo_adapter_info.is_link_adapter = true;
	link_adapter->mlo_adapter_info.is_ml_adapter = false;
	link_adapter->mlo_adapter_info.ml_adapter = sta_adapter;
	link_adapter->mlo_adapter_info.associate_with_ml_adapter =
				      adapter_params->associate_with_ml_adapter;
	qdf_set_bit(WDEV_ONLY_REGISTERED, &link_adapter->event_flags);

	for (i = 0; i < WLAN_MAX_MLD; i++) {
		if (sta_adapter->mlo_adapter_info.link_adapter[i])
			continue;
		sta_adapter->mlo_adapter_info.link_adapter[i] = link_adapter;
		break;
	}

	qdf_mem_copy(link_adapter->mld_addr.bytes, sta_adapter->mld_addr.bytes,
		     QDF_MAC_ADDR_SIZE);
	hdd_exit();
}

static
void hdd_mlo_close_adapter(struct hdd_adapter *link_adapter, bool rtnl_held)
{
	struct osif_vdev_sync *vdev_sync;

	vdev_sync = osif_vdev_sync_unregister(link_adapter->dev);
	if (vdev_sync)
		osif_vdev_sync_wait_for_ops(vdev_sync);

	hdd_check_for_net_dev_ref_leak(link_adapter);
	wlan_hdd_release_intf_addr(link_adapter->hdd_ctx,
				   link_adapter->mac_addr.bytes);
	policy_mgr_clear_concurrency_mode(link_adapter->hdd_ctx->psoc,
					  link_adapter->device_mode);
	link_adapter->wdev.netdev = NULL;

	wlan_hdd_unregister_ml_link(link_adapter, rtnl_held);
	free_netdev(link_adapter->dev);

	if (vdev_sync)
		osif_vdev_sync_destroy(vdev_sync);
}

QDF_STATUS hdd_wlan_unregister_mlo_interfaces(struct hdd_adapter *adapter,
					      bool rtnl_held)
{
	int i;
	struct hdd_mlo_adapter_info *mlo_adapter_info;
	struct hdd_adapter *link_adapter;

	mlo_adapter_info = &adapter->mlo_adapter_info;

	if (mlo_adapter_info->is_link_adapter) {
		if (!qdf_is_macaddr_equal(&adapter->mac_addr,
					  &adapter->mld_addr)) {
			ucfg_dp_destroy_intf(adapter->hdd_ctx->psoc,
					     &adapter->mac_addr);
		}
		hdd_remove_front_adapter(adapter->hdd_ctx, &adapter);
		return QDF_STATUS_E_AGAIN;
	}

	for (i = 0; i < WLAN_MAX_MLD; i++) {
		link_adapter = mlo_adapter_info->link_adapter[i];
		if (!link_adapter)
			continue;
		if (!qdf_is_macaddr_equal(&link_adapter->mac_addr,
					  &link_adapter->mld_addr)) {
			ucfg_dp_destroy_intf(link_adapter->hdd_ctx->psoc,
					     &link_adapter->mac_addr);
		}
		hdd_cleanup_conn_info(link_adapter);
		hdd_remove_adapter(link_adapter->hdd_ctx, link_adapter);
		hdd_mlo_close_adapter(link_adapter, rtnl_held);
	}

	return QDF_STATUS_SUCCESS;
}

void hdd_wlan_register_mlo_interfaces(struct hdd_context *hdd_ctx)
{
	uint8_t *mac_addr;
	struct hdd_adapter_create_param params = {0};
	QDF_STATUS status;

	mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_STA_MODE);
	if (mac_addr) {
		/* if target supports MLO create a new dev */
		params.only_wdev_register = true;
		params.associate_with_ml_adapter = false;
		status = hdd_open_adapter_no_trans(hdd_ctx,
						   QDF_STA_MODE,
						   "null", mac_addr,
						   &params);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("Failed to register link adapter:%d", status);
	}

	qdf_mem_zero(&params, sizeof(params));
	params.only_wdev_register  = true;
	params.associate_with_ml_adapter = true;
	mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_STA_MODE);
	if (mac_addr) {
		/* if target supports MLO create a new dev */
		status = hdd_open_adapter_no_trans(hdd_ctx, QDF_STA_MODE,
						   "null", mac_addr, &params);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("Failed to register link adapter:%d", status);
	}
}

#ifdef CFG80211_MLD_MAC_IN_WDEV
static inline
void wlan_hdd_populate_mld_address(struct hdd_adapter *adapter,
				   uint8_t *mld_addr)
{
	qdf_mem_copy(adapter->wdev.mld_address, mld_addr,
		     QDF_NET_MAC_ADDR_MAX_LEN);
}
#else
static inline
void wlan_hdd_populate_mld_address(struct hdd_adapter *adapter,
				   uint8_t *mld_addr)
{
}
#endif
void
hdd_adapter_set_ml_adapter(struct hdd_adapter *adapter)
{
	adapter->mlo_adapter_info.is_ml_adapter = true;
}

void
hdd_adapter_set_sl_ml_adapter(struct hdd_adapter *adapter)
{
	adapter->mlo_adapter_info.is_single_link_ml = true;
}

struct hdd_adapter *hdd_get_ml_adapter(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER_BY_VDEV;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (hdd_adapter_is_ml_adapter(adapter)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

void hdd_mlo_t2lm_register_callback(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev || !vdev->mlo_dev_ctx)
		return;

	wlan_register_t2lm_link_update_notify_handler(
			hdd_mlo_dev_t2lm_notify_link_update,
			vdev->mlo_dev_ctx);
}

void hdd_mlo_t2lm_unregister_callback(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev || !vdev->mlo_dev_ctx)
		return;

	wlan_unregister_t2lm_link_update_notify_handler(vdev->mlo_dev_ctx, 0);
}

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
int hdd_update_vdev_mac_address(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter,
				struct qdf_mac_addr mac_addr)
{
	int i, ret = 0;
	struct hdd_mlo_adapter_info *mlo_adapter_info;
	struct hdd_adapter *link_adapter;
	bool eht_capab;

	ucfg_psoc_mlme_get_11be_capab(hdd_ctx->psoc, &eht_capab);
	if (hdd_adapter_is_ml_adapter(adapter) && eht_capab) {
		if (hdd_adapter_is_sl_ml_adapter(adapter)) {
			ret = hdd_dynamic_mac_address_set(hdd_ctx, adapter,
							  mac_addr);
			return ret;
		}
		mlo_adapter_info = &adapter->mlo_adapter_info;

		for (i = 0; i < WLAN_MAX_MLD; i++) {
			link_adapter = mlo_adapter_info->link_adapter[i];
			if (!link_adapter)
				continue;
			ret = hdd_dynamic_mac_address_set(hdd_ctx, link_adapter,
							  mac_addr);
			if (ret)
				return ret;
		}
	} else {
		ret = hdd_dynamic_mac_address_set(hdd_ctx, adapter, mac_addr);
	}

	return ret;
}
#endif

const struct nla_policy
ml_link_state_request_policy[QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE] = {.type = NLA_U32},
};

static int
__wlan_hdd_cfg80211_process_ml_link_state(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	int ret = 0;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct wlan_objmgr_vdev *vdev;

	hdd_enter_dev(wdev->netdev);

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);

	if (!vdev)
		return -EINVAL;

	wlan_handle_mlo_link_state_operation(wiphy, vdev, data, data_len);

	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);

	return ret;
}

int wlan_hdd_cfg80211_process_ml_link_state(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_process_ml_link_state(wiphy, wdev, data,
							  data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static inline void ml_link_state_resp_cb(struct ml_link_state_info_event *ev,
					 void *cookie)
{
	struct ml_link_state_info_event *priv;
	struct osif_request *request;

	request = osif_request_get(cookie);

	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);

	qdf_mem_copy(priv, ev, sizeof(*priv));
	osif_request_complete(request);
	osif_request_put(request);
}

static uint32_t
hdd_get_ml_link_state_response_len(const struct ml_link_state_info_event *event)
{
	uint32_t len = 0;
	uint32_t info_len = 0;

	len = NLMSG_HDRLEN;
	/* QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE */
	len += NLA_HDRLEN + sizeof(u32);

	/* QCA_WLAN_VENDOR_ATTR_LINK_STATE_OPERATION_MODE */
	len += NLA_HDRLEN + sizeof(u32);

	/* nest */
	info_len = NLA_HDRLEN;
	/* QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID */
	info_len += NLA_HDRLEN + sizeof(u8);
	/* QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE */
	info_len += NLA_HDRLEN + sizeof(u32);

	/* QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG */
	len += NLA_HDRLEN + (info_len * event->num_mlo_vdev_link_info);

	return len;
}

static int
hdd_ml_generate_link_state_resp_nlmsg(struct sk_buff *skb,
				      struct ml_link_state_info_event *params,
				      uint32_t num_link_info)
{
	struct nlattr *nla_config_attr, *nla_config_params;
	uint32_t i = 0, attr;
	int errno;
	uint32_t value;

	attr = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE;

	/* Default control mode is only supported */
	value = QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT;
	errno = nla_put_u32(skb, attr, value);
	if (errno)
		return errno;

	attr = QCA_WLAN_VENDOR_ATTR_LINK_STATE_OPERATION_MODE;

	/* Default link state operation mode is only supported */
	value = QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_DEFAULT;
	errno = nla_put_u32(skb, attr, value);
	if (errno)
		return errno;

	attr = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG;
	nla_config_attr = nla_nest_start(skb, attr);

	if (!nla_config_attr)
		return -EINVAL;

	for (i = 0; i < num_link_info; i++) {
		nla_config_params = nla_nest_start(skb, attr);
		if (!nla_config_params)
			return -EINVAL;

		attr = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID;
		value = params->link_info[i].link_id;
		errno = nla_put_u8(skb, attr, value);
		if (errno)
			return errno;

		attr = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE;
		value = params->link_info[i].link_status;
		errno = nla_put_u32(skb, attr, value);

		if (errno)
			return errno;

		nla_nest_end(skb, nla_config_params);
	}

	nla_nest_end(skb, nla_config_attr);

	return 0;
}

static QDF_STATUS wlan_hdd_link_state_request(struct wiphy *wiphy,
					      struct wlan_objmgr_vdev *vdev)
{
	int errno;
	int skb_len;
	struct sk_buff *reply_skb = NULL;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	void *cookie;
	struct ml_link_state_info_event *link_state_event = NULL;
	struct osif_request *request;
	struct ml_link_state_cmd_info info = {0};
	int num_info = 0;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*link_state_event),
		.timeout_ms = WLAN_WAIT_TIME_LINK_STATE,
		.dealloc = NULL,
	};

	if (!wiphy || !vdev)
		return status;

	request = osif_request_alloc(&params);
	if (!request)
		return QDF_STATUS_E_NOMEM;

	cookie = osif_request_cookie(request);
	link_state_event = osif_request_priv(request);

	info.request_cookie = cookie;
	info.ml_link_state_resp_cb = ml_link_state_resp_cb;

	status = mlo_get_link_state_register_resp_cb(vdev,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to register resp callback: %d", status);
		status = qdf_status_to_os_return(status);
		goto free_event;
	}

	status = ml_post_get_link_state_msg(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to post scheduler msg");
		goto free_event;
		return status;
	}

	status = osif_request_wait_for_response(request);
	if (status) {
		hdd_err("wait failed or timed out ret: %d", status);
		goto free_event;
	}

	hdd_debug("ml_link_state_resp: vdev id %d status %d num %d MAC addr " QDF_MAC_ADDR_FMT,
		  link_state_event->vdev_id, link_state_event->status,
		  link_state_event->num_mlo_vdev_link_info,
		  QDF_MAC_ADDR_REF(link_state_event->mldaddr.bytes));

	for (num_info = 0; num_info < link_state_event->num_mlo_vdev_link_info;
	     num_info++) {
		hdd_debug("ml_link_state_resp: chan_freq %d vdev_id %d link_id %d link_status %d",
			  link_state_event->link_info[num_info].chan_freq,
			  link_state_event->link_info[num_info].vdev_id,
			  link_state_event->link_info[num_info].link_id,
			  link_state_event->link_info[num_info].link_status);
	}

	skb_len = hdd_get_ml_link_state_response_len(link_state_event);

	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(
						wiphy,
						skb_len);
	if (!reply_skb) {
		hdd_err("Get stats - alloc reply_skb failed");
		status = QDF_STATUS_E_NOMEM;
		goto free_event;
	}

	status = hdd_ml_generate_link_state_resp_nlmsg(
			reply_skb, link_state_event,
			link_state_event->num_mlo_vdev_link_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to pack nl response");
		goto free_skb;
	}

	osif_request_put(request);

	errno = wlan_cfg80211_vendor_cmd_reply(reply_skb);
	return qdf_status_from_os_return(errno);

free_skb:
	wlan_cfg80211_vendor_free_skb(reply_skb);
free_event:
	osif_request_put(request);

	return status;
}

QDF_STATUS
wlan_handle_mlo_link_state_operation(struct wiphy *wiphy,
				     struct wlan_objmgr_vdev *vdev,
				     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX + 1];
	enum qca_wlan_vendor_link_state_op_types ml_link_op;
	struct nlattr *link_oper_attr;
	uint32_t id;
	int ret = 0;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX,
				    data,
				    data_len,
				    ml_link_state_request_policy)) {
		hdd_err_rl("invalid twt attr");
		return -EINVAL;
	}

	id = QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE;
	link_oper_attr = tb[id];
	if (!link_oper_attr) {
		hdd_err_rl("link state operation NOT specified");
		return -EINVAL;
	}

	ml_link_op = nla_get_u8(link_oper_attr);

	hdd_debug("ml link state request:%d", ml_link_op);
	switch (ml_link_op) {
	case QCA_WLAN_VENDOR_LINK_STATE_OP_GET:
		ret = wlan_hdd_link_state_request(wiphy, vdev);
		break;
	case QCA_WLAN_VENDOR_LINK_STATE_OP_SET:
		hdd_debug_rl("ml link SET state not supported");
		break;
	default:
		hdd_err_rl("Invalid link state operation");
		ret = -EINVAL;
		break;
	}

	return ret;
}

#endif
