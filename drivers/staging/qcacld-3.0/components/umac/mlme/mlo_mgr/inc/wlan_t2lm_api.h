/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains TID to Link mapping related functionality
 */
#ifndef _WLAN_T2LM_API_H_
#define _WLAN_T2LM_API_H_

#include "lim_types.h"
#include "lim_utils.h"
#include "lim_send_sme_rsp_messages.h"
#include "parser_api.h"
#include "lim_send_messages.h"

/**
 * struct t2lm_event_data - TID to Link mapping event data
 * @status: qdf status used to indicate if t2lm action frame status
 * @data: event data
 */
struct t2lm_event_data {
	QDF_STATUS status;
	void *data;
};

/**
 * enum wlan_t2lm_evt: T2LM manager events
 * @WLAN_T2LM_EV_ACTION_FRAME_RX_REQ:Handle T2LM request frame received from AP
 * @WLAN_T2LM_EV_ACTION_FRAME_TX_RESP:Handle T2LM response frame sent to AP
 * @WLAN_T2LM_EV_ACTION_FRAME_TX_REQ:Handle T2LM request frame sent by STA
 * @WLAN_T2LM_EV_ACTION_FRAME_RX_RESP:Handle T2LM response frame received from AP
 * @WLAN_T2LM_EV_ACTION_FRAME_RX_TEARDOWN:Handle received teardown frame event
 * @WLAN_T2LM_EV_ACTION_FRAME_TX_TEARDOWN:Handle sending teardown frame event
 * @WLAN_T2LM_EV_ACTION_FRAME_MAX: Maximum T2LM action frame event value
 */
enum wlan_t2lm_evt {
	WLAN_T2LM_EV_ACTION_FRAME_RX_REQ = 0,
	WLAN_T2LM_EV_ACTION_FRAME_TX_RESP = 1,
	WLAN_T2LM_EV_ACTION_FRAME_TX_REQ = 2,
	WLAN_T2LM_EV_ACTION_FRAME_RX_RESP = 3,
	WLAN_T2LM_EV_ACTION_FRAME_RX_TEARDOWN = 4,
	WLAN_T2LM_EV_ACTION_FRAME_TX_TEARDOWN = 5,
	WLAN_T2LM_EV_ACTION_FRAME_MAX = 6,
};

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * t2lm_deliver_event - Handler to deliver T2LM event
 * @vdev: vdev pointer
 * @peer: pointer to peer
 * @event: T2LM event
 * @event_data: T2LM event data pointer
 * @dialog_token: Dialog token
 *
 * This api will be called from lim  layers, to process T2LM event
 *
 * Return: qdf_status
 */
QDF_STATUS t2lm_deliver_event(struct wlan_objmgr_vdev *vdev,
			      struct wlan_objmgr_peer *peer,
			      enum wlan_t2lm_evt event,
			      void *event_data,
			      uint8_t *dialog_token);

/**
 * t2lm_handle_rx_req - Handler for parsing T2LM action frame
 * @vdev: vdev pointer
 * @peer: pointer to peer
 * @event_data: T2LM event data pointer
 * @token: Dialog token
 *
 * This api will be called from lim  layers, after T2LM action frame
 * is received, the api will parse the T2LM request frame.
 *
 * Return: qdf_status
 */
QDF_STATUS t2lm_handle_rx_req(struct wlan_objmgr_vdev *vdev,
			      struct wlan_objmgr_peer *peer,
			      void *event_data, uint8_t *token);

/**
 * t2lm_handle_tx_resp - Handler for populating T2LM action frame
 * @vdev: vdev pointer
 * @event_data: T2LM event data pointer
 * @token: Dialog token
 *
 * This api will be called to populate T2LM response action frame.
 *
 * Return: qdf_status
 */
QDF_STATUS t2lm_handle_tx_resp(struct wlan_objmgr_vdev *vdev,
			       void *event_data, uint8_t *token);

