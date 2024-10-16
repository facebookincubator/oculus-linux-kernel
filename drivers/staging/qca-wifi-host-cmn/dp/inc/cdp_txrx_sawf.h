/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _CDP_TXRX_SAWF_H_
#define _CDP_TXRX_SAWF_H_

#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>

static inline QDF_STATUS
cdp_sawf_peer_svcid_map(ol_txrx_soc_handle soc,
			uint8_t *mac, uint8_t svc_id)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_def_queues_map_req) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->sawf_ops->sawf_def_queues_map_req(soc, mac, svc_id);
}

static inline QDF_STATUS
cdp_sawf_peer_unmap(ol_txrx_soc_handle soc,
		    uint8_t *mac, uint8_t svc_id)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_def_queues_unmap_req) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->sawf_ops->sawf_def_queues_unmap_req(soc, mac, svc_id);
}

static inline QDF_STATUS
cdp_sawf_peer_get_map_conf(ol_txrx_soc_handle soc,
			   uint8_t *mac)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_def_queues_get_map_report) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->sawf_ops->sawf_def_queues_get_map_report(soc, mac);
}

#ifdef CONFIG_SAWF
/**
 * cdp_get_peer_sawf_delay_stats() - Call to get SAWF delay stats
 * @soc: soc handle
 * @svc_class_id: service class ID
 * @mac: peer mac address
 * @data: opaque pointer
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_get_peer_sawf_delay_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			      uint8_t *mac, void *data)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_get_peer_sawf_delay_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_get_peer_sawf_delay_stats(soc, svc_id,
								  mac, data);
}

/**
 * cdp_get_peer_sawf_tx_stats() - Call to get SAWF Tx stats
 * @soc: soc handle
 * @svc_class_id: service class ID
 * @mac: peer mac address
 * @data: opaque pointer
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_get_peer_sawf_tx_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			   uint8_t *mac, void *data)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_get_peer_sawf_tx_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_get_peer_sawf_tx_stats(soc, svc_id,
							       mac, data);
}

/**
 * cdp_sawf_mpdu_stats_req() - Call to subscribe to MPDU stats TLV
 * @soc: soc handle
 * @enable: 1: enable 0: disable
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_sawf_mpdu_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_mpdu_stats_req)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->sawf_mpdu_stats_req(soc, enable);
}

/**
 * cdp_sawf_mpdu_details_stats_req - Call to subscribe to MPDU details stats TLV
 * @soc: soc handle
 * @enable: 1: enable 0: disable
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_sawf_mpdu_details_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_mpdu_details_stats_req)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->sawf_mpdu_details_stats_req(soc, enable);
}

/**
 * cdp_sawf_set_mov_avg_params - Set moving average pararms
 * @num_pkt: No of packets per window to calucalte moving average
 * @num_win: No of windows to calucalte moving average
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_sawf_set_mov_avg_params(ol_txrx_soc_handle soc,
			    uint32_t num_pkt,
			    uint32_t num_win)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_sawf_set_mov_avg_params)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_sawf_set_mov_avg_params(num_pkt,
								num_win);
}

/**
 * cdp_sawf_set_sla_params - Set SLA pararms
 * @num_pkt: No of packets to detect SLA breach
 * @time_secs: Time ins secs to detect breach
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_sawf_set_sla_params(ol_txrx_soc_handle soc,
			uint32_t num_pkt,
			uint32_t time_secs)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_sawf_set_sla_params)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_sawf_set_sla_params(num_pkt,
							    time_secs);
}

/**
 * cdp_sawf_init_telemetry_param - Initialize telemetry pararms
 *
 * Return: none
 */
static inline QDF_STATUS
cdp_sawf_init_telemtery_params(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_sawf_init_telemtery_params)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_sawf_init_telemtery_params();
}

static inline QDF_STATUS
cdp_get_throughput_stats(ol_txrx_soc_handle soc, void *arg,
			 uint64_t *in_bytes, uint64_t *in_cnt,
			 uint64_t *tx_bytes, uint64_t *tx_cnt,
			 uint8_t tid, uint8_t msduq)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->telemetry_get_throughput_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->telemetry_get_throughput_stats(
			arg, in_bytes, in_cnt, tx_bytes,
			tx_cnt, tid, msduq);
}

static inline QDF_STATUS
cdp_get_mpdu_stats(ol_txrx_soc_handle soc, void *arg,
		   uint64_t *svc_int_pass, uint64_t *svc_int_fail,
		   uint64_t *burst_pass, uint64_t *burst_fail,
		   uint8_t tid, uint8_t msduq)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->telemetry_get_mpdu_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->telemetry_get_mpdu_stats(
			arg, svc_int_pass, svc_int_fail, burst_pass,
			burst_fail, tid, msduq);
}

static inline QDF_STATUS
cdp_get_drop_stats(ol_txrx_soc_handle soc, void *arg,
		   uint64_t *pass, uint64_t *drop,
		   uint64_t *drop_ttl,
		   uint8_t tid, uint8_t msduq)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->telemetry_get_drop_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->telemetry_get_drop_stats(
			arg, pass, drop, drop_ttl, tid, msduq);
}

/**
 * cdp_sawf_peer_config_ul - Config uplink QoS parameters
 * @soc: SOC handle
 * @mac_addr: MAC address
 * @tid: TID
 * @service_interval: Service Interval
 * @burst_size: Burst Size
 * @add_or_sub: Add or Sub parameters
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_sawf_peer_config_ul(ol_txrx_soc_handle soc, uint8_t *mac_addr, uint8_t tid,
			uint32_t service_interval, uint32_t burst_size,
			uint8_t add_or_sub)
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->peer_config_ul) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return false;
	}

	return soc->ops->sawf_ops->peer_config_ul(soc, mac_addr, tid,
						  service_interval, burst_size,
						  add_or_sub);
}

/*
 * cdp_swaf_peer_is_sla_configured() - Check if sla is configured for a peer
 * @soc_hdl: SOC handle
 * @mac_addr: peer mac address
 *
 * Return: true is peer is sla configured
 */
static inline bool
cdp_swaf_peer_is_sla_configured(ol_txrx_soc_handle soc, uint8_t *mac_addr)
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->swaf_peer_is_sla_configured) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return false;
	}

	return soc->ops->sawf_ops->swaf_peer_is_sla_configured(soc, mac_addr);
}

#else
static inline QDF_STATUS
cdp_sawf_mpdu_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_sawf_mpdu_details_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_get_peer_sawf_delay_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			      uint8_t *mac, void *data)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_get_peer_sawf_tx_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			   uint8_t *mac, void *data)
{
	return QDF_STATUS_E_FAILURE;
}

static inline bool
cdp_swaf_peer_is_sla_configured(ol_txrx_soc_handle soc, uint8_t *mac_addr)
{
	return false;
}
#endif
#endif /* _CDP_TXRX_SAWF_H_ */
