/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _OS_IF_DP_LOCAL_PKT_CAPTURE_H_
#define _OS_IF_DP_LOCAL_PKT_CAPTURE_H_

#include "qdf_types.h"
#include "qca_vendor.h"

#ifdef WLAN_FEATURE_LOCAL_PKT_CAPTURE

#define FEATURE_MONITOR_MODE_VENDOR_COMMANDS				   \
	{								   \
		.info.vendor_id = QCA_NL80211_VENDOR_ID,		   \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE, \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |			   \
			WIPHY_VENDOR_CMD_NEED_NETDEV |			   \
			WIPHY_VENDOR_CMD_NEED_RUNNING,			   \
		.doit = wlan_hdd_cfg80211_set_monitor_mode,		   \
		vendor_command_policy(set_monitor_mode_policy,		   \
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MAX) \
	},								   \
	{								   \
		.info.vendor_id = QCA_NL80211_VENDOR_ID,		   \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE, \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |			   \
			WIPHY_VENDOR_CMD_NEED_NETDEV |			   \
			WIPHY_VENDOR_CMD_NEED_RUNNING,			   \
		.doit = wlan_hdd_cfg80211_get_monitor_mode,		   \
		vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)		   \
	},

extern const struct nla_policy
set_monitor_mode_policy[QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MAX + 1];

/**
 * os_if_dp_set_lpc_configure() - Configure local packet capture
 * operation in the received vendor command
 * @vdev: vdev
 * @data: NL data
 * @data_len: NL data length
 *
 * Return: QDF_STATUS_SUCCESS if Success;
 *         QDF_STATUS_E_* if Failure
 */
QDF_STATUS os_if_dp_set_lpc_configure(struct wlan_objmgr_vdev *vdev,
				      const void *data, int data_len);

/**
 * os_if_dp_local_pkt_capture_stop() - Stop local packet capture
 * @vdev: vdev
 *
 * Return: 0 for Success and negative value for failure
 */
QDF_STATUS os_if_dp_local_pkt_capture_stop(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_dp_get_lpc_state() - get local packet capture state
 * in the received vendor command
 * @vdev: vdev
 * @data: NL data
 * @data_len: NL data length
 *
 * Return: QDF_STATUS_SUCCESS if Success;
 *         QDF_STATUS_E_* if Failure
 */
QDF_STATUS os_if_dp_get_lpc_state(struct wlan_objmgr_vdev *vdev,
				  const void *data, int data_len);

/**
 * os_if_lpc_mon_intf_creation_allowed() - Check if local packet capture
 * monitor interface creation is allowed or not
 * @psoc: psoc object handle
 *
 * Return: true, If monitor interface creation is allowed
 *         false, Otherwise
 */
bool os_if_lpc_mon_intf_creation_allowed(struct wlan_objmgr_psoc *psoc);
#else
static inline
QDF_STATUS os_if_dp_set_lpc_configure(struct wlan_objmgr_vdev *vdev,
				      const void *data, int data_len)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS os_if_dp_local_pkt_capture_stop(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS os_if_dp_get_lpc_state(struct wlan_objmgr_vdev *vdev,
				  const void *data, int data_len)
{
	return QDF_STATUS_SUCCESS;
}

static inline
bool os_if_lpc_mon_intf_creation_allowed(struct wlan_objmgr_psoc *psoc)
{
	return true;
}

#endif /* WLAN_FEATURE_LOCAL_PKT_CAPTURE */
#endif
