/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <dp_types.h>
#include <dp_internal.h>
#include <wlan_cfg.h>
#include <wlan_dp_lapb_flow.h>
#include "qdf_time.h"
#include "qdf_util.h"
#include "hal_internal.h"
#include "hal_api.h"
#include "hif.h"
#include <qdf_status.h>
#include <qdf_nbuf.h>
#include "qdf_hrtimer.h"
#include "qdf_types.h"
#include "dp_tx.h"

/**
 * dp_lapb_tcl_hp_update_timer_handler() - LAPB timer interrupt handler
 * @arg: private data of the timer
 *
 * Returns: none
 */
static enum qdf_hrtimer_restart_status
dp_lapb_tcl_hp_update_timer_handler(qdf_hrtimer_data_t *arg)
{
	struct dp_soc *soc;
	struct wlan_lapb *lapb_ctx;
	hal_ring_handle_t hal_ring_hdl;
	uint8_t ring_id;

	lapb_ctx = qdf_container_of(arg, struct wlan_lapb,
				    lapb_flow_timer);

	ring_id = lapb_ctx->ring_id;
	soc = lapb_ctx->soc;

	hal_ring_hdl = soc->tcl_data_ring[ring_id].hal_srng;
	if (!hal_ring_hdl)
		goto fail;

	if (hal_srng_try_access_start(soc->hal_soc, hal_ring_hdl) < 0)
		goto fail;

	if (hif_rtpm_get(HIF_RTPM_GET_ASYNC, HIF_RTPM_ID_DP)) {
		hal_srng_access_end_reap(soc->hal_soc, hal_ring_hdl);
		hal_srng_set_event(hal_ring_hdl, HAL_SRNG_FLUSH_EVENT);
		hal_srng_inc_flush_cnt(hal_ring_hdl);
		goto fail;
	}

	hal_srng_access_end(soc->hal_soc, hal_ring_hdl);
	hif_rtpm_put(HIF_RTPM_PUT_ASYNC, HIF_RTPM_ID_DP);

	lapb_ctx->stats.timer_expired++;
fail:
	return QDF_HRTIMER_NORESTART;
}

/**
 * dp_lapb_tcl_hp_update_timer() - handle non-flush packets latency
 * @soc: Datapath global soc handle
 * @max_latency: Tx packet mapped flow latency
 *
 * Returns: none
 */
static inline void
dp_lapb_tcl_hp_update_timer(struct dp_soc *soc, uint32_t max_latency)
{
	struct wlan_lapb *lapb_ctx = &soc->lapb;
	qdf_hrtimer_data_t *lapb_flow_timer =
				&lapb_ctx->lapb_flow_timer;
	uint64_t time_remaining;
	bool test;

	if (qdf_hrtimer_callback_running(lapb_flow_timer))
		return;

	test = qdf_hrtimer_is_queued(lapb_flow_timer);
	if (!test) {
		qdf_hrtimer_start(lapb_flow_timer,
				  qdf_time_ms_to_ktime(max_latency),
				  QDF_HRTIMER_MODE_REL);
		return;
	}

	time_remaining = qdf_ktime_to_ms
				(qdf_hrtimer_get_remaining(lapb_flow_timer));
	if (time_remaining > max_latency) {
		qdf_hrtimer_kill(lapb_flow_timer);
		qdf_hrtimer_start(lapb_flow_timer,
				  qdf_time_ms_to_ktime(max_latency),
				  QDF_HRTIMER_MODE_REL);
	}
}

/**
 * dp_lapb_handle_tcl_hp_update() - set flag to trigger TCL hp update
 * @soc: Datapath global soc handle
 * @coalesce: TCL hp update trigger flag
 * @msdu_info: pointer to tx descriptor
 *
 * Returns: none
 */
static inline void
dp_lapb_handle_tcl_hp_update(struct dp_soc *soc, int *coalesce,
			     struct dp_tx_msdu_info_s *msdu_info)
{
	struct wlan_lapb *lapb_ctx = &soc->lapb;
	qdf_hrtimer_data_t *lapb_flow_timer =
				&lapb_ctx->lapb_flow_timer;

	if (msdu_info->frm_type == dp_tx_frm_tso) {
		if (msdu_info->skip_hp_update) {
			if (qdf_hrtimer_is_queued(lapb_flow_timer))
				qdf_hrtimer_kill(lapb_flow_timer);

			*coalesce = 1;
			return;
		}
	}

	if (qdf_hrtimer_is_queued(lapb_flow_timer))
		qdf_hrtimer_kill(lapb_flow_timer);

	*coalesce = 0;
	lapb_ctx->stats.app_spcl_ind_recvd++;
}

/**
 * dp_lapb_update_dscp_if_needed() - replace spcl dscp with def dscp for
 * flush packets and set flush indication bit
 * @nbuf: sk_buff abstraction
 * @spl_dscp: Tx packet mapped flow spcl dscp value
 * @def_dscp: Tx packet mapped flow def dscp value
 *
 * Returns: none
 */
static inline void
dp_lapb_update_dscp_if_needed(qdf_nbuf_t nbuf, uint8_t spl_dscp,
			      uint8_t def_dscp)
{
	uint8_t dscp, ecn, tos;

	if (qdf_nbuf_data_is_ipv4_pkt(qdf_nbuf_data(nbuf)))
		tos = qdf_nbuf_data_get_ipv4_tos(qdf_nbuf_data(nbuf));
	else if (qdf_nbuf_is_ipv6_pkt(nbuf))
		tos = qdf_nbuf_data_get_ipv6_tc(qdf_nbuf_data(nbuf));
	else
		return;

	ecn = (tos & ~QDF_NBUF_PKT_IPV4_DSCP_MASK);
	dscp = (tos & QDF_NBUF_PKT_IPV4_DSCP_MASK) >>
		QDF_NBUF_PKT_IPV4_DSCP_SHIFT;

	if (spl_dscp == dscp) {
		tos = ((def_dscp << QDF_NBUF_PKT_IPV4_DSCP_SHIFT) | ecn);
		if (qdf_nbuf_data_is_ipv4_pkt(qdf_nbuf_data(nbuf))) {
			qdf_nbuf_data_set_ipv4_tos(qdf_nbuf_data(nbuf), tos);
			qdf_nbuf_set_tx_ip_cksum(nbuf);
			if (qdf_nbuf_is_ipv4_last_fragment(nbuf))
				SET_FLAGS_LAPB_FLUSH_IND(nbuf->mark);
		} else if (qdf_nbuf_is_ipv6_pkt(nbuf)) {
			qdf_nbuf_data_set_ipv6_tc(qdf_nbuf_data(nbuf), tos);
		}
	}
}

/**
 * wlan_dp_lapb_handle_app_ind() - handle flush indication
 * @nbuf: sk_buff abstraction
 *
 * Returns: QDF_STATUS_SUCCESS
 */
QDF_STATUS
wlan_dp_lapb_handle_app_ind(qdf_nbuf_t nbuf)
{
	uint8_t spl_dscp, def_dscp;
	QDF_STATUS status;

	if (!IS_LAPB_FRAME(nbuf->mark))
		return QDF_STATUS_SUCCESS;

	status = dp_svc_get_app_ind_special_dscp_by_id(GET_SERVICE_CLASS_ID(nbuf->mark),
						       &spl_dscp);
	if (status != QDF_STATUS_SUCCESS)
		return QDF_STATUS_SUCCESS;

	status = dp_svc_get_app_ind_default_dscp_by_id(GET_SERVICE_CLASS_ID(nbuf->mark),
						       &def_dscp);
	if (status != QDF_STATUS_SUCCESS)
		return QDF_STATUS_SUCCESS;

	dp_lapb_update_dscp_if_needed(nbuf, spl_dscp, def_dscp);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_dp_lapb_handle_frame() - handle tcl hp update
 * @soc: Datapath global soc handle
 * @nbuf: sk_buff abstraction
 * @coalesce: TCL hp update trigger flag
 * @msdu_info: pointer to tx descriptor
 *
 * Returns: none
 */
void
wlan_dp_lapb_handle_frame(struct dp_soc *soc, qdf_nbuf_t  nbuf, int *coalesce,
			  struct dp_tx_msdu_info_s *msdu_info)
{
	uint32_t latency;
	struct wlan_lapb *lapb_ctx = &soc->lapb;

	if (!IS_LAPB_FRAME(nbuf->mark))
		return;

	if (GET_FLAGS_LAPB_FLUSH_IND(nbuf->mark)) {
		dp_lapb_handle_tcl_hp_update(soc, coalesce, msdu_info);
	} else {
		dp_svc_get_buffer_latency_tolerance_by_id(GET_SERVICE_CLASS_ID(nbuf->mark),
							  &latency);
		dp_lapb_tcl_hp_update_timer(soc, latency);
		*coalesce = 1;
	}

	DP_STATS_INC(lapb_ctx, pkt_recvd, 1);
}

static struct wlan_lapb_ops dp_lapb_flow_ops = {
	.wlan_dp_lapb_handle_frame = wlan_dp_lapb_handle_frame,
};

/**
 * wlan_dp_lapb_flow_attach() - attach the LAPB flow
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS wlan_dp_lapb_flow_attach(struct dp_soc *soc)
{
	struct wlan_lapb *lapb_ctx = &soc->lapb;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;

	/* Check if it is enabled in the INI */
	if (!wlan_cfg_is_lapb_enabled(cfg)) {
		dp_info("LAPB: ini is disabled");
		lapb_ctx->is_init = false;
		return QDF_STATUS_E_NOSUPPORT;
	}

	lapb_ctx->ring_id = soc->num_tcl_data_rings - 1;
	lapb_ctx->ops = &dp_lapb_flow_ops;
	qdf_hrtimer_init(&lapb_ctx->lapb_flow_timer,
			 dp_lapb_tcl_hp_update_timer_handler,
			 QDF_CLOCK_MONOTONIC,
			 QDF_HRTIMER_MODE_REL,
			 QDF_CONTEXT_HARDWARE);

	lapb_ctx->soc = soc;
	lapb_ctx->is_init = true;
	qdf_mem_set(&lapb_ctx->stats, 0, sizeof(struct wlan_lapb_stats));

	dp_info("LAPB attach");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_dp_lapb_flow_detach() - detach the LAPB flow
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS wlan_dp_lapb_flow_detach(struct dp_soc *soc)
{
	struct wlan_lapb *lapb_ctx = &soc->lapb;

	if (!lapb_ctx->is_init)
		return QDF_STATUS_SUCCESS;

	lapb_ctx->ops = NULL;
	lapb_ctx->is_init = false;

	qdf_hrtimer_cancel(&lapb_ctx->lapb_flow_timer);
	dp_info("LAPB detach");
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_dp_lapb_display_stats() - display LAPB stats
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS wlan_dp_lapb_display_stats(struct dp_soc *soc)
{
	struct wlan_lapb *lapb_ctx = &soc->lapb;

	dp_info("LAPB stats");
	dp_info("Packet received: %u", lapb_ctx->stats.pkt_recvd);
	dp_info("Timer expired: %u", lapb_ctx->stats.timer_expired);
	dp_info("App ind received: %u", lapb_ctx->stats.app_spcl_ind_recvd);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_dp_lapb_clear_stats() - clear LAPB stats
 * @soc: Datapath global soc handle
 *
 * Returns: none
 */
void wlan_dp_lapb_clear_stats(struct dp_soc *soc)
{
	struct wlan_lapb *lapb_ctx = &soc->lapb;

	qdf_mem_set(&lapb_ctx->stats, 0, sizeof(struct wlan_lapb_stats));
}
