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
 * DOC: wlan_hdd_mlo.h
 *
 * WLAN Host Device Driver file for 802.11be (Extremely High Throughput)
 * support.
 *
 */
#if !defined(WLAN_HDD_MLO_H)
#define WLAN_HDD_MLO_H
#include <wlan_hdd_main.h>
#include "wlan_osif_features.h"

/**
 * struct hdd_adapter_create_param - adapter create parameters
 * @only_wdev_register:  Register only the wdev not the netdev
 * @associate_with_ml_adapter: Vdev points to the same netdev adapter
 * @is_ml_adapter: is a ml adapter with associated netdev
 * @is_add_virtual_iface: is netdev create request from add virtual interface
 * @is_single_link: Is the adapter single link ML
 * @unused: Reserved spare bits
 */
struct hdd_adapter_create_param {
	uint32_t only_wdev_register:1,
		 associate_with_ml_adapter:1,
		 is_ml_adapter:1,
		 is_add_virtual_iface:1,
		 is_single_link:1,
		 unused:27;
};

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
#define hdd_adapter_is_link_adapter(x) ((x)->mlo_adapter_info.is_link_adapter)
#define hdd_adapter_is_ml_adapter(x)   ((x)->mlo_adapter_info.is_ml_adapter)
#define hdd_adapter_is_sl_ml_adapter(x) \
			   ((x)->mlo_adapter_info.is_single_link_ml)
#define hdd_adapter_is_associated_with_ml_adapter(x) \
			   ((x)->mlo_adapter_info.associate_with_ml_adapter)
#define hdd_adapter_get_mlo_adapter_from_link(x) \
			   ((x)->mlo_adapter_info.ml_adapter)
/* MLO_STATE_COMMANDS */
#define FEATURE_ML_LINK_STATE_COMMANDS					\
	{								\
		.info.vendor_id = QCA_NL80211_VENDOR_ID,		\
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE,\
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |			\
			 WIPHY_VENDOR_CMD_NEED_NETDEV |			\
			 WIPHY_VENDOR_CMD_NEED_RUNNING,			\
		.doit = wlan_hdd_cfg80211_process_ml_link_state,	\
		vendor_command_policy(ml_link_state_request_policy,	\
				QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX)	\
	},
#else
#define hdd_adapter_is_link_adapter(x) (0)
#define hdd_adapter_is_ml_adapter(x)   (0)
#define hdd_adapter_is_sl_ml_adapter(x) (0)
#define hdd_adapter_is_associated_with_ml_adapter(x) (0)
#define hdd_adapter_get_mlo_adapter_from_link(x) (NULL)
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)

/**
 * struct hdd_mlo_adapter_info - Mlo specific adapter information
 * @is_ml_adapter: Whether this is the main ml adaper attached to netdev
 * @is_link_adapter: Whether this a link adapter without netdev
 * @associate_with_ml_adapter: adapter which shares the vdev object with the ml
 * adapter
 * @num_of_vdev_links: Num of vdevs/links part of the association
 * @is_single_link_ml: Is the adapter a single link ML adapter
 * @unused: Reserved spare bits
 * @ml_adapter: ML adapter backpointer
 * @link_adapter: backpointers to link adapters part of association
 */
struct hdd_mlo_adapter_info {
	uint32_t is_ml_adapter:1,
		 is_link_adapter:1,
		 associate_with_ml_adapter:1,
		 num_of_vdev_links:2,
		 is_single_link_ml:1,
		 unused:26;
	struct hdd_adapter *ml_adapter;
	struct hdd_adapter *link_adapter[WLAN_MAX_MLD];
};

/**
 * hdd_register_wdev() - Function to register only wdev
 * @sta_adapter : Station adapter linked with netdevice
 * @link_adapter: Link adapter
 * @adapter_params: Adapter params
 *
 * Function to register only the wdev not the netdev
 * Return: none
 */
void hdd_register_wdev(struct hdd_adapter *sta_adapter,
		       struct hdd_adapter *link_adapter,
		       struct hdd_adapter_create_param *adapter_params);
/**
 * hdd_wlan_unregister_mlo_interfaces() - Function to unregister mlo
 * interfaces
 * @adapter: Link adapter
 * @rtnl_held: RTNL held or not
 *
 * Function to unregister only the link adapter/wdev.
 * Return: none
 */
