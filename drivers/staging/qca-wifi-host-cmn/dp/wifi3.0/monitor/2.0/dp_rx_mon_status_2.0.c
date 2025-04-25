/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "dp_types.h"
#include "qdf_nbuf.h"
#include "dp_internal.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include <qdf_nbuf_frag.h>
#include <hal_be_api_mon.h>
#include <dp_mon.h>
#include <dp_mon_2.0.h>
#include <dp_rx_mon_2.0.h>
#include <dp_rx.h>
#include <dp_rx_mon.h>

#ifdef BE_PKTLOG_SUPPORT
#define BUFFER_RESIDUE 1
#define RX_MON_MIN_HEAD_ROOM   64

QDF_STATUS
dp_rx_process_pktlog_be(struct dp_soc *soc, struct dp_pdev *pdev,
			struct hal_rx_ppdu_info *ppdu_info,
			void *status_frag, uint32_t end_offset)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	qdf_nbuf_t nbuf = NULL;
	enum WDI_EVENT pktlog_mode = WDI_NO_VAL;

	if (!mon_pdev->dp_peer_based_pktlog &&
	    (mon_pdev->rx_pktlog_mode == DP_RX_PKTLOG_DISABLED)) {
		return QDF_STATUS_E_INVAL;
	}

	nbuf = qdf_nbuf_alloc(soc->osdev, RX_MON_MIN_HEAD_ROOM,
			      RX_BUFFER_RESERVATION, 0, FALSE);
	if (!nbuf)
		return QDF_STATUS_E_NOMEM;

	qdf_nbuf_add_rx_frag(status_frag, nbuf, 0,
			     (end_offset + BUFFER_RESIDUE),
			     RX_MON_MIN_HEAD_ROOM, true);

	if (mon_pdev->dp_peer_based_pktlog && ppdu_info) {
		dp_rx_process_peer_based_pktlog(soc, ppdu_info,
						nbuf, pdev->pdev_id);
	} else {
		if (mon_pdev->rx_pktlog_mode == DP_RX_PKTLOG_FULL)
			pktlog_mode = WDI_EVENT_RX_DESC;
		else if (mon_pdev->rx_pktlog_mode == DP_RX_PKTLOG_LITE)
			pktlog_mode = WDI_EVENT_LITE_RX;

		if (pktlog_mode != WDI_NO_VAL)
			dp_wdi_event_handler(pktlog_mode, soc,
					     nbuf, HTT_INVALID_PEER,
					     WDI_NO_VAL, pdev->pdev_id);
	}
	qdf_nbuf_free(nbuf);

	return QDF_STATUS_SUCCESS;
}
#endif

