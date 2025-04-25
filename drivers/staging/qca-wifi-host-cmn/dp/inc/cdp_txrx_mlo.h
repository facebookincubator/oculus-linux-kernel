/*
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
#ifndef _CDP_TXRX_MLO_H_
#define _CDP_TXRX_MLO_H_
#include "cdp_txrx_ops.h"

struct cdp_mlo_ctxt;

static inline
struct cdp_mlo_ctxt *cdp_mlo_ctxt_attach(ol_txrx_soc_handle soc,
					 struct cdp_ctrl_mlo_mgr *ctrl_ctxt)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->mlo_ops ||
	    !soc->ops->mlo_ops->mlo_ctxt_attach)
		return NULL;

	return soc->ops->mlo_ops->mlo_ctxt_attach(ctrl_ctxt);
}

static inline
void cdp_mlo_ctxt_detach(ol_txrx_soc_handle soc,
			 struct cdp_mlo_ctxt *ml_ctxt)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->mlo_ops ||
	    !soc->ops->mlo_ops->mlo_ctxt_detach)
		return;

	soc->ops->mlo_ops->mlo_ctxt_detach(ml_ctxt);
}

static inline void cdp_soc_mlo_soc_setup(ol_txrx_soc_handle soc,
					 struct cdp_mlo_ctxt *mlo_ctx)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->mlo_ops ||
	    !soc->ops->mlo_ops->mlo_soc_setup)
		return;

	soc->ops->mlo_ops->mlo_soc_setup(soc, mlo_ctx);
}

static inline void cdp_soc_mlo_soc_teardown(ol_txrx_soc_handle soc,
					    struct cdp_mlo_ctxt *mlo_ctx,
					    bool is_force_down)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->mlo_ops ||
	    !soc->ops->mlo_ops->mlo_soc_teardown)
		return;

	soc->ops->mlo_ops->mlo_soc_teardown(soc, mlo_ctx, is_force_down);
}

static inline void cdp_mlo_setup_complete(ol_txrx_soc_handle soc,
					  struct cdp_mlo_ctxt *mlo_ctx)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->mlo_ops ||
	    !soc->ops->mlo_ops->mlo_setup_complete)
		return;

	soc->ops->mlo_ops->mlo_setup_complete(mlo_ctx);
}

/*
 * cdp_mlo_update_delta_tsf2 - Update delta_tsf2
 * @soc: soc handle
 * @pdev_id: pdev id
 * @delta_tsf2: delta_tsf2
 *
 * return: none
 */
static inline void cdp_mlo_update_delta_tsf2(ol_txrx_soc_handle soc,
					     uint8_t pdev_id,
					     uint64_t delta_tsf2)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->mlo_ops ||
	    !soc->ops->mlo_ops->mlo_update_delta_tsf2)
		return;

	soc->ops->mlo_ops->mlo_update_delta_tsf2(soc, pdev_id, delta_tsf2);
}

/*
 * cdp_mlo_update_delta_tqm - Update delta_tqm
 * @soc: soc handle
 * @delta_tqm: delta_tqm
 *
 * return: none
 */
static inline void cdp_mlo_update_delta_tqm(ol_txrx_soc_handle soc,
					    uint64_t delta_tqm)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->mlo_ops ||
	    !soc->ops->mlo_ops->mlo_update_delta_tqm)
		return;

	soc->ops->mlo_ops->mlo_update_delta_tqm(soc, delta_tqm);
}

/*
 * cdp_mlo_get_mld_vdev_stats - Get MLD vdev stats
 * @soc: soc handle
 * @vdev_id: vdev_id of one of the vdev's of the MLD group
 * @buf: buffer to hold vdev_stats
 * @link_vdev_only: flag to indicate if stats are required for specific vdev
 *
 * return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_mlo_get_mld_vdev_stats(ol_txrx_soc_handle soc,
			   uint8_t vdev_id, struct cdp_vdev_stats *buf,
			   bool link_vdev_only)
{
	if (!soc || !soc->ops) {
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->mlo_ops || !soc->ops->mlo_ops->mlo_get_mld_vdev_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->mlo_ops->mlo_get_mld_vdev_stats(soc,
							 vdev_id,
							 buf,
							 link_vdev_only);
}
#endif /*_CDP_TXRX_MLO_H_*/
