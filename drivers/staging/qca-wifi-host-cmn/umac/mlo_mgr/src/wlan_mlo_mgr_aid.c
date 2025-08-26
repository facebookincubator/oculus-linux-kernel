/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wlan_mlo_mgr_main.h"
#include "qdf_types.h"
#include "wlan_cmn.h"
#include <include/wlan_vdev_mlme.h>
#include "wlan_mlo_mgr_ap.h"
#include "wlan_mlo_mgr_cmn.h"

static void mlo_peer_set_aid_bit(struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
				 uint16_t assoc_id_ix)
{
	uint16_t ix;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;

	/* Mark this bit as AID assigned */
	for (ix = 0; ix < WLAN_UMAC_MLO_MAX_VDEVS; ix++) {
		vdev_aid_mgr = ml_aid_mgr->aid_mgr[ix];
		if (vdev_aid_mgr)
			qdf_set_bit(assoc_id_ix, vdev_aid_mgr->aid_bitmap);
	}
}

static bool wlan_mlo_check_aid_free(struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
				    uint16_t assoc_idx, bool skip_link,
				    uint8_t link_ix)
{
	uint16_t j;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;

	for (j = 0; j < WLAN_UMAC_MLO_MAX_VDEVS; j++) {
		if (skip_link && j == link_ix)
			continue;

		vdev_aid_mgr = ml_aid_mgr->aid_mgr[j];
		if (vdev_aid_mgr &&
		    qdf_test_bit(assoc_idx, vdev_aid_mgr->aid_bitmap))
			break;

		/* AID is free */
		if (j == WLAN_UMAC_MLO_MAX_VDEVS - 1)
			return true;
	}

	return false;
}

static bool wlan_mlo_aid_idx_check(uint16_t start_idx, uint16_t end_idx,
				   uint16_t curr_idx)
{
	if (start_idx < end_idx)
		return (curr_idx < end_idx);

	return (curr_idx >= end_idx);
}

static int32_t wlan_mlo_aid_idx_update(uint16_t start_idx, uint16_t end_idx,
				       uint16_t curr_idx)
{
	if (start_idx < end_idx)
		return (curr_idx + 1);

	if (curr_idx >= end_idx)
		return ((int32_t)curr_idx - 1);

	mlo_err("AID index is out of sync");
	QDF_BUG(0);
	return 0;
}

static uint16_t wlan_mlo_alloc_aid(struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
				   uint16_t start_idx, uint16_t end_idx,
				   uint8_t link_ix, bool is_mlo_peer)
{
	uint16_t assoc_id = (uint16_t)-1;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	uint16_t first_aid = 0;
	uint16_t assoc_idx = start_idx;
	int32_t signed_assoc_idx = assoc_idx;

	while (wlan_mlo_aid_idx_check(start_idx, end_idx, assoc_idx)) {
		if (qdf_test_bit(assoc_idx, ml_aid_mgr->aid_bitmap)) {
			signed_assoc_idx = wlan_mlo_aid_idx_update(start_idx,
								   end_idx,
								   assoc_idx);
			if (signed_assoc_idx < 0)
				break;

			assoc_idx = signed_assoc_idx;
			continue;
		}

		if (is_mlo_peer) {
			if (wlan_mlo_check_aid_free(ml_aid_mgr, assoc_idx,
						    false, link_ix)) {
				/* associd available */
				mlo_peer_set_aid_bit(ml_aid_mgr, assoc_idx);
				qdf_set_bit(assoc_idx, ml_aid_mgr->aid_bitmap);
				assoc_id = assoc_idx + 1;
				break;
			}
		} else {
			vdev_aid_mgr = ml_aid_mgr->aid_mgr[link_ix];
			if (!vdev_aid_mgr)
				break;

			if (qdf_test_bit(assoc_idx, vdev_aid_mgr->aid_bitmap)) {
				signed_assoc_idx =
					wlan_mlo_aid_idx_update(start_idx,
								end_idx,
								assoc_idx);
				if (signed_assoc_idx < 0)
					break;

				assoc_idx = signed_assoc_idx;
				continue;
			}

			if (!first_aid)
				first_aid = assoc_idx + 1;

			/* Check whether this bit used by other VDEV
			 * Non-MLO peers
			 */
			if (!wlan_mlo_check_aid_free(ml_aid_mgr, assoc_idx,
						     true, link_ix)) {
				/* Assoc ID is used by other link, return this
				 * aid to caller
				 */
				assoc_id = assoc_idx + 1;
				vdev_aid_mgr = ml_aid_mgr->aid_mgr[link_ix];
				qdf_set_bit(assoc_idx,
					    vdev_aid_mgr->aid_bitmap);
				first_aid = 0;
				break;
			}
		}

		signed_assoc_idx = wlan_mlo_aid_idx_update(start_idx,
							   end_idx, assoc_idx);
		if (signed_assoc_idx < 0)
			break;
		assoc_idx = signed_assoc_idx;
	}

	if ((!is_mlo_peer) && first_aid) {
		vdev_aid_mgr = ml_aid_mgr->aid_mgr[link_ix];
		qdf_set_bit(first_aid - 1, vdev_aid_mgr->aid_bitmap);
		assoc_id = first_aid;
	}

	return assoc_id;
}

#ifdef WLAN_FEATURE_11BE
#define AID_NUM_BUCKET 3
static uint16_t _wlan_mlo_peer_alloc_aid(
		struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
		bool is_mlo_peer, bool t2lm_peer,
		uint8_t link_ix)
{
	uint16_t assoc_id = (uint16_t)-1;
	uint16_t start_aid, aid_end1, aid_end2, tot_aid;
	uint16_t pool_1_max_aid;

	start_aid = ml_aid_mgr->start_aid;
	if (start_aid > ml_aid_mgr->max_aid) {
		mlo_err("MAX AID %d is less than start aid %d ",
			ml_aid_mgr->max_aid, start_aid);
		return assoc_id;
	}

	tot_aid = ml_aid_mgr->max_aid - start_aid;
	pool_1_max_aid = tot_aid / AID_NUM_BUCKET;
	aid_end1 = pool_1_max_aid + start_aid;
	aid_end2 = pool_1_max_aid + pool_1_max_aid + start_aid;

	mlo_debug("max_aid = %d start_aid = %d tot_aid = %d pool_1_max_aid = %d aid_end1 = %d aid_end2 = %d",
		  ml_aid_mgr->max_aid, start_aid, tot_aid, pool_1_max_aid,
		  aid_end1, aid_end2);
	if ((start_aid > aid_end1) || (aid_end1 > aid_end2)) {
		assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, start_aid,
					      ml_aid_mgr->max_aid, link_ix,
					      is_mlo_peer);
		return assoc_id;
	}
	mlo_debug("T2LM peer = %d", t2lm_peer);

	if (t2lm_peer) {
		assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, aid_end1,
					      aid_end2, link_ix,
					      is_mlo_peer);

		if (assoc_id != (uint16_t)-1)
			return assoc_id;

		assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, aid_end2,
					      ml_aid_mgr->max_aid,
					      link_ix, is_mlo_peer);

		if (assoc_id != (uint16_t)-1)
			return assoc_id;

		assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, aid_end1,
					      start_aid, link_ix,
					      is_mlo_peer);
	} else {
		assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, start_aid,
					      aid_end1, link_ix,
					      is_mlo_peer);

		if (assoc_id != (uint16_t)-1)
			return assoc_id;

		assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, aid_end2,
					      ml_aid_mgr->max_aid,
					      link_ix, is_mlo_peer);

		if (assoc_id != (uint16_t)-1)
			return assoc_id;

		assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, aid_end2,
					      aid_end1, link_ix,
					      is_mlo_peer);
	}

	return assoc_id;
}
#else
static uint16_t _wlan_mlo_peer_alloc_aid(
		struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
		bool is_mlo_peer, bool t2lm_peer,
		uint8_t link_ix)
{
	uint16_t assoc_id = (uint16_t)-1;

	assoc_id = wlan_mlo_alloc_aid(ml_aid_mgr, ml_aid_mgr->start_aid,
				      ml_aid_mgr->max_aid,
				      link_ix, is_mlo_peer);

	return assoc_id;
}
#endif

static uint16_t wlan_mlo_peer_alloc_aid(
		struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
		bool is_mlo_peer, bool t2lm_peer,
		uint8_t link_ix)
{
	uint16_t assoc_id = (uint16_t)-1;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_mgr_ctx) {
		mlo_err(" MLO mgr context is NULL, assoc id alloc failed");
		return assoc_id;
	}

	if (!is_mlo_peer && link_ix == MLO_INVALID_LINK_IDX) {
		mlo_err(" is MLO peer %d, link_ix %d", is_mlo_peer, link_ix);
		return assoc_id;
	}
	/* TODO check locking strategy */
	ml_aid_lock_acquire(mlo_mgr_ctx);

	assoc_id = _wlan_mlo_peer_alloc_aid(ml_aid_mgr, is_mlo_peer,
					    t2lm_peer, link_ix);
	if (assoc_id == (uint16_t)-1)
		mlo_err("MLO aid allocation failed (reached max)");

	ml_aid_lock_release(mlo_mgr_ctx);

	return assoc_id;
}

static uint16_t wlan_mlme_peer_alloc_aid(
		struct wlan_vdev_aid_mgr *vdev_aid_mgr,
		bool no_lock)
{
	uint16_t assoc_id = (uint16_t)-1;
	uint16_t i;
	uint16_t start_aid;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_mgr_ctx)
		return assoc_id;

	if (!no_lock)
		ml_aid_lock_acquire(mlo_mgr_ctx);

	start_aid = vdev_aid_mgr->start_aid;
	for (i = start_aid; i < vdev_aid_mgr->max_aid; i++) {
		if (qdf_test_bit(i, vdev_aid_mgr->aid_bitmap))
			continue;

		assoc_id = i + 1;
		qdf_set_bit(i, vdev_aid_mgr->aid_bitmap);
		break;
	}

	if (!no_lock)
		ml_aid_lock_release(mlo_mgr_ctx);

	if (i == vdev_aid_mgr->max_aid)
		return (uint16_t)-1;

	return assoc_id;
}

static QDF_STATUS wlan_mlo_peer_set_aid(
		struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
		bool is_mlo_peer,
		uint8_t link_ix,
		uint16_t assoc_id)
{
	uint16_t j;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_mgr_ctx)
		return QDF_STATUS_E_FAILURE;

	if (!is_mlo_peer && link_ix == 0xff)
		return QDF_STATUS_E_FAILURE;
	/* TODO check locking strategy */
	ml_aid_lock_acquire(mlo_mgr_ctx);

	if (qdf_test_bit(WLAN_AID(assoc_id) - 1,  ml_aid_mgr->aid_bitmap)) {
		ml_aid_lock_release(mlo_mgr_ctx);
		mlo_err("Assoc id %d is not available on ml aid mgr", assoc_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (is_mlo_peer) {
		if ((assoc_id < ml_aid_mgr->start_aid) ||
		    (assoc_id >= ml_aid_mgr->max_aid)) {
			ml_aid_lock_release(mlo_mgr_ctx);
			mlo_err("Assoc id %d is not in bounds, start aid %d, max aid %d",
				assoc_id, ml_aid_mgr->start_aid,
				ml_aid_mgr->max_aid);
			return QDF_STATUS_E_FAILURE;
		}
		for (j = 0; j < WLAN_UMAC_MLO_MAX_VDEVS; j++) {
			vdev_aid_mgr = ml_aid_mgr->aid_mgr[j];
			if (vdev_aid_mgr &&
			    qdf_test_bit(WLAN_AID(assoc_id) - 1,
					 vdev_aid_mgr->aid_bitmap)) {
				ml_aid_lock_release(mlo_mgr_ctx);
				mlo_err("Assoc id %d is not available on link vdev %d",
					assoc_id, j);
				return QDF_STATUS_E_FAILURE;
			}
			/* AID is free */
			if (j == WLAN_UMAC_MLO_MAX_VDEVS - 1)
				mlo_peer_set_aid_bit(ml_aid_mgr,
						     WLAN_AID(assoc_id) - 1);
		}
		qdf_set_bit(WLAN_AID(assoc_id) - 1, ml_aid_mgr->aid_bitmap);
	} else {
		vdev_aid_mgr = ml_aid_mgr->aid_mgr[link_ix];
		if (!vdev_aid_mgr) {
			ml_aid_lock_release(mlo_mgr_ctx);
			return QDF_STATUS_E_FAILURE;
		}
		if ((assoc_id < vdev_aid_mgr->start_aid) ||
		    (assoc_id >= vdev_aid_mgr->max_aid)) {
			ml_aid_lock_release(mlo_mgr_ctx);
			mlo_err("Assoc id %d is not in bounds, start aid %d, max aid %d",
				assoc_id, vdev_aid_mgr->start_aid,
				vdev_aid_mgr->max_aid);
			return QDF_STATUS_E_FAILURE;
		}

		if (qdf_test_bit(WLAN_AID(assoc_id) - 1,
				 vdev_aid_mgr->aid_bitmap)) {
			ml_aid_lock_release(mlo_mgr_ctx);
			mlo_err("Assoc id %d is not available on vdev aid mgr",
				assoc_id);
			return QDF_STATUS_E_FAILURE;
		}

		qdf_set_bit(WLAN_AID(assoc_id) - 1, vdev_aid_mgr->aid_bitmap);
	}

	ml_aid_lock_release(mlo_mgr_ctx);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wlan_mlme_peer_set_aid(
		struct wlan_vdev_aid_mgr *vdev_aid_mgr,
		bool no_lock, uint16_t assoc_id)
{
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!mlo_mgr_ctx)
		return status;

	if (!no_lock)
		ml_aid_lock_acquire(mlo_mgr_ctx);

	if ((assoc_id < vdev_aid_mgr->start_aid) ||
	    (assoc_id >= vdev_aid_mgr->max_aid)) {
		if (!no_lock)
			ml_aid_lock_release(mlo_mgr_ctx);

		mlo_err("Assoc id %d is not in bounds, start aid %d, max aid %d",
			assoc_id, vdev_aid_mgr->start_aid,
			vdev_aid_mgr->max_aid);
		return QDF_STATUS_E_FAILURE;
	}

	if (!qdf_test_bit(WLAN_AID(assoc_id) - 1, vdev_aid_mgr->aid_bitmap)) {
		qdf_set_bit(WLAN_AID(assoc_id) - 1, vdev_aid_mgr->aid_bitmap);
		status = QDF_STATUS_SUCCESS;
	}

	if (!no_lock)
		ml_aid_lock_release(mlo_mgr_ctx);

	return status;
}

QDF_STATUS wlan_mlo_peer_free_aid(
		struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
		uint8_t link_ix,
		uint16_t assoc_id)
{
	uint16_t  j;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();
	uint16_t assoc_id_ix;

	if (!mlo_mgr_ctx)
		return QDF_STATUS_E_FAILURE;

	/* TODO check locking strategy */
	ml_aid_lock_acquire(mlo_mgr_ctx);
	assoc_id_ix = WLAN_AID(assoc_id) - 1;
	if (qdf_test_bit(assoc_id_ix, ml_aid_mgr->aid_bitmap)) {
		qdf_clear_bit(assoc_id_ix, ml_aid_mgr->aid_bitmap);
		for (j = 0; j < WLAN_UMAC_MLO_MAX_VDEVS; j++) {
			vdev_aid_mgr = ml_aid_mgr->aid_mgr[j];
			if (vdev_aid_mgr &&
			    qdf_test_bit(assoc_id_ix,
					 vdev_aid_mgr->aid_bitmap)) {
				qdf_clear_bit(assoc_id_ix,
					      vdev_aid_mgr->aid_bitmap);
			}
		}
	} else {
		if ((link_ix != 0xff) && (link_ix < WLAN_UMAC_MLO_MAX_VDEVS)) {
			vdev_aid_mgr = ml_aid_mgr->aid_mgr[link_ix];
			if (vdev_aid_mgr)
				qdf_clear_bit(assoc_id_ix,
					      vdev_aid_mgr->aid_bitmap);
		} else {
			mlo_err("AID free failed, link ix(%d) is invalid for assoc_id %d",
				link_ix, assoc_id);
		}
	}

	ml_aid_lock_release(mlo_mgr_ctx);

	return QDF_STATUS_SUCCESS;
}

static int wlan_mlme_peer_aid_is_set(struct wlan_vdev_aid_mgr *vdev_aid_mgr,
				     bool no_lock, uint16_t assoc_id)
{
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();
	int isset = 0;

	if (!mlo_mgr_ctx)
		return isset;

	if (!no_lock)
		ml_aid_lock_acquire(mlo_mgr_ctx);

	isset = qdf_test_bit(WLAN_AID(assoc_id) - 1, vdev_aid_mgr->aid_bitmap);

	if (!no_lock)
		ml_aid_lock_release(mlo_mgr_ctx);

	return isset;
}

void wlan_mlme_peer_free_aid(
		struct wlan_vdev_aid_mgr *vdev_aid_mgr,
		bool no_lock, uint16_t assoc_id)
{
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_mgr_ctx)
		return;

	if (!no_lock)
		ml_aid_lock_acquire(mlo_mgr_ctx);

	qdf_clear_bit(WLAN_AID(assoc_id) - 1, vdev_aid_mgr->aid_bitmap);

	if (!no_lock)
		ml_aid_lock_release(mlo_mgr_ctx);
}

uint16_t wlan_mlme_get_aid_count(struct wlan_objmgr_vdev *vdev)
{
	uint16_t i;
	uint16_t aid_count = 0;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;

	vdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
	if (!vdev_aid_mgr)
		return (uint16_t)-1;

	for (i = 0; i < vdev_aid_mgr->max_aid; i++) {
		if (qdf_test_bit(i, vdev_aid_mgr->aid_bitmap))
			aid_count++;
	}

	return aid_count;
}

#ifdef WLAN_FEATURE_11BE
static bool mlo_peer_t2lm_enabled(struct wlan_mlo_peer_context *ml_peer)
{
	if (ml_peer->t2lm_policy.t2lm_enable_val > WLAN_T2LM_NOT_SUPPORTED &&
	    ml_peer->t2lm_policy.t2lm_enable_val < WLAN_T2LM_ENABLE_INVALID)
		return true;

	return false;
}
#else
static bool mlo_peer_t2lm_enabled(struct wlan_mlo_peer_context *ml_peer)
{
	return false;
}
#endif

QDF_STATUS mlo_peer_allocate_aid(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer)
{
	uint16_t assoc_id = (uint16_t)-1;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_mlo_ap *ap_ctx;
	bool t2lm_peer = false;

	ap_ctx = ml_dev->ap_ctx;
	if (!ap_ctx) {
		mlo_err("MLD ID %d ap_ctx is NULL", ml_dev->mld_id);
		return QDF_STATUS_E_INVAL;
	}

	ml_aid_mgr = ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr) {
		mlo_err("MLD ID %d aid mgr is NULL", ml_dev->mld_id);
		return QDF_STATUS_E_INVAL;
	}

	t2lm_peer = mlo_peer_t2lm_enabled(ml_peer);

	assoc_id = wlan_mlo_peer_alloc_aid(ml_aid_mgr, true, t2lm_peer, 0xff);
	if (assoc_id == (uint16_t)-1) {
		mlo_err("MLD ID %d AID alloc failed", ml_dev->mld_id);
		return QDF_STATUS_E_NOENT;
	}

	ml_peer->assoc_id = assoc_id;

	mlo_debug("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " ML assoc id %d",
		  ml_dev->mld_id,
		  QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes), assoc_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlo_peer_free_aid(struct wlan_mlo_dev_context *ml_dev,
			     struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!ml_dev->ap_ctx) {
		mlo_err("ml_dev->ap_ctx is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!ml_peer) {
		mlo_err("ml_peer is null");
		return QDF_STATUS_E_INVAL;
	}

	ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr) {
		mlo_err(" Free failed, ml_aid_mgr is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!ml_peer->assoc_id) {
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " ML assoc id is 0",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return status;
	}

	wlan_mlo_peer_free_aid(ml_aid_mgr, 0xff, ml_peer->assoc_id);

	return status;
}

uint16_t mlo_get_aid(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev;
	uint16_t assoc_id = (uint16_t)-1;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_mlo_ap *ap_ctx;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev)
		return assoc_id;

	ap_ctx = ml_dev->ap_ctx;
	if (!ap_ctx)
		return QDF_STATUS_E_INVAL;

	ml_aid_mgr = ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr)
		return assoc_id;

	return wlan_mlo_peer_alloc_aid(ml_aid_mgr, true, false, 0xff);
}

QDF_STATUS mlo_free_aid(struct wlan_objmgr_vdev *vdev, uint16_t assoc_id)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_mlo_ap *ap_ctx;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev)
		return QDF_STATUS_E_INVAL;

	ap_ctx = ml_dev->ap_ctx;
	if (!ap_ctx)
		return QDF_STATUS_E_INVAL;

	ml_aid_mgr = ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr)
		return QDF_STATUS_E_INVAL;

	return wlan_mlo_peer_free_aid(ml_aid_mgr, 0xff, assoc_id);
}

static QDF_STATUS mlo_peer_set_aid(struct wlan_mlo_dev_context *ml_dev,
				   uint16_t assoc_id)
{
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	QDF_STATUS status;
	struct wlan_mlo_ap *ap_ctx;

	ap_ctx = ml_dev->ap_ctx;
	if (!ap_ctx)
		return QDF_STATUS_E_FAILURE;

	ml_aid_mgr = ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr)
		return QDF_STATUS_E_FAILURE;

	status = wlan_mlo_peer_set_aid(ml_aid_mgr, true, 0xff, assoc_id);

	return status;
}

QDF_STATUS mlo_set_aid(struct wlan_objmgr_vdev *vdev, uint16_t assoc_id)
{
	struct wlan_mlo_dev_context *ml_dev;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev)
		return QDF_STATUS_E_FAILURE;

	return mlo_peer_set_aid(ml_dev, assoc_id);
}

int mlme_is_aid_set(struct wlan_objmgr_vdev *vdev, uint16_t assoc_id)
{
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	bool no_lock = true;

	vdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
	if (vdev_aid_mgr) {
		if (qdf_atomic_read(&vdev_aid_mgr->ref_cnt) > 1)
			no_lock = false;

		return wlan_mlme_peer_aid_is_set(vdev_aid_mgr, no_lock,
						 assoc_id);
	}

	return 0;
}

uint16_t mlme_get_aid(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev;
	uint16_t assoc_id = (uint16_t)-1;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	bool no_lock = true;
	uint8_t link_id;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev) {
		vdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
		if (vdev_aid_mgr) {
			if (qdf_atomic_read(&vdev_aid_mgr->ref_cnt) > 1)
				no_lock = false;
			return wlan_mlme_peer_alloc_aid(vdev_aid_mgr, no_lock);
		}
		return assoc_id;
	}

	ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr)
		return assoc_id;

	link_id = mlo_get_link_vdev_ix(ml_dev, vdev);

	assoc_id = wlan_mlo_peer_alloc_aid(ml_aid_mgr, false, false, link_id);

	return assoc_id;
}

QDF_STATUS mlme_set_aid(struct wlan_objmgr_vdev *vdev,
			uint16_t assoc_id)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	bool no_lock = true;
	uint8_t link_id;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev) {
		vdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
		if (vdev_aid_mgr) {
			if (qdf_atomic_read(&vdev_aid_mgr->ref_cnt) > 1)
				no_lock = false;

			return wlan_mlme_peer_set_aid(vdev_aid_mgr, no_lock,
						      assoc_id);
		} else {
			return QDF_STATUS_E_FAILURE;
		}
	}

	ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr)
		return QDF_STATUS_E_FAILURE;

	link_id = mlo_get_link_vdev_ix(ml_dev, vdev);

	return wlan_mlo_peer_set_aid(ml_aid_mgr, false, link_id, assoc_id);
}

void mlme_free_aid(struct wlan_objmgr_vdev *vdev, uint16_t assoc_id)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	bool no_lock = true;
	uint8_t link_id;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev) {
		vdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
		if (vdev_aid_mgr) {
			if (qdf_atomic_read(&vdev_aid_mgr->ref_cnt) > 1)
				no_lock = false;

			wlan_mlme_peer_free_aid(vdev_aid_mgr, no_lock,
						assoc_id);
		}
		return;
	}

	if (!ml_dev->ap_ctx)
		return;

	ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr)
		return;

	link_id = mlo_get_link_vdev_ix(ml_dev, vdev);

	wlan_mlo_peer_free_aid(ml_aid_mgr, link_id, assoc_id);
}

void wlan_vdev_mlme_aid_mgr_max_aid_set(struct wlan_objmgr_vdev *vdev,
					uint16_t max_aid)
{
	struct wlan_vdev_aid_mgr *aid_mgr;
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_mlo_dev_context *ml_dev;
	uint16_t j, max_sta_count = 0;
	uint16_t aidmgr_sta_count = 0;

	aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
	if (!aid_mgr || !max_aid)
		return;

	mlo_debug("VDEV mgr max aid %d", max_aid);

	aid_mgr->max_aid = max_aid;
	ml_dev = vdev->mlo_dev_ctx;
	if (ml_dev) {
		ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
		if (!ml_aid_mgr)
			return;

		/* Derive lower max_aid */
		for (j = 0; j < WLAN_UMAC_MLO_MAX_VDEVS; j++) {
			vdev_aid_mgr = ml_aid_mgr->aid_mgr[j];
			if (!vdev_aid_mgr)
				continue;

			aidmgr_sta_count = vdev_aid_mgr->max_aid -
					   vdev_aid_mgr->start_aid;
			if (!max_sta_count) {
				max_sta_count = aidmgr_sta_count;
				continue;
			}

			if (max_sta_count > aidmgr_sta_count)
				max_sta_count = aidmgr_sta_count;
		}

		ml_aid_mgr->max_aid = ml_aid_mgr->start_aid + max_sta_count;
		mlo_debug("MLO mgr max aid %d", ml_aid_mgr->max_aid);
	}
}

QDF_STATUS wlan_vdev_mlme_set_start_aid(struct wlan_objmgr_vdev *vdev,
					uint16_t start_aid)
{
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	struct wlan_mlo_dev_context *ml_dev;
	uint16_t j, max_aid_start = 0;

	vdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
	if (!vdev_aid_mgr)
		return QDF_STATUS_E_FAILURE;

	vdev_aid_mgr->start_aid = start_aid;
	mlo_debug("VDEV mgr start aid %d", start_aid);

	ml_dev = vdev->mlo_dev_ctx;
	if (ml_dev) {
		ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
		if (!ml_aid_mgr)
			return QDF_STATUS_E_FAILURE;

		/* Derive higher start_aid */
		for (j = 0; j < WLAN_UMAC_MLO_MAX_VDEVS; j++) {
			vdev_aid_mgr = ml_aid_mgr->aid_mgr[j];
			if (!vdev_aid_mgr)
				continue;

			if (max_aid_start < vdev_aid_mgr->start_aid)
				max_aid_start = vdev_aid_mgr->start_aid;
		}

		ml_aid_mgr->start_aid = max_aid_start;
		mlo_debug("MLO mgr start aid %d", max_aid_start);
	}

	return QDF_STATUS_SUCCESS;
}

uint16_t wlan_vdev_mlme_get_start_aid(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_vdev_aid_mgr *vdev_aid_mgr;

	vdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
	if (!vdev_aid_mgr)
		return 0;

	return vdev_aid_mgr->start_aid;
}

struct wlan_vdev_aid_mgr *wlan_vdev_aid_mgr_init(uint16_t max_aid)
{
	struct wlan_vdev_aid_mgr *aid_mgr;

	aid_mgr = qdf_mem_malloc(sizeof(struct wlan_vdev_aid_mgr));
	if (!aid_mgr)
		return NULL;

	aid_mgr->start_aid = 0;
	aid_mgr->max_aid = max_aid;
	qdf_atomic_init(&aid_mgr->ref_cnt);
	/* Take reference before returning */
	qdf_atomic_inc(&aid_mgr->ref_cnt);

	return aid_mgr;
}

void wlan_vdev_aid_mgr_free(struct wlan_vdev_aid_mgr *aid_mgr)
{
	if (!aid_mgr)
		return;

	if (!qdf_atomic_dec_and_test(&aid_mgr->ref_cnt))
		return;

	aid_mgr->max_aid = 0;
	qdf_mem_free(aid_mgr);
}

QDF_STATUS wlan_mlo_vdev_init_mbss_aid_mgr(struct wlan_mlo_dev_context *ml_dev,
					   struct wlan_objmgr_vdev *vdev,
					   struct wlan_objmgr_vdev *tx_vdev)
{
	struct wlan_vdev_aid_mgr *aid_mgr;
	struct wlan_vdev_aid_mgr *txvdev_aid_mgr;
	struct wlan_objmgr_vdev *vdev_iter;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	uint16_t start_aid = 0;
	uint8_t i;