/**
 * t2lm_handle_tx_req - Handler for populating T2LM action frame
 * @vdev: vdev pointer
 * @event_data: T2LM event data pointer
 * @token: Dialog token
 *
 * This api will be called to populate T2LM request action frame.
 *
 * Return: qdf_status
 */
QDF_STATUS t2lm_handle_tx_req(struct wlan_objmgr_vdev *vdev,
			      void *event_data, uint8_t *token);

/**
 * t2lm_handle_rx_resp - Handler for parsing T2LM action frame
 * @vdev: vdev pointer
 * @event_data: T2LM event data pointer
 * @token: Dialog token
 *
 * This api will be called to parsing T2LM response action frame.
 *
 * Return: qdf_status
 */
QDF_STATUS t2lm_handle_rx_resp(struct wlan_objmgr_vdev *vdev,
			       void *event_data, uint8_t *token);

/**
 * t2lm_handle_rx_teardown - Handler for parsing T2LM action frame
 * @vdev: vdev pointer
 * @peer: peer pointer
 * @event_data: T2LM event data pointer
 *
 * This api will be called to parsing T2LM teardown action frame.
 *
 * Return: qdf_status
 */
QDF_STATUS t2lm_handle_rx_teardown(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   void *event_data);

/**
 * t2lm_handle_tx_teardown - Handler for populating T2LM action frame
 * @vdev: vdev pointer
 * @event_data: T2LM event data pointer
 *
 * This api will be called to populate T2LM teardown action frame.
 *
 * Return: qdf_status
 */
QDF_STATUS t2lm_handle_tx_teardown(struct wlan_objmgr_vdev *vdev,
				   void *event_data);

/**
 * wlan_t2lm_validate_candidate - Validate candidate based on T2LM IE
 * @cm_ctx: connection manager context pointer
 * @scan_entry: scan entry pointer
 *
 * This api will be called to validate candidate based on T2LM IE received
 * in beacon or probe response
 *
 * Return: qdf_status
 */

QDF_STATUS
wlan_t2lm_validate_candidate(struct cnx_mgr *cm_ctx,
			     struct scan_cache_entry *scan_entry);
/**
 * wlan_t2lm_deliver_event() - TID-to-link-mapping event handler
 * @vdev: vdev object
 * @peer: pointer to peer
 * @event: T2LM event
 * @event_data: T2LM event data
 * @dialog_token: Dialog token
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_t2lm_deliver_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   enum wlan_t2lm_evt event,
				   void *event_data,
				   uint8_t *dialog_token);

/**
 * wlan_t2lm_clear_peer_negotiation - Clear previously
 * negotiated peer level TID-to-link-mapping.
 * @peer: pointer to peer
 *
 * Return: none
 */
void
wlan_t2lm_clear_peer_negotiation(struct wlan_objmgr_peer *peer);
#else
static inline QDF_STATUS
t2lm_handle_rx_req(struct wlan_objmgr_vdev *vdev,
		   struct wlan_objmgr_peer *peer,
		   void *event_data, uint8_t *token)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
t2lm_handle_tx_resp(struct wlan_objmgr_vdev *vdev,
		    void *event_data, uint8_t *token)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
t2lm_handle_tx_req(struct wlan_objmgr_vdev *vdev,
		   void *event_data, uint8_t *token)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
t2lm_handle_rx_resp(struct wlan_objmgr_vdev *vdev,
		    void *event_data, uint8_t *token)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
t2lm_handle_rx_teardown(struct wlan_objmgr_vdev *vdev,
			struct wlan_objmgr_peer *peer,
			void *event_data)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
t2lm_handle_tx_teardown(struct wlan_objmgr_vdev *vdev,
			void *event_data)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_t2lm_validate_candidate(struct cnx_mgr *cm_ctx,
			     struct scan_cache_entry *scan_entry)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
wlan_t2lm_clear_peer_negotiation(struct wlan_objmgr_peer *peer)
{}

static inline
QDF_STATUS wlan_t2lm_deliver_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   enum wlan_t2lm_evt event,
				   void *event_data,
				   uint8_t *dialog_token)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif
