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

#include "qdf_types.h"
#include <net/cfg80211.h>
#include "wlan_cfg80211.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "os_if_dp_local_pkt_capture.h"
#include "wlan_dp_ucfg_api.h"
#include "wlan_dp_main.h"
#include "cdp_txrx_mon.h"
#include "wlan_policy_mgr_api.h"
#include <ol_defines.h>
#include "wlan_osif_priv.h"

/* Short name for QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE command */
#define SET_MONITOR_MODE_CONFIG_MAX \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MAX
#define SET_MONITOR_MODE_INVALID \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_INVALID
#define SET_MONITOR_MODE_DATA_TX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE
#define SET_MONITOR_MODE_DATA_RX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE
#define SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE
#define SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE
#define SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE
#define SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE

/* Short name for QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE command */
#define GET_MONITOR_MODE_CONFIG_MAX \
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_MAX
#define GET_MONITOR_MODE_INVALID \
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_INVALID
#define GET_MONITOR_MODE_STATUS \
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_STATUS

#define MGMT_FRAME_TYPE    0
#define DATA_FRAME_TYPE    1
#define CTRL_FRAME_TYPE    2

const struct nla_policy
set_monitor_mode_policy[SET_MONITOR_MODE_CONFIG_MAX + 1] = {
	[SET_MONITOR_MODE_DATA_TX_FRAME_TYPE] = { .type = NLA_U32 },
	[SET_MONITOR_MODE_DATA_RX_FRAME_TYPE] = { .type = NLA_U32 },
	[SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE] = { .type = NLA_U32 },
	[SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE] = { .type = NLA_U32 },
	[SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE] = { .type = NLA_U32 },
	[SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE] = { .type = NLA_U32 },
};

static
bool os_if_local_pkt_capture_concurrency_allowed(struct wlan_objmgr_psoc *psoc)
{
	uint32_t num_connections, sta_count;

	num_connections = policy_mgr_get_connection_count(psoc);
	osif_debug("Total connections %d", num_connections);

	/*
	 * No connections, local packet capture is allowed
	 * Only 1 connection and its STA, then local packet capture is allowed
	 * 2+ port concurrency, local packet capture is not allowed
	 */
	if (!num_connections)
		return true;

	if (num_connections > 1)
		return false;

	sta_count = policy_mgr_mode_specific_connection_count(psoc,
							      PM_STA_MODE,
							      NULL);
	osif_debug("sta_count %d", sta_count);
	if (sta_count == 1)
		return true;

	return false;
}

bool os_if_lpc_mon_intf_creation_allowed(struct wlan_objmgr_psoc *psoc)
{
	if (ucfg_dp_is_local_pkt_capture_enabled(psoc)) {
		if (policy_mgr_is_mlo_sta_present(psoc)) {
			osif_err("MLO STA present, lpc interface creation not allowed");
			return false;
		}

		if (!os_if_local_pkt_capture_concurrency_allowed(psoc)) {
			osif_err("Concurrency check failed, lpc interface creation not allowed");
			return false;
		}
	}

	return true;
}

