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

#ifndef _WLAN_DP_LAPB_FLOW_H_
#define _WLAN_DP_LAPB_FLOW_H_

#ifdef WLAN_SUPPORT_LAPB
#include <dp_types.h>
#include <qdf_status.h>
#include <wlan_dp_svc.h>
#include <wlan_dp_metadata.h>

/**
 * wlan_dp_lapb_handle_frame() - handle tcl hp update
 * @soc: Datapath global soc handle
 * @nbuf: sk_buff abstraction
 * @coalesce: TCL hp update trigger flag
 * @msdu_info: pointer to tx descriptor
 *
 * Returns: none
 */
void wlan_dp_lapb_handle_frame(struct dp_soc *soc, qdf_nbuf_t  nbuf,
			       int *coalesce,
			       struct dp_tx_msdu_info_s *msdu_info);

/**
 * wlan_dp_lapb_handle_app_ind() - handle flush indication
 * @nbuf: sk_buff abstraction
 *
 * Returns: none
 */
QDF_STATUS wlan_dp_lapb_handle_app_ind(qdf_nbuf_t nbuf);

/**
 * wlan_dp_lapb_flow_attach() - attach the LAPB flow
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS wlan_dp_lapb_flow_attach(struct dp_soc *soc);

/**
 * wlan_dp_lapb_flow_detach() - detach the LAPB flow
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS wlan_dp_lapb_flow_detach(struct dp_soc *soc);

/**
 * wlan_dp_lapb_display_stats() - display LAPB stats
 * @soc: Datapath global soc handle
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS wlan_dp_lapb_display_stats(struct dp_soc *soc);

/**
 * wlan_dp_lapb_clear_stats() - clear LAPB stats
 * @soc: Datapath global soc handle
 *
 * Returns: none
 */
void wlan_dp_lapb_clear_stats(struct dp_soc *soc);

/**
 * wlan_dp_is_lapb_frame() - Check if lapb packet
 * @soc: Datapath global soc handle
 * @nbuf: sk_buff abstraction
 *
 * Returns: QDF_STATUS
 */
static inline bool wlan_dp_is_lapb_frame(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
	return IS_LAPB_FRAME(nbuf->mark);
}
#endif /* WLAN_SUPPORT_LAPB */
#endif
