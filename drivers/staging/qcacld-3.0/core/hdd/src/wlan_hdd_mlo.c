/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <wlan_osif_priv.h>

/*max time in ms, caller may wait for link state request get serviced */
#define WLAN_WAIT_TIME_LINK_STATE 3000

#if defined(CFG80211_11BE_BASIC)
#ifndef WLAN_HDD_MULTI_VDEV_SINGLE_NDEV
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
		ucfg_dp_destroy_intf(adapter->hdd_ctx->psoc,
				     &adapter->mac_addr);
		hdd_remove_front_adapter(adapter->hdd_ctx, &adapter);
		return QDF_STATUS_E_AGAIN;
	}

	for (i = 0; i < WLAN_MAX_MLD; i++) {
		link_adapter = mlo_adapter_info->link_adapter[i];
		if (!link_adapter)
			continue;
		hdd_cleanup_conn_info(link_adapter->deflink);
		ucfg_dp_destroy_intf(link_adapter->hdd_ctx->psoc,
				     &link_adapter->mac_addr);
		hdd_remove_adapter(link_adapter->hdd_ctx, link_adapter);
		hdd_mlo_close_adapter(link_adapter, rtnl_held);
	}

	return QDF_STATUS_SUCCESS;
}

void hdd_wlan_register_mlo_interfaces(struct hdd_context *hdd_ctx)
{
	int i = 0;
	QDF_STATUS status;
	struct hdd_adapter *ml_adapter;
	struct wlan_hdd_link_info *link_info;
	struct hdd_adapter_create_param params = {0};
	struct qdf_mac_addr link_addr[WLAN_MAX_ML_BSS_LINKS] = {0};

	ml_adapter = hdd_get_ml_adapter(hdd_ctx);
	if (!ml_adapter)
		return;

	status = hdd_derive_link_address_from_mld(hdd_ctx->psoc,
						  &ml_adapter->mld_addr,
						  &link_addr[0],
						  WLAN_MAX_ML_BSS_LINKS);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	/* if target supports MLO create a new dev */
	params.only_wdev_register = true;
	params.associate_with_ml_adapter = true;
	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_STA_MODE, "null",
					   link_addr[0].bytes, &params);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to register link adapter:%d", status);

	qdf_mem_zero(&params, sizeof(params));
	params.only_wdev_register  = true;
	params.associate_with_ml_adapter = false;
	/* if target supports MLO create a new dev */
	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_STA_MODE, "null",
					   link_addr[1].bytes, &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to register link adapter:%d", status);
	} else {
		hdd_adapter_for_each_link_info(ml_adapter, link_info) {
			qdf_copy_macaddr(&link_info->link_addr,
					 &link_addr[i++]);
		}
	}
}

void
hdd_adapter_set_sl_ml_adapter(struct hdd_adapter *adapter)
{
	adapter->mlo_adapter_info.is_single_link_ml = true;
}