	if (!ml_dev) {
		mlo_err("ML DEV pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr) {
		mlo_err("AID mgr of ML VDEV(%d) is invalid", ml_dev->mld_id);
		return QDF_STATUS_E_FAILURE;
	}

	txvdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(tx_vdev);
	if (!txvdev_aid_mgr) {
		mlo_err("AID mgr of Tx VDEV%d is invalid",
			wlan_vdev_get_id(tx_vdev));
		return QDF_STATUS_E_FAILURE;
	}

	aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_iter = ml_dev->wlan_vdev_list[i];
		if (!vdev_iter)
			continue;

		if (vdev != vdev_iter)
			continue;

		start_aid = wlan_vdev_mlme_get_start_aid(tx_vdev);
		/* Update start_aid, which updates MLO Dev start aid */
		wlan_vdev_mlme_set_start_aid(vdev, start_aid);

		qdf_atomic_inc(&txvdev_aid_mgr->ref_cnt);
		wlan_vdev_mlme_set_aid_mgr(vdev,
					   txvdev_aid_mgr);
		ml_aid_mgr->aid_mgr[i] = txvdev_aid_mgr;

		if (aid_mgr) {
			mlo_info("AID mgr is freed for vdev %d with txvdev %d",
				 wlan_vdev_get_id(vdev),
				 wlan_vdev_get_id(tx_vdev));
			wlan_vdev_aid_mgr_free(aid_mgr);
		}

		mlo_debug("AID mgr replaced for vdev %d with txvdev %d",
			  wlan_vdev_get_id(vdev), wlan_vdev_get_id(tx_vdev));
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_vdev_deinit_mbss_aid_mgr(struct wlan_mlo_dev_context *mldev,
					     struct wlan_objmgr_vdev *vdev,
					     struct wlan_objmgr_vdev *tx_vdev)
{
	struct wlan_vdev_aid_mgr *aid_mgr;
	struct wlan_vdev_aid_mgr *txvdev_aid_mgr;
	struct wlan_objmgr_vdev *vdev_iter;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	uint8_t i;

	if (!mldev) {
		mlo_err("ML DEV pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ml_aid_mgr = mldev->ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr) {
		mlo_err("AID mgr of ML VDEV(%d) is invalid", mldev->mld_id);
		return QDF_STATUS_E_FAILURE;
	}

	txvdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(tx_vdev);
	if (!txvdev_aid_mgr) {
		mlo_err("AID mgr of Tx VDEV%d is invalid",
			wlan_vdev_get_id(tx_vdev));
		return QDF_STATUS_E_FAILURE;
	}

	aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
	if (!aid_mgr) {
		mlo_err("AID mgr of VDEV%d is invalid",
			wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	if (aid_mgr != txvdev_aid_mgr) {
		mlo_err("AID mgr of VDEV%d and tx vdev(%d) aid mgr doesn't match",
			wlan_vdev_get_id(vdev), wlan_vdev_get_id(tx_vdev));
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_iter = mldev->wlan_vdev_list[i];
		if (!vdev_iter)
			continue;

		if (vdev != vdev_iter)
			continue;

		aid_mgr = wlan_vdev_aid_mgr_init(ml_aid_mgr->max_aid);
		if (!aid_mgr) {
			mlo_err("AID bitmap allocation failed for VDEV%d",
				wlan_vdev_get_id(vdev));
			QDF_BUG(0);
			return QDF_STATUS_E_NOMEM;
		}

		wlan_vdev_mlme_set_aid_mgr(vdev, aid_mgr);
		ml_aid_mgr->aid_mgr[i] = aid_mgr;

		wlan_vdev_aid_mgr_free(txvdev_aid_mgr);

		mlo_debug("AID mgr restored for vdev %d (txvdev %d)",
			  wlan_vdev_get_id(vdev), wlan_vdev_get_id(tx_vdev));
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_vdev_init_mbss_aid_mgr(struct wlan_objmgr_vdev *vdev,
					    struct wlan_objmgr_vdev *tx_vdev)
{
	struct wlan_vdev_aid_mgr *aid_mgr;
	struct wlan_vdev_aid_mgr *txvdev_aid_mgr;

	txvdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(tx_vdev);
	if (!txvdev_aid_mgr) {
		mlo_err("AID mgr of Tx VDEV%d is invalid",
			wlan_vdev_get_id(tx_vdev));
		return QDF_STATUS_E_FAILURE;
	}

	aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);

	qdf_atomic_inc(&txvdev_aid_mgr->ref_cnt);
	wlan_vdev_mlme_set_aid_mgr(vdev,
				   txvdev_aid_mgr);

	if (aid_mgr) {
		mlo_info("AID mgr is freed for vdev %d with txvdev %d",
			 wlan_vdev_get_id(vdev), wlan_vdev_get_id(tx_vdev));
		wlan_vdev_aid_mgr_free(aid_mgr);
	}

	mlo_debug("AID mgr replaced for vdev %d with txvdev %d",
		  wlan_vdev_get_id(vdev), wlan_vdev_get_id(tx_vdev));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_vdev_deinit_mbss_aid_mgr(struct wlan_objmgr_vdev *vdev,
					      struct wlan_objmgr_vdev *tx_vdev)
{
	struct wlan_vdev_aid_mgr *aid_mgr;
	struct wlan_vdev_aid_mgr *txvdev_aid_mgr;

	txvdev_aid_mgr = wlan_vdev_mlme_get_aid_mgr(tx_vdev);
	if (!txvdev_aid_mgr) {
		mlo_err("AID mgr of Tx VDEV%d is invalid",
			wlan_vdev_get_id(tx_vdev));
		return QDF_STATUS_E_FAILURE;
	}

	aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
	if (!aid_mgr) {
		mlo_err("AID mgr of VDEV%d is invalid",
			wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	if (aid_mgr != txvdev_aid_mgr) {
		mlo_err("AID mgr of VDEV%d and tx vdev(%d) aid mgr doesn't match",
			wlan_vdev_get_id(vdev), wlan_vdev_get_id(tx_vdev));
		return QDF_STATUS_E_FAILURE;
	}

	aid_mgr = wlan_vdev_aid_mgr_init(txvdev_aid_mgr->max_aid);
	if (!aid_mgr) {
		mlo_err("AID bitmap allocation failed for VDEV%d",
			wlan_vdev_get_id(vdev));
		QDF_BUG(0);
		return QDF_STATUS_E_NOMEM;
	}

	wlan_vdev_mlme_set_aid_mgr(vdev, aid_mgr);

	wlan_vdev_aid_mgr_free(txvdev_aid_mgr);

	mlo_debug("AID mgr restored for vdev %d (txvdev %d)",
		  wlan_vdev_get_id(vdev), wlan_vdev_get_id(tx_vdev));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_vdev_alloc_aid_mgr(struct wlan_mlo_dev_context *ml_dev,
				       struct wlan_objmgr_vdev *vdev)
{
	uint8_t i;
	struct wlan_objmgr_vdev *vdev_iter;
	struct wlan_ml_vdev_aid_mgr *ml_aidmgr;
	struct wlan_vdev_aid_mgr *aid_mgr = NULL;
	uint16_t max_aid = WLAN_UMAC_MAX_AID;

	if (!ml_dev->ap_ctx) {
		mlo_err(" ML AP context is not initialized");
		QDF_BUG(0);
		return QDF_STATUS_E_NOMEM;
	}
	ml_aidmgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aidmgr) {
		mlo_err(" ML AID mgr allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_iter = ml_dev->wlan_vdev_list[i];
		if (!vdev_iter)
			continue;

		if (vdev != vdev_iter)
			continue;

		/* if it is already allocated, assign it to ML AID mgr */
		aid_mgr = wlan_vdev_mlme_get_aid_mgr(vdev);
		if (!aid_mgr) {
			aid_mgr = wlan_vdev_aid_mgr_init(max_aid);
			if (aid_mgr) {
				wlan_vdev_mlme_set_aid_mgr(vdev, aid_mgr);
			} else {
				mlo_err("AID bitmap allocation failed for VDEV%d",
					wlan_vdev_get_id(vdev));
				return QDF_STATUS_E_NOMEM;
			}
		}

		ml_aidmgr->aid_mgr[i] = aid_mgr;
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_vdev_free_aid_mgr(struct wlan_mlo_dev_context *ml_dev,
				      struct wlan_objmgr_vdev *vdev)
{
	uint8_t i;
	struct wlan_objmgr_vdev *vdev_iter;
	struct wlan_ml_vdev_aid_mgr *ml_aidmgr;

	if (!ml_dev->ap_ctx) {
		mlo_err(" ML AP context is not initialized");
		QDF_BUG(0);
		return QDF_STATUS_E_NOMEM;
	}
	ml_aidmgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aidmgr) {
		mlo_err(" ML AID mgr allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_iter = ml_dev->wlan_vdev_list[i];
		if (!vdev_iter)
			continue;

		if (vdev != vdev_iter)
			continue;

		wlan_vdev_aid_mgr_free(ml_aidmgr->aid_mgr[i]);
		ml_aidmgr->aid_mgr[i] = NULL;
		wlan_vdev_mlme_set_aid_mgr(vdev, NULL);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_vdev_aid_mgr_init(struct wlan_mlo_dev_context *ml_dev)
{
	uint8_t i;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_ml_vdev_aid_mgr *ml_aidmgr;
	uint16_t max_aid = WLAN_UMAC_MAX_AID;

	ml_aidmgr = qdf_mem_malloc(sizeof(struct wlan_ml_vdev_aid_mgr));
	if (!ml_aidmgr) {
		ml_dev->ap_ctx->ml_aid_mgr = NULL;
		mlo_err(" ML AID mgr allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	ml_aidmgr->start_aid = 0;
	ml_aidmgr->max_aid = max_aid;
	ml_dev->ap_ctx->ml_aid_mgr = ml_aidmgr;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev = ml_dev->wlan_vdev_list[i];
		if (!vdev)
			continue;

		ml_aidmgr->aid_mgr[i] = wlan_vdev_aid_mgr_init(max_aid);
		if (!ml_aidmgr->aid_mgr[i]) {
			mlo_err("AID bitmap allocation failed for VDEV%d",
				wlan_vdev_get_id(vdev));
			goto free_ml_aid_mgr;
		}
		wlan_vdev_mlme_set_aid_mgr(vdev, ml_aidmgr->aid_mgr[i]);
	}

	return QDF_STATUS_SUCCESS;

free_ml_aid_mgr:
	wlan_mlo_vdev_aid_mgr_deinit(ml_dev);

	return QDF_STATUS_E_NOMEM;
}

void wlan_mlo_vdev_aid_mgr_deinit(struct wlan_mlo_dev_context *ml_dev)
{
	uint8_t i;
	struct wlan_ml_vdev_aid_mgr *ml_aid_mgr;
	int32_t n;

	ml_aid_mgr = ml_dev->ap_ctx->ml_aid_mgr;
	if (!ml_aid_mgr)
		return;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {

		if (ml_aid_mgr->aid_mgr[i]) {
			n = qdf_atomic_read(&ml_aid_mgr->aid_mgr[i]->ref_cnt);
			mlo_info("AID mgr ref cnt %d", n);
		} else {
			mlo_err("ID %d, doesn't have associated AID mgr", i);
			continue;
		}
		wlan_vdev_aid_mgr_free(ml_aid_mgr->aid_mgr[i]);
		ml_aid_mgr->aid_mgr[i] = NULL;
	}

	qdf_mem_free(ml_aid_mgr);
	ml_dev->ap_ctx->ml_aid_mgr = NULL;
}
