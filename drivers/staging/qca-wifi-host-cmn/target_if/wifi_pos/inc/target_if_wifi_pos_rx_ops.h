/*
 * Copyright (c) 2017, 2019-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: target_if_wifi_pos_rx_ops.h
 * This file declares the functions pertinent to wifi positioning component's
 * target if layer.
 */
#ifndef _WIFI_POS_TGT_IF_RX_OPS_H_
#define _WIFI_POS_TGT_IF_RX_OPS_H_

#include "qdf_types.h"
#include "qdf_status.h"
#include "target_if_wifi_pos.h"
#include "wlan_cmn.h"

/**
 * target_if_wifi_pos_oem_rsp_ev_handler: handler registered with
 * WMI_OEM_RESPONSE_EVENTID
 * @scn: scn handle
 * @data_buf: event buffer
 * @data_len: event buffer length
 *
 * Return: status of operation
 */
int target_if_wifi_pos_oem_rsp_ev_handler(ol_scn_t scn,
					  uint8_t *data_buf,
					  uint32_t data_len);

/**
 * wifi_pos_oem_cap_ev_handler: handler registered with wmi_oem_cap_event_id
 * @scn: scn handle
 * @buf: event buffer
 * @len: event buffer length
 *
 * Return: status of operation
 */
int wifi_pos_oem_cap_ev_handler(ol_scn_t scn, uint8_t *buf, uint32_t len);

/**
 * wifi_pos_oem_meas_rpt_ev_handler: handler registered with
 * wmi_oem_meas_report_event_id
 * @scn: scn handle
 * @buf: event buffer
 * @len: event buffer length
 *
 * Return: status of operation
 */
int wifi_pos_oem_meas_rpt_ev_handler(ol_scn_t scn, uint8_t *buf,
				     uint32_t len);

/**
 * wifi_pos_oem_err_rpt_ev_handler: handler registered with
 * wmi_oem_err_report_event_id
 * @scn: scn handle
 * @buf: event buffer
 * @len: event buffer length
 *
 * Return: status of operation
 */
int wifi_pos_oem_err_rpt_ev_handler(ol_scn_t scn, uint8_t *buf,
				    uint32_t len);

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
/**
 * target_if_wifi_pos_pasn_peer_create_ev_handler() - Handle
 * wmi_rtt_pasn_peer_create_req_eventid event from firmware
 * @scn: scn handle
 * @buf: event buffer
 * @len: event buffer length
 *
 * Return: zero if success, non-zero status on failure
 */
int target_if_wifi_pos_pasn_peer_create_ev_handler(ol_scn_t scn,
						   uint8_t *buf,
						   uint32_t len);

/**
 * target_if_wifi_pos_pasn_peer_delete_ev_handler() - Handle
 * wmi_rtt_pasn_peer_delete_eventid event from firmware
 * @scn: scn handle
 * @buf: event buffer
 * @len: event buffer length
 *
 * Return: zero if success, non-zero status on failure
 */
int target_if_wifi_pos_pasn_peer_delete_ev_handler(ol_scn_t scn,
						   uint8_t *buf,
						   uint32_t len);

#else
static inline
int target_if_wifi_pos_pasn_peer_create_ev_handler(ol_scn_t scn,
						   uint8_t *buf,
						   uint32_t len)
{
	return 0;
}

static inline
int target_if_wifi_pos_pasn_peer_delete_ev_handler(ol_scn_t scn,
						   uint8_t *buf,
						   uint32_t len)
{
	return 0;
}
#endif /* WIFI_POS_CONVERGED && WLAN_FEATURE_RTT_11AZ_SUPPORT */
#endif /* _WIFI_POS_TGT_IF_RX_OPS_H_ */