void
hdd_adapter_clear_sl_ml_adapter(struct hdd_adapter *adapter)
{
	adapter->mlo_adapter_info.is_single_link_ml = false;
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
#endif

#ifndef WLAN_HDD_MULTI_VDEV_SINGLE_NDEV
void hdd_adapter_set_ml_adapter(struct hdd_adapter *adapter)
{
	adapter->mlo_adapter_info.is_ml_adapter = true;
	qdf_copy_macaddr(&adapter->mld_addr, &adapter->mac_addr);
}
#else
void hdd_adapter_set_ml_adapter(struct hdd_adapter *adapter)
{
	adapter->mlo_adapter_info.is_ml_adapter = true;
}

static struct mlo_osif_ext_ops mlo_osif_ops = {
	.mlo_mgr_osif_update_bss_info = hdd_cm_save_connected_links_info,
	.mlo_mgr_osif_update_mac_addr = hdd_link_switch_vdev_mac_addr_update,
	.mlo_mgr_osif_link_switch_notification =
					hdd_adapter_link_switch_notification,
};

QDF_STATUS hdd_mlo_mgr_register_osif_ops(void)
{
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	return wlan_mlo_mgr_register_osif_ext_ops(mlo_mgr_ctx, &mlo_osif_ops);
}

QDF_STATUS hdd_mlo_mgr_unregister_osif_ops(void)
{
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	return wlan_mlo_mgr_unregister_osif_ext_ops(mlo_mgr_ctx);
}

QDF_STATUS hdd_adapter_link_switch_notification(struct wlan_objmgr_vdev *vdev,
						uint8_t non_trans_vdev_id)
{
	bool found = false;
	struct hdd_adapter *adapter;
	struct vdev_osif_priv *osif_priv;
	struct wlan_hdd_link_info *link_info, *iter_link_info;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		hdd_err("Invalid osif priv");
		return QDF_STATUS_E_INVAL;
	}

	link_info = osif_priv->legacy_osif_priv;
	adapter = link_info->adapter;

	if (link_info->vdev_id != adapter->deflink->vdev_id) {
		hdd_err("Deflink VDEV %d not equals current VDEV %d",
			adapter->deflink->vdev_id, link_info->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	hdd_adapter_for_each_link_info(adapter, iter_link_info) {
		if (non_trans_vdev_id == iter_link_info->vdev_id) {
			adapter->deflink = iter_link_info;
			found = true;
			break;
		}
	}

	if (!found)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#endif

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

QDF_STATUS hdd_derive_link_address_from_mld(struct wlan_objmgr_psoc *psoc,
					    struct qdf_mac_addr *mld_addr,
					    struct qdf_mac_addr *link_addr_list,
					    uint8_t max_idx)
{
	uint8_t last_byte, temp_byte, idx, start_idx = 0;
	struct qdf_mac_addr new_addr;
	struct qdf_mac_addr *link_addr;

	if (!psoc || !mld_addr || !link_addr_list || !max_idx ||
	    max_idx > WLAN_MAX_ML_BSS_LINKS || qdf_is_macaddr_zero(mld_addr)) {
		hdd_err("Invalid values");
		return QDF_STATUS_E_INVAL;
	}

	qdf_copy_macaddr(&new_addr, mld_addr);
	/* Set locally administered bit */
	new_addr.bytes[0] |= 0x02;

	link_addr = link_addr_list;
	last_byte = mld_addr->bytes[5];
	hdd_debug("MLD addr: " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(mld_addr->bytes));

	if (wlan_mlme_get_sta_same_link_mld_addr(psoc)) {
		qdf_copy_macaddr(link_addr, mld_addr);
		link_addr++;
		start_idx++;
	}

	for (idx = start_idx; idx < max_idx; idx++) {
		temp_byte = ((last_byte >> 4 & INTF_MACADDR_MASK) + idx) &
			     INTF_MACADDR_MASK;
		new_addr.bytes[5] = last_byte + temp_byte;
		new_addr.bytes[5] ^= (1 << 7);

		qdf_copy_macaddr(link_addr, &new_addr);
		link_addr++;
		hdd_debug("Derived link addr: " QDF_MAC_ADDR_FMT ", idx: %d",
			  QDF_MAC_ADDR_REF(new_addr.bytes), idx);
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
#ifdef WLAN_HDD_MULTI_VDEV_SINGLE_NDEV
static void hdd_adapter_restore_link_vdev_map(struct hdd_adapter *adapter)
{
	int i;
	unsigned long link_flags;
	uint8_t vdev_id, cur_link_idx, temp_link_idx;
	struct vdev_osif_priv *osif_priv;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_hdd_link_info *temp_link_info, *link_info;

	hdd_adapter_for_each_link_info(adapter, link_info) {
		cur_link_idx = hdd_adapter_get_index_of_link_info(link_info);
		/* If the current index matches the current pos in mapping
		 * then the link info is in same position
		 */
		if (adapter->curr_link_info_map[cur_link_idx] == cur_link_idx)
			continue;

		/* Find the index where current link info is moved to perform
		 * VDEV info swap.
		 */
		for (i = cur_link_idx + 1; i < WLAN_MAX_ML_BSS_LINKS; i++) {
			if (adapter->curr_link_info_map[i] == cur_link_idx) {
				temp_link_idx = i;
				break;
			}
		}

		if (i == WLAN_MAX_ML_BSS_LINKS)
			continue;

		temp_link_info = &adapter->link_info[temp_link_idx];

		/* Move VDEV info from current link info */
		qdf_spin_lock_bh(&temp_link_info->vdev_lock);
		vdev = temp_link_info->vdev;
		vdev_id = temp_link_info->vdev_id;
		temp_link_info->vdev = link_info->vdev;
		temp_link_info->vdev_id = link_info->vdev_id;
		qdf_spin_unlock_bh(&temp_link_info->vdev_lock);

		/* Update VDEV-OSIF priv pointer to new link info. */
		if (temp_link_info->vdev) {
			osif_priv = wlan_vdev_get_ospriv(temp_link_info->vdev);
			if (osif_priv)
				osif_priv->legacy_osif_priv = temp_link_info;
		}

		/* Fill current link info's actual VDEV info */
		qdf_spin_lock_bh(&link_info->vdev_lock);
		link_info->vdev = vdev;
		link_info->vdev_id = vdev_id;
		qdf_spin_unlock_bh(&link_info->vdev_lock);

		/* Update VDEV-OSIF priv pointer to new link info. */
		if (link_info->vdev) {
			osif_priv = wlan_vdev_get_ospriv(link_info->vdev);
			if (osif_priv)
				osif_priv->legacy_osif_priv = link_info;
		}

		/* Swap link flags */
		link_flags = temp_link_info->link_flags;
		temp_link_info->link_flags = link_info->link_flags;
		link_info->link_flags = link_flags;

		/* Update the mapping, current link info's mapping will be
		 * set to be proper.
		 */
		adapter->curr_link_info_map[temp_link_idx] =
				adapter->curr_link_info_map[cur_link_idx];
		adapter->curr_link_info_map[cur_link_idx] = cur_link_idx;
	}
	hdd_adapter_disable_all_links(adapter);
}

int hdd_update_vdev_mac_address(struct hdd_adapter *adapter,
				struct qdf_mac_addr mac_addr)
{
	int idx, i, ret = 0;
	bool eht_capab, update_self_peer;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wlan_hdd_link_info *link_info;
	uint8_t *addr_list[WLAN_MAX_ML_BSS_LINKS + 1] = {0};
	struct qdf_mac_addr link_addrs[WLAN_MAX_ML_BSS_LINKS] = {0};

	/* This API is only called with is ml adapter set for STA mode adapter.
	 * For SAP mode, hdd_hostapd_set_mac_address() is the entry point for
	 * MAC address update.
	 */
	ucfg_psoc_mlme_get_11be_capab(hdd_ctx->psoc, &eht_capab);
	if (!(eht_capab && hdd_adapter_is_ml_adapter(adapter))) {
		struct qdf_mac_addr mld_addr = QDF_MAC_ADDR_ZERO_INIT;

		ret = hdd_dynamic_mac_address_set(adapter->deflink, mac_addr,
						  mld_addr, true);
		return ret;
	}

	status = hdd_derive_link_address_from_mld(hdd_ctx->psoc,
						  &mac_addr, &link_addrs[0],
						  WLAN_MAX_ML_BSS_LINKS);

	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	hdd_adapter_restore_link_vdev_map(adapter);

	i = 0;
	hdd_adapter_for_each_active_link_info(adapter, link_info) {
		idx = hdd_adapter_get_index_of_link_info(link_info);
		addr_list[i++] = &link_addrs[idx].bytes[0];
	}

	status = sme_check_for_duplicate_session(hdd_ctx->mac_handle,
						 &addr_list[0]);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	i = 0;
	hdd_adapter_for_each_link_info(adapter, link_info)
		qdf_copy_macaddr(&link_info->link_addr, &link_addrs[i++]);

	hdd_adapter_for_each_active_link_info(adapter, link_info) {
		idx = hdd_adapter_get_index_of_link_info(link_info);
		update_self_peer =
			(link_info == adapter->deflink) ? true : false;
		ret = hdd_dynamic_mac_address_set(link_info, link_addrs[idx],
						  mac_addr, update_self_peer);
		if (ret)
			return ret;

		qdf_copy_macaddr(&link_info->link_addr, &link_addrs[idx]);
	}

	hdd_adapter_update_mlo_mgr_mac_addr(adapter);
	return ret;
}
#else
int hdd_update_vdev_mac_address(struct hdd_adapter *adapter,
				struct qdf_mac_addr mac_addr)
{
	int i, ret = 0;
	QDF_STATUS status;
	bool eht_capab, update_self_peer;
	struct hdd_adapter *link_adapter;
	struct hdd_mlo_adapter_info *mlo_adapter_info;
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	uint8_t *addr_list[WLAN_MAX_MLD + 1] = {0};
	struct qdf_mac_addr link_addrs[WLAN_MAX_ML_BSS_LINKS] = {0};

	/* This API is only called with is ml adapter set for STA mode adapter.
	 * For SAP mode, hdd_hostapd_set_mac_address() is the entry point for
	 * MAC address update.
	 */
	ucfg_psoc_mlme_get_11be_capab(hdd_ctx->psoc, &eht_capab);
	if (!(eht_capab && hdd_adapter_is_ml_adapter(adapter))) {
		struct qdf_mac_addr mld_addr = QDF_MAC_ADDR_ZERO_INIT;

		ret = hdd_dynamic_mac_address_set(adapter->deflink, mac_addr,
						  mld_addr, true);
		return ret;
	}

	status = hdd_derive_link_address_from_mld(hdd_ctx->psoc,
						  &mac_addr, &link_addrs[0],
						  WLAN_MAX_ML_BSS_LINKS);

	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	for (i = 0; i < WLAN_MAX_MLD; i++)
		addr_list[i] = &link_addrs[i].bytes[0];

	status = sme_check_for_duplicate_session(hdd_ctx->mac_handle,
						 &addr_list[0]);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	mlo_adapter_info = &adapter->mlo_adapter_info;
	for (i = 0; i < WLAN_MAX_MLD; i++) {
		link_adapter = mlo_adapter_info->link_adapter[i];
		if (!link_adapter)
			continue;

		if (hdd_adapter_is_associated_with_ml_adapter(link_adapter))
			update_self_peer = true;
		else
			update_self_peer = false;

		ret = hdd_dynamic_mac_address_set(link_adapter->deflink,
						  link_addrs[i], mac_addr,
						  update_self_peer);
		if (ret)
			return ret;

		/* Update DP intf and new link address in link adapter
		 */
		ucfg_dp_update_intf_mac(hdd_ctx->psoc, &link_adapter->mac_addr,
					&link_addrs[i],
					link_adapter->deflink->vdev);
		qdf_copy_macaddr(&link_adapter->mac_addr, &link_addrs[i]);
		qdf_copy_macaddr(&adapter->link_info[i].link_addr,
				 &link_addrs[i]);
	}

	qdf_copy_macaddr(&adapter->link_info[i].link_addr, &link_addrs[i]);
	hdd_adapter_update_mlo_mgr_mac_addr(adapter);

	return ret;
}
#endif
#endif /* WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE */

const struct nla_policy
ml_link_state_config_policy [QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID] =  {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE] =    {.type = NLA_U32},
};

const struct nla_policy
ml_link_state_request_policy[QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_LINK_STATE_MIXED_MODE_ACTIVE_NUM_LINKS] = {
							.type = NLA_U8},
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
	struct hdd_context *hdd_ctx = NULL;

	hdd_enter_dev(wdev->netdev);

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx)
		return -EINVAL;

	if (adapter->device_mode != QDF_STA_MODE)
		return -EINVAL;

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_OSIF_ID);

	if (!vdev)
		return -EINVAL;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev))
		goto release_ref;

	ret = wlan_handle_mlo_link_state_operation(adapter, wiphy, vdev,
						   hdd_ctx, data, data_len);

release_ref:
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
				      struct wlan_objmgr_psoc *psoc,
				      struct ml_link_state_info_event *params,
				      uint32_t num_link_info)
{
	struct nlattr *nla_config_attr, *nla_config_params;
	uint32_t i = 0, attr;
	int errno;
	uint32_t value;

	attr = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE;
	value = ucfg_mlme_get_ml_link_control_mode(psoc, params->vdev_id);
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

static char *link_state_status_id_to_str(uint32_t status)
{
	switch (status) {
	case WLAN_LINK_INFO_EVENT_SUCCESS:
		return "LINK_INFO_EVENT_SUCCESS";
	case WLAN_LINK_INFO_EVENT_REJECT_FAILURE:
		return "LINK_INFO_EVENT_REJECT_FAILURE";
	case WLAN_LINK_INFO_EVENT_REJECT_VDEV_NOT_UP:
		return "LINK_INFO_EVENT_REJECT_VDEV_NOT_UP";
	case WLAN_LINK_INFO_EVENT_REJECT_ROAMING_IN_PROGRESS:
		return "LINK_INFO_EVENT_REJECT_ROAMING_IN_PROGRESS";
	case WLAN_LINK_INFO_EVENT_REJECT_NON_MLO_CONNECTION:
		return "LINK_INFO_EVENT_REJECT_NON_MLO_CONNECTION";
	}
	return "Undefined link state status ID";
}

static bool
wlan_hdd_link_state_request_needed(struct hdd_adapter *adapter)
{
	qdf_time_t link_state_cached_duration = 0;

	link_state_cached_duration =
				qdf_system_ticks_to_msecs(qdf_system_ticks()) -
				adapter->link_state_cached_timestamp;
	if (link_state_cached_duration <=
		adapter->hdd_ctx->config->link_state_cache_expiry_time)
		return false;

	return true;
}

#ifdef WLAN_FEATURE_11BE_MLO

void hdd_update_link_state_cached_timestamp(struct hdd_adapter *adapter)
{
	adapter->link_state_cached_timestamp =
		qdf_system_ticks_to_msecs(qdf_system_ticks());
}
#endif /* WLAN_FEATURE_11BE_MLO */

static QDF_STATUS
wlan_hdd_cached_link_state_request(struct hdd_adapter *adapter,
				   struct wiphy *wiphy,
				   struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct ml_link_state_info_event link_state_event = {0};
	struct wlan_hdd_link_info *link_info;
	struct hdd_station_ctx *sta_ctx;
	int skb_len;
	struct sk_buff *reply_skb = NULL;
	int errno;
	struct qdf_mac_addr *mld_addr;
	uint8_t link_iter = 0;
	struct mlo_link_info *ml_link_info;
	struct wlan_mlo_dev_context *mlo_ctx;

	mlo_ctx = vdev->mlo_dev_ctx;
	if (!mlo_ctx) {
		hdd_err("null mlo_dev_ctx");
		return -EINVAL;
	}

	hdd_adapter_for_each_link_info(adapter, link_info) {

		if (link_iter >= WLAN_MAX_ML_BSS_LINKS) {
			hdd_err("Invalid number of link info");
			return -EINVAL;
		}

		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(link_info);
		link_state_event.link_info[link_iter].link_id =
				sta_ctx->conn_info.ieee_link_id;
		link_state_event.link_info[link_iter].vdev_id =
				link_info->vdev_id;
		link_state_event.link_info[link_iter].chan_freq =
				sta_ctx->ch_info.freq;

		if (sta_ctx->conn_info.ieee_link_id == WLAN_INVALID_LINK_ID)
			continue;

		ml_link_info = mlo_mgr_get_ap_link_by_link_id(
				mlo_ctx,
				sta_ctx->conn_info.ieee_link_id);
		if (!ml_link_info) {
			hdd_debug("link: %d info does not exist",
				  sta_ctx->conn_info.ieee_link_id);
			return -EINVAL;
		}

		link_state_event.link_info[link_iter].link_status =
			ml_link_info->is_link_active;

		link_iter++;

		hdd_debug_rl("vdev id %d sta_ctx->conn_info.ieee_link_id %d is_mlo_vdev_active %d ",
			     link_info->vdev_id, sta_ctx->conn_info.ieee_link_id,
			     ml_link_info->is_link_active);
	}

	link_state_event.num_mlo_vdev_link_info = link_iter;
	link_state_event.vdev_id = wlan_vdev_get_id(vdev);
	link_state_event.status = 0;
	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
	link_state_event.mldaddr = *mld_addr;

	hdd_debug_rl("cached link_state_resp: vdev id %d status %d num %d MAC addr " QDF_MAC_ADDR_FMT,
		     link_state_event.vdev_id, link_state_event.status,
		     link_state_event.num_mlo_vdev_link_info,
		     QDF_MAC_ADDR_REF(link_state_event.mldaddr.bytes));

	skb_len = hdd_get_ml_link_state_response_len(&link_state_event);

	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy, skb_len);
	if (!reply_skb) {
		hdd_err("Get stats - alloc reply_skb failed");
		status = QDF_STATUS_E_NOMEM;
		return status;
	}

	status = hdd_ml_generate_link_state_resp_nlmsg(
			reply_skb, psoc, &link_state_event,
			link_state_event.num_mlo_vdev_link_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to pack nl response");
		goto free_skb;
	}

	errno = wlan_cfg80211_vendor_cmd_reply(reply_skb);

	return qdf_status_from_os_return(errno);

free_skb:
	wlan_cfg80211_vendor_free_skb(reply_skb);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wlan_hdd_link_state_request(struct hdd_adapter *adapter,
					      struct wiphy *wiphy,
					      struct wlan_objmgr_psoc *psoc,
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

	if (!wiphy || !vdev || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return status;

	if (adapter->device_mode != QDF_STA_MODE)
		return QDF_STATUS_SUCCESS;

	if (!wlan_hdd_link_state_request_needed(adapter)) {
		hdd_debug_rl("sending cached link state request");
		status = wlan_hdd_cached_link_state_request(adapter, wiphy,
							    psoc, vdev);
		return status;
	}

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

	if (QDF_IS_STATUS_ERROR(link_state_event->status)) {
		hdd_debug("ml_link_state_status failed %s",
			  link_state_status_id_to_str(link_state_event->status));
		goto free_event;
	}

	for (num_info = 0; num_info < link_state_event->num_mlo_vdev_link_info;
	     num_info++) {
		hdd_debug("ml_link_state_resp: chan_freq %d vdev_id %d link_id %d link_status %d",
			  link_state_event->link_info[num_info].chan_freq,
			  link_state_event->link_info[num_info].vdev_id,
			  link_state_event->link_info[num_info].link_id,
			  link_state_event->link_info[num_info].link_status);
	}

	hdd_update_link_state_cached_timestamp(adapter);

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
			reply_skb, psoc, link_state_event,
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

#define MLD_MAX_SUPPORTED_LINKS 2

int wlan_handle_mlo_link_state_operation(struct hdd_adapter *adapter,
					 struct wiphy *wiphy,
					 struct wlan_objmgr_vdev *vdev,
					 struct hdd_context *hdd_ctx,
					 const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX + 1];
	enum qca_wlan_vendor_link_state_op_types ml_link_op;
	struct nlattr *link_oper_attr, *mode_attr, *curr_attr, *num_link_attr;
	int rem_len = 0, rc;
	uint32_t attr_id, ml_config_state;
	uint8_t ml_active_num_links, ml_link_control_mode;
	uint8_t ml_config_link_id, num_links = 0;
	uint8_t vdev_id = vdev->vdev_objmgr.vdev_id;
	uint8_t link_id_list[MLD_MAX_SUPPORTED_LINKS] = {0};
	uint32_t config_state_list[MLD_MAX_SUPPORTED_LINKS] = {0};
	QDF_STATUS status;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX,
				    data, data_len,
				    ml_link_state_request_policy)) {
		hdd_debug("vdev %d: invalid mlo link state attr", vdev_id);
		return -EINVAL;
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE;
	link_oper_attr = tb[attr_id];
	if (!link_oper_attr) {
		hdd_debug("vdev %d: link state op not specified", vdev_id);
		return -EINVAL;
	}
	ml_link_op = nla_get_u8(link_oper_attr);
	switch (ml_link_op) {
	case QCA_WLAN_VENDOR_LINK_STATE_OP_GET:
		status = wlan_hdd_link_state_request(adapter, wiphy,
						     hdd_ctx->psoc, vdev);
		return qdf_status_to_os_return(status);
	case QCA_WLAN_VENDOR_LINK_STATE_OP_SET:
		if (policy_mgr_is_set_link_in_progress(hdd_ctx->psoc)) {
			hdd_debug("vdev %d: change link already in progress",
				  vdev_id);
			return -EBUSY;
		}

		break;
	default:
		hdd_debug("vdev %d: Invalid op type:%d", vdev_id, ml_link_op);
		return -EINVAL;
	}

	attr_id = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE;
	mode_attr = tb[attr_id];
	if (!mode_attr) {
		hdd_debug("vdev %d: ml links control mode attr not present",
			  vdev_id);
		return -EINVAL;
	}
	ml_link_control_mode = nla_get_u8(mode_attr);

	switch (ml_link_control_mode) {
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT:
		/* clear mlo link(s) settings in fw as per driver */
		status = policy_mgr_clear_ml_links_settings_in_fw(hdd_ctx->psoc,
								  vdev_id);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;
		break;
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_USER:
		attr_id = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG;
		if (!tb[attr_id]) {
			hdd_debug("vdev %d: state config attr not present",
				  vdev_id);
			return -EINVAL;
		}

		nla_for_each_nested(curr_attr, tb[attr_id], rem_len) {
			rc = wlan_cfg80211_nla_parse_nested(
				tb2, QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX,
				curr_attr,
				ml_link_state_config_policy);
			if (rc) {
				hdd_debug("vdev %d: nested attr not present",
					     vdev_id);
				return -EINVAL;
			}

			attr_id =
				QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID;
			if (!tb2[attr_id]) {
				hdd_debug("vdev %d: link id attr not present",
					  vdev_id);
				return -EINVAL;
			}

			ml_config_link_id = nla_get_u8(tb2[attr_id]);

			attr_id = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE;
			if (!tb2[attr_id]) {
				hdd_debug("vdev %d: config attr not present",
					  vdev_id);
				return -EINVAL;
			}

			ml_config_state = nla_get_u32(tb2[attr_id]);
			hdd_debug("vdev %d: ml_link_id %d, ml_link_state:%d",
				  vdev_id, ml_config_link_id, ml_config_state);
			link_id_list[num_links] = ml_config_link_id;
			config_state_list[num_links] = ml_config_state;
			num_links++;

			if (num_links >= MLD_MAX_SUPPORTED_LINKS)
				break;
		}

		status = policy_mgr_update_mlo_links_based_on_linkid(
						hdd_ctx->psoc,
						vdev_id, num_links,
						link_id_list,
						config_state_list);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;
		break;
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED:
		attr_id =
		   QCA_WLAN_VENDOR_ATTR_LINK_STATE_MIXED_MODE_ACTIVE_NUM_LINKS;
		num_link_attr = tb[attr_id];
		if (!num_link_attr) {
			hdd_debug("number of active state links not specified");
			return -EINVAL;
		}
		ml_active_num_links = nla_get_u8(num_link_attr);
		hdd_debug("vdev %d: ml_active_num_links: %d", vdev_id,
			  ml_active_num_links);
		if (ml_active_num_links > MLD_MAX_SUPPORTED_LINKS)
			return -EINVAL;
		status = policy_mgr_update_active_mlo_num_links(hdd_ctx->psoc,
						vdev_id, ml_active_num_links);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;
		break;
	default:
		hdd_debug("vdev %d: invalid ml_link_control_mode: %d", vdev_id,
			  ml_link_control_mode);
		return -EINVAL;
	}

	ucfg_mlme_set_ml_link_control_mode(hdd_ctx->psoc, vdev_id,
					   ml_link_control_mode);

	hdd_debug("vdev: %d, processed link state command successfully",
		  vdev_id);
	return 0;
}

static uint32_t
hdd_get_t2lm_setup_event_len(void)
{
	uint32_t len = 0;
	uint32_t info_len = 0;

	len = NLMSG_HDRLEN;

	/* QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_AP_MLD_ADDR */
	len += nla_total_size(QDF_MAC_ADDR_SIZE);

	/* nest */
	info_len = NLA_HDRLEN;
	/* QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_UPLINK */
	info_len += NLA_HDRLEN + sizeof(u16);
	/* QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_DOWNLINK */
	info_len += NLA_HDRLEN + sizeof(u16);

	/* QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_STATUS */
	len += NLA_HDRLEN + (info_len * T2LM_MAX_NUM_TIDS);

	return len;
}

static QDF_STATUS
hdd_t2lm_pack_nl_response(struct sk_buff *skb,
			  struct wlan_objmgr_vdev *vdev,
			  struct wlan_t2lm_info *t2lm,
			  struct qdf_mac_addr mld_addr)
{
	struct nlattr *config_attr, *config_params;
	uint32_t i = 0, attr, attr1;
	int errno;
	uint32_t value;
	uint8_t tid_num;

	attr = QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_AP_MLD_ADDR;
	if (nla_put(skb, attr, QDF_MAC_ADDR_SIZE, mld_addr.bytes)) {
		hdd_err("Failed to put mac_addr");
		return QDF_STATUS_E_INVAL;
	}

	if (t2lm->default_link_mapping) {
		hdd_debug("update mld addr for default mapping");
		return QDF_STATUS_SUCCESS;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_STATUS;
	config_attr = nla_nest_start(skb, attr);
	if (!config_attr) {
		hdd_err("nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	switch (t2lm->direction) {
	case WLAN_T2LM_UL_DIRECTION:
		for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
			config_params = nla_nest_start(skb, tid_num + 1);
			if (!config_params)
				return -EINVAL;

			attr1 = QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_UPLINK;
			value = t2lm->ieee_link_map_tid[i];
			errno = nla_put_u16(skb, attr1, value);
			if (errno)
				return errno;

			attr1 = QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_DOWNLINK;
			value = 0;
			errno = nla_put_u16(skb, attr1, value);
			if (errno)
				return errno;
			nla_nest_end(skb, config_params);
		}
		break;
	case WLAN_T2LM_DL_DIRECTION:
		for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
			config_params = nla_nest_start(skb, tid_num + 1);
			if (!config_params)
				return -EINVAL;
			attr1 = QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_DOWNLINK;
			value = t2lm->ieee_link_map_tid[i];
			errno = nla_put_u16(skb, attr1, value);
			if (errno)
				return errno;

			attr1 = QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_UPLINK;
			value = 0;
			errno = nla_put_u16(skb, attr1, value);
			if (errno)
				return errno;
			nla_nest_end(skb, config_params);
		}
		break;
	case WLAN_T2LM_BIDI_DIRECTION:
		for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
			config_params = nla_nest_start(skb, tid_num + 1);
			if (!config_params)
				return -EINVAL;

			attr1 = QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_UPLINK;
			value = t2lm->ieee_link_map_tid[i];
			errno = nla_put_u16(skb, attr1, value);
			if (errno)
				return errno;

			attr1 = QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_DOWNLINK;
			value = t2lm->ieee_link_map_tid[i];
			errno = nla_put_u16(skb, attr1, value);
			if (errno)
				return errno;
			nla_nest_end(skb, config_params);
		}
		break;
	default:
		return -EINVAL;
	}
	nla_nest_end(skb, config_attr);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_hdd_send_t2lm_event(struct wlan_objmgr_vdev *vdev,
				    struct wlan_t2lm_info *t2lm)
{
	struct sk_buff *skb;
	size_t data_len;
	QDF_STATUS status;
	struct qdf_mac_addr mld_addr;
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;

	enum qca_nl80211_vendor_subcmds_index index =
		QCA_NL80211_VENDOR_SUBCMD_TID_TO_LINK_MAP_INDEX;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("Invalid VDEV");
		return QDF_STATUS_E_FAILURE;
	}

	adapter = link_info->adapter;
	data_len = hdd_get_t2lm_setup_event_len();
	skb = wlan_cfg80211_vendor_event_alloc(adapter->hdd_ctx->wiphy,
					       NULL,
					       data_len,
					       index, GFP_KERNEL);
	if (!skb) {
		hdd_err("wlan_cfg80211_vendor_event_alloc failed");
		return -EINVAL;
	}

	/* get mld addr */
	status = wlan_vdev_get_bss_peer_mld_mac(vdev, &mld_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get mld address");
		goto free_skb;
	}

	status = hdd_t2lm_pack_nl_response(skb, vdev, t2lm, mld_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to pack nl response");
		goto free_skb;
	}

	wlan_cfg80211_vendor_event(skb, GFP_KERNEL);

	return status;
free_skb:
	wlan_cfg80211_vendor_free_skb(skb);

	return status;
}
#endif
