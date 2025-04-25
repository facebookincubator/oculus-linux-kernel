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

/*
 * DOC: contains EPCS (Emergency Preparedness Communications Service)
 * related functionality
 */
#ifndef _WLAN_EPCS_API_H_
#define _WLAN_EPCS_API_H_

#include "lim_types.h"
#include "lim_utils.h"
#include "lim_send_sme_rsp_messages.h"
#include "parser_api.h"
#include "lim_send_messages.h"

#define RSVD_SHIFT_BIT 0x7
#define ACI_SHIFT_BIT 0x5
#define ACM_SHIFT_BIT 0x4
#define AIFSN_SHIFT_BIT 0x0
#define CWMAX_SHIFT_BIT 0x4
#define CWMIN_SHIFT_BIT 0x0

#define RSVD_MASK 0x1
#define ACI_MASK 0x3
#define ACM_MASK 0x1
#define AIFSN_MASK 0xf
#define CWMAX_MASK 0xf
#define CWMIN_MASK 0xf

#define WMM_VENDOR_HEADER_LEN 7

/**
 * enum wlan_epcs_evt: EPCS manager events
 * @WLAN_EPCS_EV_ACTION_FRAME_RX_REQ:Handle EPCS request frame received from AP
 * @WLAN_EPCS_EV_ACTION_FRAME_TX_RESP:Handle EPCS response frame sent to AP
 * @WLAN_EPCS_EV_ACTION_FRAME_TX_REQ:Handle EPCS request frame sent by STA
 * @WLAN_EPCS_EV_ACTION_FRAME_RX_RESP:Handle EPCS response frame received from AP
 * @WLAN_EPCS_EV_ACTION_FRAME_RX_TEARDOWN:Handle received teardown frame event
 * @WLAN_EPCS_EV_ACTION_FRAME_TX_TEARDOWN:Handle sending teardown frame event
 * @WLAN_EPCS_EV_ACTION_FRAME_MAX: Maximum EPCS action frame event value
 */
enum wlan_epcs_evt {
	WLAN_EPCS_EV_ACTION_FRAME_RX_REQ = 0,
	WLAN_EPCS_EV_ACTION_FRAME_TX_RESP = 1,
	WLAN_EPCS_EV_ACTION_FRAME_TX_REQ = 2,
	WLAN_EPCS_EV_ACTION_FRAME_RX_RESP = 3,
	WLAN_EPCS_EV_ACTION_FRAME_RX_TEARDOWN = 4,
	WLAN_EPCS_EV_ACTION_FRAME_TX_TEARDOWN = 5,
	WLAN_EPCS_EV_ACTION_FRAME_MAX = 6,
};

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_epcs_deliver_event() - Handler to deliver EPCS event
 * @vdev: vdev pointer
 * @peer: pointer to peer
 * @event: EPCS event
 * @event_data: EPCS event data pointer
 * @len: length of data
 *
 * This api will be called from lim  layers, to process EPCS event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_epcs_deliver_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   enum wlan_epcs_evt event,
				   void *event_data, uint32_t len);

/**
 * wlan_epcs_set_config() - Set EPCS enable flag
 * @vdev: vdev pointer
 * @flag: EPCS flag
 *
 * This api will be called from os_if layers, to set EPCS flag
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_epcs_set_config(struct wlan_objmgr_vdev *vdev,
				uint8_t flag);

/**
 * wlan_epcs_get_config() - Get EPCS config
 * @vdev: vdev pointer
 *
 * This api will be called from other module
 *
 * Return: bool
 */
bool wlan_epcs_get_config(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_epcs_deliver_cmd() - Handler to deliver EPCS command
 * @vdev: vdev pointer
 * @event: EPCS event
 *
 * This api will be called from os_if layers, to process EPCS command
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_epcs_deliver_cmd(struct wlan_objmgr_vdev *vdev,
				 enum wlan_epcs_evt event);
#else
static inline
QDF_STATUS wlan_epcs_deliver_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   enum wlan_epcs_evt event,
				   void *event_data, uint32_t len)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_epcs_set_config(struct wlan_objmgr_vdev *vdev, uint8_t flag)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool wlan_epcs_get_config(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
QDF_STATUS wlan_epcs_deliver_cmd(struct wlan_objmgr_vdev *vdev,
				 enum wlan_epcs_evt event)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif
