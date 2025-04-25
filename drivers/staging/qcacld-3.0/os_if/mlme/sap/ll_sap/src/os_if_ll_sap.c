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

/**
 * DOC: contains ll_sap definitions specific to the ll_lt_sap module
 */

#include "os_if_ll_sap.h"
#include "wlan_ll_sap_public_structs.h"
#include "wlan_ll_sap_ucfg_api.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_cfg80211.h"
#include "wlan_osif_priv.h"

#define WLAN_AUDIO_TRANSPORT_SWITCH_TYPE_INVALID 0xFFFF

/**
 * osif_convert_audio_transport_switch_req_type_to_qca_type() - Convert
 * audio transport switch request type to qca audio transport switch req type
 * @req_type: Request type
 *
 * Return:   enum qca_wlan_audio_transport_switch_type
 */
static enum qca_wlan_audio_transport_switch_type
osif_convert_audio_transport_switch_req_type_to_qca_type
					(enum bearer_switch_req_type req_type)
{
	switch (req_type) {
	case WLAN_BS_REQ_TO_NON_WLAN:
		return QCA_WLAN_AUDIO_TRANSPORT_SWITCH_TYPE_NON_WLAN;
	case WLAN_BS_REQ_TO_WLAN:
		return QCA_WLAN_AUDIO_TRANSPORT_SWITCH_TYPE_WLAN;
	default:
		osif_err("Invalid audio transport switch type");
		return WLAN_AUDIO_TRANSPORT_SWITCH_TYPE_INVALID;
	}
}

/**
 * osif_convert_audio_transport_switch_req_type_from_qca_type() - Convert
 * audio transport switch request type from qca audio transport switch req type
 * @req_type: Request type.
 *
 * Return:   enum bearer_switch_req_type
 */
static enum bearer_switch_req_type
osif_convert_audio_transport_switch_req_type_from_qca_type
			(enum qca_wlan_audio_transport_switch_type req_type)
{
	switch (req_type) {
	case  QCA_WLAN_AUDIO_TRANSPORT_SWITCH_TYPE_NON_WLAN:
		return WLAN_BS_REQ_TO_NON_WLAN;
	case QCA_WLAN_AUDIO_TRANSPORT_SWITCH_TYPE_WLAN:
		return WLAN_BS_REQ_TO_WLAN;
	default:
		osif_err("Invalid %d req type", req_type);
		return WLAN_BS_REQ_INVALID;
	}
}

/**
 * osif_convert_audio_transport_switch_status_type_from_qca_type() - Convert
 * audio transport switch status from qca audio transport switch status type
 * @status:    audio transport switch status.
 *
 * Return:   enum bearer_switch_status
 */
static enum bearer_switch_status
osif_convert_audio_transport_switch_status_type_from_qca_type
			(enum qca_wlan_audio_transport_switch_status status)
{
	switch (status) {
	case  QCA_WLAN_AUDIO_TRANSPORT_SWITCH_STATUS_REJECTED:
		return WLAN_BS_STATUS_REJECTED;
	case QCA_WLAN_AUDIO_TRANSPORT_SWITCH_STATUS_COMPLETED:
		return WLAN_BS_STATUS_COMPLETED;
	default:
		return WLAN_BS_STATUS_INVALID;
	}
}

/**
 * wlan_osif_send_audio_transport_switch_req() - Send audio transport
 * switch request
 * @vdev: pointer to vdev structure.
 * @req_type: Request type.
 *
 * Return: None.
 */
static void
wlan_osif_send_audio_transport_switch_req(struct wlan_objmgr_vdev *vdev,
					  enum bearer_switch_req_type req_type)
{
	struct sk_buff *vendor_event;
	struct wireless_dev *wdev;
	struct vdev_osif_priv *osif_priv;
	uint32_t len;
	enum qca_wlan_audio_transport_switch_type switch_type;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("Vdev %d osif_priv is null", vdev_id);
		return;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("vdev %d wireless dev is null", vdev_id);
		return;
	}

	switch_type =
		osif_convert_audio_transport_switch_req_type_to_qca_type(
								req_type);
	len = nla_total_size(sizeof(uint8_t)) + NLMSG_HDRLEN;

	vendor_event = wlan_cfg80211_vendor_event_alloc(
			wdev->wiphy, wdev, len,
			QCA_NL80211_VENDOR_SUBCMD_AUDIO_TRANSPORT_SWITCH_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		osif_err("vdev %d wlan_cfg80211_vendor_event_alloc failed",
			 vdev_id);
		return;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_TYPE,
		       switch_type)) {
		osif_err("Vdev %d VENDOR_ATTR_AUDIO_TRANSPORT_SWITCH_TYPE put fail",
			 vdev_id);
		wlan_cfg80211_vendor_free_skb(vendor_event);
		return;
	}

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	osif_nofl_debug("Vdev %d Audio Transport switch request %d sent",
			vdev_id, switch_type);
}

QDF_STATUS osif_ll_lt_sap_request_for_audio_transport_switch(
			struct wlan_objmgr_vdev *vdev,
			enum qca_wlan_audio_transport_switch_type req_type)
{
	return ucfg_ll_lt_sap_request_for_audio_transport_switch(
		vdev,
		osif_convert_audio_transport_switch_req_type_from_qca_type(
								req_type));
}

QDF_STATUS osif_ll_lt_sap_deliver_audio_transport_switch_resp(
			struct wlan_objmgr_vdev *vdev,
			enum qca_wlan_audio_transport_switch_type req_type,
			enum qca_wlan_audio_transport_switch_status status)
{
	static enum bearer_switch_status bs_status;
	enum bearer_switch_req_type bs_req_type;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);

	if (status == QCA_WLAN_AUDIO_TRANSPORT_SWITCH_STATUS_COMPLETED) {
		osif_nofl_debug("vdev %d Transport switch request %d completed",
				vdev_id, req_type);
	} else if (status == QCA_WLAN_AUDIO_TRANSPORT_SWITCH_STATUS_REJECTED) {
		osif_nofl_debug("vdev %d Transport switch request %d rejected",
				vdev_id, req_type);
	} else {
		osif_err("vdev %d Invalid transport switch status %d", vdev_id,
			 status);
		return QDF_STATUS_E_INVAL;
	}
	bs_status =
		osif_convert_audio_transport_switch_status_type_from_qca_type(
									status);
	bs_req_type =
		osif_convert_audio_transport_switch_req_type_from_qca_type(
								req_type);

	ucfg_ll_lt_sap_deliver_audio_transport_switch_resp(vdev, bs_req_type,
							   bs_status);

	return QDF_STATUS_SUCCESS;
}

static struct ll_sap_ops ll_sap_global_ops = {
	.ll_sap_send_audio_transport_switch_req_cb =
		wlan_osif_send_audio_transport_switch_req,
};

QDF_STATUS osif_ll_sap_register_cb(void)
{
	ucfg_ll_sap_register_cb(&ll_sap_global_ops);
	return QDF_STATUS_SUCCESS;
}

void osif_ll_sap_unregister_cb(void)
{
	ucfg_ll_sap_unregister_cb();
}
