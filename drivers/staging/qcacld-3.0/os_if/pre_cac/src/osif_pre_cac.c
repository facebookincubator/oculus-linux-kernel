/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <qdf_types.h>
#include "osif_pre_cac.h"
#include "wlan_pre_cac_public_struct.h"
#include "wlan_pre_cac_ucfg_api.h"
#include "wlan_cfg80211.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_osif_priv.h"
#include "osif_vdev_sync.h"

static struct osif_pre_cac_legacy_ops *osif_pre_cac_legacy_ops;

static void
osif_pre_cac_complete_legacy_cb(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				QDF_STATUS status)
{
	osif_pre_cac_complete_status_legacy_cb cb = NULL;

	if (osif_pre_cac_legacy_ops)
		cb = osif_pre_cac_legacy_ops->pre_cac_complete_legacy_cb;

	if (cb)
		cb(psoc, vdev_id, status);
}

static void osif_pre_cac_complete_cb(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     QDF_STATUS status)
{
	struct vdev_osif_priv *osif_priv;
	struct osif_vdev_sync *vdev_sync;
	int errno;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				psoc, vdev_id,
				WLAN_PRE_CAC_ID);
	if (!vdev) {
		osif_err("Invalid vdev for %d", vdev_id);
		return;
	}
	osif_priv = wlan_vdev_get_ospriv(vdev);
	errno = osif_vdev_sync_trans_start_wait(osif_priv->wdev->netdev,
						&vdev_sync);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PRE_CAC_ID);
	if (errno)
		return;

	osif_pre_cac_complete_legacy_cb(psoc, vdev_id, status);

	osif_vdev_sync_trans_stop(vdev_sync);
}

static void
osif_pre_cac_conditional_csa_ind_legacy_cb(struct wlan_objmgr_vdev *vdev,
					   bool completed)
{
	osif_conditional_csa_ind_legacy_cb cb = NULL;

	if (osif_pre_cac_legacy_ops)
		cb = osif_pre_cac_legacy_ops->conditional_csa_ind_legacy_cb;

	if (cb)
		cb(vdev, completed);
}

static void
osif_pre_cac_send_conditional_freq_switch_status(struct wlan_objmgr_psoc *psoc,
						 uint8_t vdev_id,
						 bool status)
{
	struct vdev_osif_priv *osif_priv;
	struct wlan_objmgr_vdev *vdev;
	struct wireless_dev *wdev;
	struct sk_buff *event;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_PRE_CAC_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		goto fail;
	}

	event = wlan_cfg80211_vendor_event_alloc(wdev->wiphy,
		  wdev, sizeof(uint32_t) + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_SAP_CONDITIONAL_CHAN_SWITCH_INDEX,
		  GFP_KERNEL);
	if (!event) {
		osif_err("wlan_cfg80211_vendor_event_alloc failed");
		goto fail;
	}

	if (nla_put_u32(event,
			QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_STATUS,
			status)) {
		osif_err("nla put failed");
		wlan_cfg80211_vendor_free_skb(event);
		goto fail;
	}

	wlan_cfg80211_vendor_event(event, GFP_KERNEL);
	osif_pre_cac_conditional_csa_ind_legacy_cb(vdev, status);

fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PRE_CAC_ID);
}

void osif_pre_cac_set_legacy_cb(struct osif_pre_cac_legacy_ops *osif_legacy_ops)
{
	osif_pre_cac_legacy_ops = osif_legacy_ops;
}

void osif_pre_cac_reset_legacy_cb(void)
{
	osif_pre_cac_legacy_ops = NULL;
}

static struct pre_cac_ops pre_cac_ops = {
	.pre_cac_conditional_csa_ind_cb =
		osif_pre_cac_send_conditional_freq_switch_status,
	.pre_cac_complete_cb = osif_pre_cac_complete_cb,
};

QDF_STATUS osif_pre_cac_register_cb(void)
{
	ucfg_pre_cac_set_osif_cb(&pre_cac_ops);

	return QDF_STATUS_SUCCESS;
}