static QDF_STATUS os_if_start_capture_allowed(struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE mode = wlan_vdev_mlme_get_opmode(vdev);
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("NULL psoc");
		return QDF_STATUS_E_INVAL;
	}

	if (!ucfg_dp_is_local_pkt_capture_enabled(psoc)) {
		osif_warn("local pkt capture feature not enabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (mode != QDF_MONITOR_MODE) {
		osif_err("Operation not permitted in mode: %d", mode);
		return QDF_STATUS_E_PERM;
	}

	/*
	 * Whether STA interface is present or not, is already checked
	 * while creating monitor interface
	 */

	if (policy_mgr_is_mlo_sta_present(psoc)) {
		osif_err("MLO STA present, start capture is not permitted");
		return QDF_STATUS_E_PERM;
	}

	if (!os_if_local_pkt_capture_concurrency_allowed(psoc)) {
		osif_err("Concurrency check failed, start capture not allowed");
		return QDF_STATUS_E_PERM;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS os_if_stop_capture_allowed(struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE mode;
	struct wlan_objmgr_psoc *psoc;
	void *soc;

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc)
		return QDF_STATUS_E_INVAL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("NULL psoc");
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_MONITOR_MODE) {
		osif_warn("Operation not permitted in mode: %d", mode);
		return QDF_STATUS_E_PERM;
	}

	if (!ucfg_dp_is_local_pkt_capture_enabled(psoc)) {
		osif_err("local pkt capture feature not enabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (!cdp_is_local_pkt_capture_running(soc, OL_TXRX_PDEV_ID)) {
		osif_debug("local pkt capture not running, no need to stop");
		return QDF_STATUS_E_PERM;
	}

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS os_if_dp_local_pkt_capture_start(struct wlan_objmgr_vdev *vdev,
					    struct nlattr **tb)
{
	QDF_STATUS status;
	struct cdp_monitor_filter filter = {0};
	uint32_t pkt_type = 0, val;
	void *soc;

	status = os_if_start_capture_allowed(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc)
		return QDF_STATUS_E_INVAL;

	if (tb[SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE]) {
		val = nla_get_u32(tb[SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE]);

		if (val != QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL) {
			osif_err("Invalid value: %d Expected: %d",
				val,
				QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL);
			status = QDF_STATUS_E_INVAL;
			goto error;
		}
		pkt_type |= BIT(MGMT_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE]) {
		val = nla_get_u32(tb[SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE]);

		if (val != QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL) {
			osif_err("Invalid value: %d Expected: %d",
				val,
				QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL);
			status = QDF_STATUS_E_INVAL;
			goto error;
		}
		pkt_type |= BIT(MGMT_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_DATA_TX_FRAME_TYPE]) {
		val = nla_get_u32(tb[SET_MONITOR_MODE_DATA_TX_FRAME_TYPE]);

		if (val != QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL) {
			osif_err("Invalid value: %d Expected: %d",
				val,
				QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL);
			status = QDF_STATUS_E_INVAL;
			goto error;
		}
		pkt_type |= BIT(DATA_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_DATA_RX_FRAME_TYPE]) {
		val = nla_get_u32(tb[SET_MONITOR_MODE_DATA_RX_FRAME_TYPE]);

		if (val != QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL) {
			osif_err("Invalid value: %d Expected: %d",
				val,
				QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL);
			status = QDF_STATUS_E_INVAL;
			goto error;
		}
		pkt_type |= BIT(DATA_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE]) {
		val = nla_get_u32(tb[SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE]);

		if (val != QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL) {
			osif_err("Invalid value: %d Expected: %d",
				val,
				QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL);
			status = QDF_STATUS_E_INVAL;
			goto error;
		}
		pkt_type |= BIT(CTRL_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE]) {
		val = nla_get_u32(tb[SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE]);

		if (val != QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL) {
			osif_err("Invalid value: %d Expected: %d",
				val,
				QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL);
			status = QDF_STATUS_E_INVAL;
			goto error;
		}
		pkt_type |= BIT(CTRL_FRAME_TYPE);
	}

	if (pkt_type == 0) {
		osif_err("Invalid config, pkt_type: %d", pkt_type);
		status = QDF_STATUS_E_INVAL;
		goto error;
	}
	osif_debug("start capture config pkt_type:0x%x", pkt_type);

	filter.mode = MON_FILTER_PASS;
	filter.fp_mgmt = pkt_type & BIT(MGMT_FRAME_TYPE) ? FILTER_MGMT_ALL : 0;
	filter.fp_data = pkt_type & BIT(DATA_FRAME_TYPE) ? FILTER_DATA_ALL : 0;
	filter.fp_ctrl = pkt_type & BIT(CTRL_FRAME_TYPE) ? FILTER_CTRL_ALL : 0;

	status = cdp_start_local_pkt_capture(soc, OL_TXRX_PDEV_ID, &filter);

error:
	return status;
}

QDF_STATUS os_if_dp_set_lpc_configure(struct wlan_objmgr_vdev *vdev,
				      const void *data, int data_len)
{
	struct nlattr *tb[SET_MONITOR_MODE_CONFIG_MAX + 1];
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (wlan_cfg80211_nla_parse(tb, SET_MONITOR_MODE_CONFIG_MAX,
				    data, data_len, set_monitor_mode_policy)) {
		osif_err("Invalid monitor attr");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	status = os_if_dp_local_pkt_capture_start(vdev, tb);

error:
	return status;
}

QDF_STATUS os_if_dp_local_pkt_capture_stop(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	void *soc;

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc || !vdev)
		return QDF_STATUS_E_INVAL;

	status = os_if_stop_capture_allowed(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return cdp_stop_local_pkt_capture(soc, OL_TXRX_PDEV_ID);
}

QDF_STATUS os_if_dp_get_lpc_state(struct wlan_objmgr_vdev *vdev,
				  const void *data, int data_len)
{
	struct wlan_objmgr_psoc *psoc;
	struct vdev_osif_priv *osif_priv;
	struct sk_buff *reply_skb;
	uint32_t skb_len = NLMSG_HDRLEN, val;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wireless_dev *wdev;
	bool running;
	void *soc;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return QDF_STATUS_E_INVAL;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		return QDF_STATUS_E_INVAL;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		return QDF_STATUS_E_INVAL;
	}

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc)
		return QDF_STATUS_E_INVAL;

	/* Length of attribute QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_STATUS */
	skb_len += nla_total_size(sizeof(u32));

	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wdev->wiphy,
							     skb_len);
	if (!reply_skb) {
		osif_err("alloc reply skb failed");
		return QDF_STATUS_E_NOMEM;
	}

	running = cdp_is_local_pkt_capture_running(soc, OL_TXRX_PDEV_ID);
	val = running ? QCA_WLAN_VENDOR_MONITOR_MODE_CAPTURE_RUNNING :
			QCA_WLAN_VENDOR_MONITOR_MODE_NO_CAPTURE_RUNNING;

	if (nla_put_u32(reply_skb, GET_MONITOR_MODE_STATUS, val)) {
		osif_err("nla put failed");
		status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	if (wlan_cfg80211_vendor_cmd_reply(reply_skb))
		status = QDF_STATUS_E_INVAL;

	return status;
fail:
	wlan_cfg80211_vendor_free_skb(reply_skb);
	return status;
}