QDF_STATUS hdd_wlan_unregister_mlo_interfaces(struct hdd_adapter *adapter,
					      bool rtnl_held);

/**
 * hdd_wlan_register_mlo_interfaces() - Function to register mlo wdev interfaces
 * @hdd_ctx: hdd context
 *
 * Function to register mlo wdev interfaces.
 * Return: none
 */
void hdd_wlan_register_mlo_interfaces(struct hdd_context *hdd_ctx);

/**
 * hdd_adapter_set_ml_adapter() - set adapter as ml adapter
 * @adapter: HDD adapter
 *
 * This function sets adapter as ml adapter
 * Return: None
 */
void hdd_adapter_set_ml_adapter(struct hdd_adapter *adapter);

/**
 * hdd_adapter_set_sl_ml_adapter() - Set adapter as sl ml adapter
 * @adapter: HDD adapter
 *
 * This function sets adapter as single link ML adapter
 * Return: None
 */
void hdd_adapter_set_sl_ml_adapter(struct hdd_adapter *adapter);

/**
 * hdd_get_ml_adapter() - get an ml adapter
 * @hdd_ctx: HDD context
 *
 * This function returns ml adapter from adapter list
 * Return: adapter or NULL
 */
struct hdd_adapter *hdd_get_ml_adapter(struct hdd_context *hdd_ctx);

/**
 * hdd_get_assoc_link_adapter() - get assoc link adapter
 * @ml_adapter: ML adapter
 *
 * This function returns assoc link adapter.
 * For single link ML adapter, function returns
 * same adapter pointer.
 *
 * Return: adapter or NULL
 */
struct hdd_adapter *hdd_get_assoc_link_adapter(struct hdd_adapter *ml_adapter);

/**
 * hdd_mlo_t2lm_register_callback() - Register T2LM callback
 * @vdev: Pointer to vdev
 *
 * Return: None
 */
void hdd_mlo_t2lm_register_callback(struct wlan_objmgr_vdev *vdev);

/**
 * hdd_mlo_t2lm_unregister_callback() - Unregister T2LM callback
 * @vdev: Pointer to vdev
 *
 * Return: None
 */
void hdd_mlo_t2lm_unregister_callback(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_handle_mlo_link_state_operation() - mlo link state operation
 * @wiphy: wiphy pointer
 * @vdev: vdev handler
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * Based on the data get or set the mlo link state
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_handle_mlo_link_state_operation(struct wiphy *wiphy,
				     struct wlan_objmgr_vdev *vdev,
				     const void *data, int data_len);

extern const struct nla_policy
ml_link_state_request_policy[QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX + 1];

/**
 * wlan_hdd_cfg80211_process_ml_link_state() - process ml link state
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * Set (or) get the ml link state.
 *
 * Return:  0 on success; error number otherwise.
 */
int wlan_hdd_cfg80211_process_ml_link_state(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len);
#else
static inline
QDF_STATUS hdd_wlan_unregister_mlo_interfaces(struct hdd_adapter *adapter,
					      bool rtnl_held)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void hdd_register_wdev(struct hdd_adapter *sta_adapter,
		       struct hdd_adapter *link_adapter,
		       struct hdd_adapter_create_param *adapter_params)
{
}

static inline
void hdd_wlan_register_mlo_interfaces(struct hdd_context *hdd_ctx)
{
}

static inline void
hdd_adapter_set_ml_adapter(struct hdd_adapter *adapter)
{
}

static inline void
hdd_adapter_set_sl_ml_adapter(struct hdd_adapter *adapter)
{
}

static inline
struct hdd_adapter *hdd_get_ml_adapter(struct hdd_context *hdd_ctx)
{
	return NULL;
}

static inline
struct hdd_adapter *hdd_get_assoc_link_adapter(struct hdd_adapter *ml_adapter)
{
	return NULL;
}

static inline
void hdd_mlo_t2lm_register_callback(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void hdd_mlo_t2lm_unregister_callback(struct wlan_objmgr_vdev *vdev)
{
}

static inline QDF_STATUS
wlan_handle_mlo_link_state_operation(struct wiphy *wiphy,
				     struct wlan_objmgr_vdev *vdev,
				     const void *data, int data_len)
{
	return QDF_STATUS_SUCCESS;
}

static inline
int wlan_hdd_cfg80211_process_ml_link_state(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	return -ENOTSUPP;
}

#define FEATURE_ML_LINK_STATE_COMMANDS
#endif
#endif
