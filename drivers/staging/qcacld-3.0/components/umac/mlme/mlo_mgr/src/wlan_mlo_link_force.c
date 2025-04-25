/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains ML STA link force active/inactive related functionality
 */
#include "wlan_mlo_link_force.h"
#include "wlan_mlo_mgr_sta.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_cm_roam_public_struct.h"
#include "wlan_cm_roam_api.h"
#include "wlan_mlo_mgr_roam.h"
#include "wlan_mlme_main.h"
#include "wlan_mlo_mgr_link_switch.h"
#include "target_if.h"

void
ml_nlink_convert_linkid_bitmap_to_vdev_bitmap(
			struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_vdev *vdev,
			uint32_t link_bitmap,
			uint32_t *associated_bitmap,
			uint32_t *vdev_id_bitmap_sz,
			uint32_t vdev_id_bitmap[MLO_VDEV_BITMAP_SZ],
			uint8_t *vdev_id_num,
			uint8_t vdev_ids[WLAN_MLO_MAX_VDEVS])
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;
	uint8_t i, j, bitmap_sz = 0, num_vdev = 0;
	uint16_t link_id;
	uint8_t vdev_id;
	uint32_t associated_link_bitmap = 0;
	uint8_t vdev_per_bitmap = MLO_MAX_VDEV_COUNT_PER_BIMTAP_ELEMENT;

	*vdev_id_bitmap_sz = 0;
	*vdev_id_num = 0;
	qdf_mem_zero(vdev_id_bitmap,
		     sizeof(vdev_id_bitmap[0]) * MLO_VDEV_BITMAP_SZ);
	qdf_mem_zero(vdev_ids,
		     sizeof(vdev_ids[0]) * WLAN_MLO_MAX_VDEVS);
	if (associated_bitmap)
		*associated_bitmap = 0;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		/*todo: add standby link */
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;
		vdev_id = wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]);
		if (!qdf_test_bit(i, sta_ctx->wlan_connected_links)) {
			mlo_debug("vdev %d is not connected", vdev_id);
			continue;
		}

		link_id = wlan_vdev_get_link_id(
					mlo_dev_ctx->wlan_vdev_list[i]);
		if (link_id >= MAX_MLO_LINK_ID) {
			mlo_err("invalid link id %d", link_id);
			continue;
		}
		associated_link_bitmap |= 1 << link_id;
		/* If the link_id is not interested one which is specified
		 * in "link_bitmap", continue the search.
		 */
		if (!(link_bitmap & (1 << link_id)))
			continue;
		j = vdev_id / vdev_per_bitmap;
		if (j >= MLO_VDEV_BITMAP_SZ)
			break;
		vdev_id_bitmap[j] |= 1 << (vdev_id % vdev_per_bitmap);
		if (j + 1 > bitmap_sz)
			bitmap_sz = j + 1;

		if (num_vdev >= WLAN_MLO_MAX_VDEVS)
			break;
		vdev_ids[num_vdev++] = vdev_id;
	}
	mlo_dev_lock_release(mlo_dev_ctx);

	*vdev_id_bitmap_sz = bitmap_sz;
	*vdev_id_num = num_vdev;
	if (associated_bitmap)
		*associated_bitmap = associated_link_bitmap;

	mlo_debug("vdev %d link bitmap 0x%x vdev_bitmap 0x%x sz %d num %d assoc 0x%x for bitmap 0x%x",
		  wlan_vdev_get_id(vdev), link_bitmap & associated_link_bitmap,
		  vdev_id_bitmap[0], *vdev_id_bitmap_sz, num_vdev,
		  associated_link_bitmap, link_bitmap);
}

void
ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				uint32_t vdev_id_bitmap_sz,
				uint32_t *vdev_id_bitmap,
				uint32_t *link_bitmap,
				uint32_t *associated_bitmap)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;
	uint8_t i, j;
	uint16_t link_id;
	uint8_t vdev_id;
	uint32_t associated_link_bitmap = 0;
	uint8_t vdev_per_bitmap = MLO_MAX_VDEV_COUNT_PER_BIMTAP_ELEMENT;

	*link_bitmap = 0;
	if (associated_bitmap)
		*associated_bitmap = 0;
	if (!vdev_id_bitmap_sz) {
		mlo_debug("vdev_id_bitmap_sz 0");
		return;
	}

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	sta_ctx = mlo_dev_ctx->sta_ctx;
	mlo_dev_lock_acquire(mlo_dev_ctx);
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;
		vdev_id = wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]);
		if (!qdf_test_bit(i, sta_ctx->wlan_connected_links)) {
			mlo_debug("vdev %d is not connected", vdev_id);
			continue;
		}

		link_id = wlan_vdev_get_link_id(
					mlo_dev_ctx->wlan_vdev_list[i]);
		if (link_id >= MAX_MLO_LINK_ID) {
			mlo_err("invalid link id %d", link_id);
			continue;
		}
		associated_link_bitmap |= 1 << link_id;
		j = vdev_id / vdev_per_bitmap;
		if (j >= vdev_id_bitmap_sz) {
			mlo_err("invalid vdev id %d", vdev_id);
			continue;
		}
		/* If the vdev_id is not interested one which is specified
		 * in "vdev_id_bitmap", continue the search.
		 */
		if (!(vdev_id_bitmap[j] & (1 << (vdev_id % vdev_per_bitmap))))
			continue;

		*link_bitmap |= 1 << link_id;
	}
	mlo_dev_lock_release(mlo_dev_ctx);

	if (associated_bitmap)
		*associated_bitmap = associated_link_bitmap;
	mlo_debug("vdev %d link bitmap 0x%x vdev_bitmap 0x%x sz %d assoc 0x%x",
		  wlan_vdev_get_id(vdev), *link_bitmap, vdev_id_bitmap[0],
		  vdev_id_bitmap_sz, associated_link_bitmap);
}

void
ml_nlink_get_curr_force_state(struct wlan_objmgr_psoc *psoc,
			      struct wlan_objmgr_vdev *vdev,
			      struct ml_link_force_state *force_cmd)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	qdf_mem_copy(force_cmd,
		     &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state,
		     sizeof(*force_cmd));
	mlo_dev_lock_release(mlo_dev_ctx);
}

void
ml_nlink_clr_force_state(struct wlan_objmgr_psoc *psoc,
			 struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx)
		return;

	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	qdf_mem_zero(force_state, sizeof(*force_state));
	ml_nlink_dump_force_state(force_state, "");
	qdf_mem_zero(mlo_dev_ctx->sta_ctx->link_force_ctx.reqs,
		     sizeof(mlo_dev_ctx->sta_ctx->link_force_ctx.reqs));
	mlo_dev_lock_release(mlo_dev_ctx);
}

static void
ml_nlink_update_link_bitmap(uint16_t *curr_link_bitmap,
			    uint16_t link_bitmap,
			    enum set_curr_control ctrl)
{
	switch (ctrl) {
	case LINK_OVERWRITE:
		*curr_link_bitmap = link_bitmap;
		break;
	case LINK_CLR:
		*curr_link_bitmap &= ~link_bitmap;
		break;
	case LINK_ADD:
		*curr_link_bitmap |= link_bitmap;
		break;
	default:
		mlo_err("unknown update ctrl %d", ctrl);
		return;
	}
}

void
ml_nlink_set_curr_force_active_state(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_vdev *vdev,
				     uint16_t link_bitmap,
				     enum set_curr_control ctrl)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	ml_nlink_update_link_bitmap(&force_state->force_active_bitmap,
				    link_bitmap, ctrl);
	ml_nlink_dump_force_state(force_state, ":ctrl %d bitmap 0x%x",
				  ctrl, link_bitmap);
	mlo_dev_lock_release(mlo_dev_ctx);
}

void
ml_nlink_set_curr_force_inactive_state(struct wlan_objmgr_psoc *psoc,
				       struct wlan_objmgr_vdev *vdev,
				       uint16_t link_bitmap,
				       enum set_curr_control ctrl)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	ml_nlink_update_link_bitmap(&force_state->force_inactive_bitmap,
				    link_bitmap, ctrl);
	ml_nlink_dump_force_state(force_state, ":ctrl %d bitmap 0x%x", ctrl,
				  link_bitmap);
	mlo_dev_lock_release(mlo_dev_ctx);
}

void
ml_nlink_set_curr_force_active_num_state(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_vdev *vdev,
					 uint8_t link_num,
					 uint16_t link_bitmap)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	force_state->force_active_num = link_num;
	force_state->force_active_num_bitmap = link_bitmap;
	ml_nlink_dump_force_state(force_state, ":num %d bitmap 0x%x",
				  link_num, link_bitmap);
	mlo_dev_lock_release(mlo_dev_ctx);
}

void
ml_nlink_set_curr_force_inactive_num_state(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev,
					   uint8_t link_num,
					   uint16_t link_bitmap)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	force_state->force_inactive_num = link_num;
	force_state->force_inactive_num_bitmap = link_bitmap;
	ml_nlink_dump_force_state(force_state, ":num %d bitmap 0x%x",
				  link_num, link_bitmap);
	mlo_dev_lock_release(mlo_dev_ctx);
}

void
ml_nlink_set_dynamic_inactive_links(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    uint16_t dynamic_link_bitmap)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	force_state->curr_dynamic_inactive_bitmap = dynamic_link_bitmap;
	ml_nlink_dump_force_state(force_state, ":dynamic bitmap 0x%x",
				  dynamic_link_bitmap);
	mlo_dev_lock_release(mlo_dev_ctx);
}

static void
ml_nlink_update_force_link_request(struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_vdev *vdev,
				   struct set_link_req *req,
				   enum set_link_source source)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct set_link_req *old;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}
	if (source >= SET_LINK_SOURCE_MAX || source < 0) {
		mlo_err("invalid source %d", source);
		return;
	}
	mlo_dev_lock_acquire(mlo_dev_ctx);
	old = &mlo_dev_ctx->sta_ctx->link_force_ctx.reqs[source];
	*old = *req;
	mlo_dev_lock_release(mlo_dev_ctx);
}

static void
ml_nlink_update_concurrency_link_request(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct ml_link_force_state *force_state,
				enum mlo_link_force_reason reason)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct set_link_req *req;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}
	mlo_dev_lock_acquire(mlo_dev_ctx);
	req =
	&mlo_dev_ctx->sta_ctx->link_force_ctx.reqs[SET_LINK_FROM_CONCURRENCY];
	req->reason = reason;
	req->force_active_bitmap = force_state->force_active_bitmap;
	req->force_inactive_bitmap = force_state->force_inactive_bitmap;
	req->force_active_num = force_state->force_active_num;
	req->force_inactive_num = force_state->force_inactive_num;
	req->force_inactive_num_bitmap =
		force_state->force_inactive_num_bitmap;
	mlo_dev_lock_release(mlo_dev_ctx);
}

void ml_nlink_init_concurrency_link_request(
	struct wlan_objmgr_psoc *psoc,
	struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct set_link_req *req;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}
	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	req =
	&mlo_dev_ctx->sta_ctx->link_force_ctx.reqs[SET_LINK_FROM_CONCURRENCY];
	req->reason = MLO_LINK_FORCE_REASON_CONNECT;
	req->force_active_bitmap = force_state->force_active_bitmap;
	req->force_inactive_bitmap = force_state->force_inactive_bitmap;
	req->force_active_num = force_state->force_active_num;
	req->force_inactive_num = force_state->force_inactive_num;
	req->force_inactive_num_bitmap =
		force_state->force_inactive_num_bitmap;
	mlo_dev_lock_release(mlo_dev_ctx);
}

void
ml_nlink_get_force_link_request(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct set_link_req *req,
				enum set_link_source source)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct set_link_req *old;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}
	if (source >= SET_LINK_SOURCE_MAX || source < 0) {
		mlo_err("invalid source %d", source);
		return;
	}
	mlo_dev_lock_acquire(mlo_dev_ctx);
	old = &mlo_dev_ctx->sta_ctx->link_force_ctx.reqs[source];
	*req = *old;
	mlo_dev_lock_release(mlo_dev_ctx);
}

static void
ml_nlink_clr_force_link_request(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				enum set_link_source source)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct set_link_req *req;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}
	if (source >= SET_LINK_SOURCE_MAX || source < 0) {
		mlo_err("invalid source %d", source);
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	req = &mlo_dev_ctx->sta_ctx->link_force_ctx.reqs[source];
	qdf_mem_zero(req, sizeof(*req));
	mlo_dev_lock_release(mlo_dev_ctx);
}

void
ml_nlink_get_dynamic_inactive_links(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    uint16_t *dynamic_link_bitmap,
				    uint16_t *force_link_bitmap)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct ml_link_force_state *force_state;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	force_state = &mlo_dev_ctx->sta_ctx->link_force_ctx.force_state;
	*dynamic_link_bitmap = force_state->curr_dynamic_inactive_bitmap;
	*force_link_bitmap = force_state->force_inactive_bitmap;
	mlo_dev_lock_release(mlo_dev_ctx);
}

/**
 * ml_nlink_get_affect_ml_sta() - Get ML STA whose link can be
 * force inactive
 * @psoc: PSOC object information
 *
 * At present we only support one ML STA. so ml_nlink_get_affect_ml_sta
 * is invoked to get one ML STA vdev from policy mgr table.
 * In future if ML STA+ML STA supported, we may need to extend it
 * to find one ML STA which is required to force inactve/active.
 *
 * Return: vdev object
 */
static struct wlan_objmgr_vdev *
ml_nlink_get_affect_ml_sta(struct wlan_objmgr_psoc *psoc)
{
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct wlan_objmgr_vdev *vdev;

	policy_mgr_get_ml_sta_info_psoc(psoc, &num_ml_sta,
					&num_disabled_ml_sta,
					ml_sta_vdev_lst, ml_freq_lst, NULL,
					NULL, NULL);
	if (!num_ml_sta || num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS)
		return NULL;

	if (num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS) {
		mlo_debug("unexpected num_ml_sta %d", num_ml_sta);
		return NULL;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						psoc, ml_sta_vdev_lst[0],
						WLAN_MLO_MGR_ID);
	if (!vdev) {
		mlo_err("invalid vdev for id %d",  ml_sta_vdev_lst[0]);
		return NULL;
	}

	return vdev;
}

bool ml_is_nlink_service_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mlo_err("Invalid WMI handle");
		return false;
	}
	return wmi_service_enabled(
			wmi_handle,
			wmi_service_n_link_mlo_support);
}

/* Exclude AP removed link */
#define NLINK_EXCLUDE_REMOVED_LINK      0x01
/* Include AP removed link only, can't work with other flags */
#define NLINK_INCLUDE_REMOVED_LINK_ONLY 0x02
/* Exclude QUITE link */
#define NLINK_EXCLUDE_QUIET_LINK        0x04
/* Exclude standby link information */
#define NLINK_EXCLUDE_STANDBY_LINK      0x08
/* Dump link information */
#define NLINK_DUMP_LINK                 0x10

static void
ml_nlink_get_standby_link_info(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_vdev *vdev,
			       uint8_t flag,
			       uint8_t ml_num_link_sz,
			       struct ml_link_info *ml_link_info,
			       qdf_freq_t *ml_freq_lst,
			       uint8_t *ml_vdev_lst,
			       uint8_t *ml_linkid_lst,
			       uint8_t *ml_num_link,
			       uint32_t *ml_link_bitmap)
{
	struct mlo_link_info *link_info;
	uint8_t link_info_iter;

	link_info = mlo_mgr_get_ap_link(vdev);
	if (!link_info)
		return;

	for (link_info_iter = 0; link_info_iter < WLAN_MAX_ML_BSS_LINKS;
	     link_info_iter++) {
		if (qdf_is_macaddr_zero(&link_info->ap_link_addr))
			break;

		if (link_info->vdev_id == WLAN_INVALID_VDEV_ID) {
			if (*ml_num_link >= ml_num_link_sz) {
				mlo_debug("link lst overflow");
				break;
			}
			if (!link_info->link_chan_info->ch_freq) {
				mlo_debug("link freq 0!");
				break;
			}
			if (*ml_link_bitmap & (1 << link_info->link_id)) {
				mlo_debug("unexpected standby linkid %d",
					  link_info->link_id);
				break;
			}
			if (link_info->link_id >= MAX_MLO_LINK_ID) {
				mlo_debug("invalid standby link id %d",
					  link_info->link_id);
				break;
			}

			if ((flag & NLINK_EXCLUDE_REMOVED_LINK) &&
			    qdf_atomic_test_bit(
					LS_F_AP_REMOVAL_BIT,
					&link_info->link_status_flags)) {
				mlo_debug("standby link %d is removed",
					  link_info->link_id);
				continue;
			}
			if ((flag & NLINK_INCLUDE_REMOVED_LINK_ONLY) &&
			    !qdf_atomic_test_bit(
					LS_F_AP_REMOVAL_BIT,
					&link_info->link_status_flags)) {
				continue;
			}

			ml_freq_lst[*ml_num_link] =
				link_info->link_chan_info->ch_freq;
			ml_vdev_lst[*ml_num_link] = WLAN_INVALID_VDEV_ID;
			ml_linkid_lst[*ml_num_link] = link_info->link_id;
			*ml_link_bitmap |= 1 << link_info->link_id;
			if (flag & NLINK_DUMP_LINK)
				mlo_debug("vdev %d link %d freq %d bitmap 0x%x flag 0x%x",
					  ml_vdev_lst[*ml_num_link],
					  ml_linkid_lst[*ml_num_link],
					  ml_freq_lst[*ml_num_link],
					  *ml_link_bitmap, flag);
			(*ml_num_link)++;
		}

		link_info++;
	}
}

uint32_t
ml_nlink_get_standby_link_bitmap(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev)
{
	uint8_t ml_num_link = 0;
	uint32_t standby_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];

	ml_nlink_get_standby_link_info(psoc, vdev, NLINK_DUMP_LINK,
				       QDF_ARRAY_SIZE(ml_linkid_lst),
				       ml_link_info, ml_freq_lst, ml_vdev_lst,
				       ml_linkid_lst, &ml_num_link,
				       &standby_link_bitmap);

	return standby_link_bitmap;
}

/**
 * ml_nlink_get_link_info() - Get ML STA link info
 * @psoc: PSOC object information
 * @vdev: ml sta vdev object
 * @flag: flag NLINK_* to specify what links should be returned
 * @ml_num_link_sz: input array size of ml_link_info and
 * other parameters.
 * @ml_link_info: ml link info array
 * @ml_freq_lst: channel frequency list
 * @ml_vdev_lst: vdev id list
 * @ml_linkid_lst: link id list
 * @ml_num_link: num of links
 * @ml_link_bitmap: link bitmaps.
 *
 * Return: void
 */
static void ml_nlink_get_link_info(struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_vdev *vdev,
				   uint8_t flag,
				   uint8_t ml_num_link_sz,
				   struct ml_link_info *ml_link_info,
				   qdf_freq_t *ml_freq_lst,
				   uint8_t *ml_vdev_lst,
				   uint8_t *ml_linkid_lst,
				   uint8_t *ml_num_link,
				   uint32_t *ml_link_bitmap)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;
	uint8_t i, num_link = 0;
	uint32_t link_bitmap = 0;
	uint16_t link_id;
	uint8_t vdev_id;
	bool connected = false;

	*ml_num_link = 0;
	*ml_link_bitmap = 0;
	qdf_mem_zero(ml_link_info, sizeof(*ml_link_info) * ml_num_link_sz);
	qdf_mem_zero(ml_freq_lst, sizeof(*ml_freq_lst) * ml_num_link_sz);
	qdf_mem_zero(ml_linkid_lst, sizeof(*ml_linkid_lst) * ml_num_link_sz);
	qdf_mem_zero(ml_vdev_lst, sizeof(*ml_vdev_lst) * ml_num_link_sz);

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo_ctx or sta_ctx null");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;

	link_bitmap = 0;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;
		if (!qdf_test_bit(i, sta_ctx->wlan_connected_links))
			continue;

		if (!wlan_cm_is_vdev_connected(
				mlo_dev_ctx->wlan_vdev_list[i])) {
			mlo_debug("Vdev id %d is not in connected state",
				  wlan_vdev_get_id(
					mlo_dev_ctx->wlan_vdev_list[i]));
			continue;
		}
		connected = true;

		vdev_id = wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]);
		link_id = wlan_vdev_get_link_id(
					mlo_dev_ctx->wlan_vdev_list[i]);
		if (link_id >= MAX_MLO_LINK_ID) {
			mlo_debug("invalid link id %x for vdev %d",
				  link_id, vdev_id);
			continue;
		}

		if ((flag & NLINK_EXCLUDE_REMOVED_LINK) &&
		    wlan_get_vdev_link_removed_flag_by_vdev_id(
						psoc, vdev_id)) {
			mlo_debug("vdev id %d link %d is removed",
				  vdev_id, link_id);
			continue;
		}
		if ((flag & NLINK_INCLUDE_REMOVED_LINK_ONLY) &&
		    !wlan_get_vdev_link_removed_flag_by_vdev_id(
						psoc, vdev_id)) {
			continue;
		}
		if ((flag & NLINK_EXCLUDE_QUIET_LINK) &&
		    mlo_is_sta_in_quiet_status(mlo_dev_ctx, link_id)) {
			mlo_debug("vdev id %d link %d is quiet",
				  vdev_id, link_id);
			continue;
		}

		if (num_link >= ml_num_link_sz)
			break;
		ml_freq_lst[num_link] = wlan_get_operation_chan_freq(
					mlo_dev_ctx->wlan_vdev_list[i]);
		ml_vdev_lst[num_link] = vdev_id;
		ml_linkid_lst[num_link] = link_id;
		link_bitmap |= 1 << link_id;
		if (flag & NLINK_DUMP_LINK)
			mlo_debug("vdev %d link %d freq %d bitmap 0x%x flag 0x%x",
				  ml_vdev_lst[num_link],
				  ml_linkid_lst[num_link],
				  ml_freq_lst[num_link], link_bitmap, flag);
		num_link++;
	}
	/* Add standby link only if mlo sta is connected */
	if (connected && !(flag & NLINK_EXCLUDE_STANDBY_LINK))
		ml_nlink_get_standby_link_info(psoc, vdev, flag,
					       ml_num_link_sz,
					       ml_link_info,
					       ml_freq_lst,
					       ml_vdev_lst,
					       ml_linkid_lst,
					       &num_link,
					       &link_bitmap);

	mlo_dev_lock_release(mlo_dev_ctx);
	*ml_num_link = num_link;
	*ml_link_bitmap = link_bitmap;
}

uint32_t
convert_link_bitmap_to_link_ids(uint32_t link_bitmap,
				uint8_t link_id_sz,
				uint8_t *link_ids)
{
	uint32_t i = 0;
	uint8_t id = 0;

	while (link_bitmap) {
		if (link_bitmap & 1) {
			if (id >= 15) {
				/* warning */
				mlo_err("linkid invalid %d 0x%x",
					id, link_bitmap);
				break;
			}
			if (link_ids) {
				if (i >= link_id_sz) {
					/* warning */
					mlo_err("linkid buff overflow 0x%x",
						link_bitmap);
					break;
				}
				link_ids[i] = id;
			}
			i++;
		}
		link_bitmap >>= 1;
		id++;
	}

	return i;
}

uint32_t
ml_nlink_convert_link_bitmap_to_ids(uint32_t link_bitmap,
				    uint8_t link_id_sz,
				    uint8_t *link_ids)
{
	return convert_link_bitmap_to_link_ids(link_bitmap, link_id_sz,
					       link_ids);
}

/**
 * ml_nlink_handle_mcc_links() - Check force inactive needed
 * if ML STA links are in MCC channels
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @force_cmd: force command to be returned
 *
 * This API will return force inactive number 1 in force_cmd
 * if STA links are in MCC channels with the link bitmap including
 * the MCC links id.
 * If the link is marked removed by AP MLD, return force inactive
 * bitmap with removed link id bitmap as well.
 *
 * Return: void
 */
static void
ml_nlink_handle_mcc_links(struct wlan_objmgr_psoc *psoc,
			  struct wlan_objmgr_vdev *vdev,
			  struct ml_link_force_state *force_cmd)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0, affected_link_bitmap = 0;
	uint32_t force_inactive_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];

	ml_nlink_get_link_info(psoc, vdev, NLINK_INCLUDE_REMOVED_LINK_ONLY |
						NLINK_DUMP_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &force_inactive_link_bitmap);
	if (force_inactive_link_bitmap) {
		/* AP removed link will be force inactive always */
		force_cmd->force_inactive_bitmap = force_inactive_link_bitmap;
		mlo_debug("AP removed link 0x%x", force_inactive_link_bitmap);
	}

	ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_REMOVED_LINK |
						NLINK_DUMP_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	if (ml_num_link < 2)
		return;

	policy_mgr_is_ml_sta_links_in_mcc(psoc, ml_freq_lst,
					  ml_vdev_lst,
					  ml_linkid_lst,
					  ml_num_link,
					  &affected_link_bitmap);
	if (affected_link_bitmap) {
		force_cmd->force_inactive_num =
			convert_link_bitmap_to_link_ids(
				affected_link_bitmap, 0, NULL);
		if (force_cmd->force_inactive_num > 1) {
			force_cmd->force_inactive_num--;
			force_cmd->force_inactive_num_bitmap =
						affected_link_bitmap;
		} else {
			force_cmd->force_inactive_num = 0;
		}
	}
	if (force_inactive_link_bitmap || affected_link_bitmap)
		ml_nlink_dump_force_state(force_cmd, "");
}

/**
 * ml_nlink_handle_legacy_sta_intf() - Check force inactive needed
 * with legacy STA
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @force_cmd: force command to be returned
 * @sta_vdev_id: legacy STA vdev id
 * @non_ml_sta_freq: legacy STA channel frequency
 *
 * If legacy STA is MCC with any link of MLO STA, the mlo link
 * will be forced inactive. And if 3 link MLO case, the left
 * 2 links have to be force inactive with num 1. For example,
 * ML STA 2+5+6, legacy STA on MCC channel of 5G link, then
 * 5G will be force inactive, and left 2+6 link will be force
 * inactive by inactive link num = 1 (with link bitmap 2+6).
 *
 * Return: void
 */
static void
ml_nlink_handle_legacy_sta_intf(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct ml_link_force_state *force_cmd,
				uint8_t sta_vdev_id,
				qdf_freq_t non_ml_sta_freq)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0, affected_link_bitmap = 0;
	uint32_t force_inactive_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t i = 0;
	uint32_t scc_link_bitmap = 0;

	ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_REMOVED_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	if (ml_num_link < 2)
		return;

	for (i = 0; i < ml_num_link; i++) {
		/*todo add removed link to force_inactive_link_bitmap*/
		if (ml_freq_lst[i] == non_ml_sta_freq) {
			scc_link_bitmap = 1 << ml_linkid_lst[i];
		} else if (policy_mgr_2_freq_always_on_same_mac(
				psoc, ml_freq_lst[i], non_ml_sta_freq)) {
			force_inactive_link_bitmap |= 1 << ml_linkid_lst[i];
		} else if (!wlan_cm_same_band_sta_allowed(psoc) &&
			   (wlan_reg_is_24ghz_ch_freq(ml_freq_lst[i]) ==
			    wlan_reg_is_24ghz_ch_freq(non_ml_sta_freq)) &&
			   !policy_mgr_are_sbs_chan(psoc, ml_freq_lst[i],
						     non_ml_sta_freq)) {
			force_inactive_link_bitmap |= 1 << ml_linkid_lst[i];
		}
	}

	/* If no left active link, don't send the force inactive command for
	 * concurrency purpose.
	 */
	if (!(ml_link_bitmap & ~force_inactive_link_bitmap)) {
		mlo_debug("unexpected ML conc with legacy STA freq %d",
			  non_ml_sta_freq);
		return;
	}

	if (force_inactive_link_bitmap) {
		/* for example SBS rd, ML 2G+5G high, Legacy intf on 5G high,
		 * set force inactive with bitmap of 5g link.
		 *
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 2G.
		 * set force inactive with bitmap 2G link,
		 * and set force inactive link num to 1 for left 5g and 6g
		 * link.
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 5G low.
		 * set force inactive with bitmap 5G low link,
		 * and set force inactive link num to 1 for left 2g and 6g
		 * link.
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 5G high.
		 * set force inactive with bitmap 6G link,
		 * and set force inactive link num to 1 for left 2g and 5g
		 * link.
		 * In above 3 link cases, if legacy intf is SCC with ml link
		 * don't force inactive by bitmap, only send force inactive
		 * num with bitmap
		 */
		force_cmd->force_inactive_bitmap = force_inactive_link_bitmap;

		affected_link_bitmap =
			ml_link_bitmap & ~force_inactive_link_bitmap;
		affected_link_bitmap &= ~scc_link_bitmap;
		force_cmd->force_inactive_num =
			convert_link_bitmap_to_link_ids(
				affected_link_bitmap, 0, NULL);
		if (force_cmd->force_inactive_num > 1) {
			force_cmd->force_inactive_num--;
			force_cmd->force_inactive_num_bitmap =
						affected_link_bitmap;

		} else {
			force_cmd->force_inactive_num = 0;
		}
	} else {
		/* for example SBS rd, ML 2G+5G high, Legacy intf on 5G low,
		 * set force inactive num to 1 with bitmap of 2g+5g link.
		 *
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 5G low SCC.
		 * set force inactive link num to 1 for left 2g and 6g
		 * link.
		 */
		affected_link_bitmap = ml_link_bitmap;
		affected_link_bitmap &= ~scc_link_bitmap;

		force_cmd->force_inactive_num =
			convert_link_bitmap_to_link_ids(
				affected_link_bitmap, 0, NULL);
		if (force_cmd->force_inactive_num > 1) {
			force_cmd->force_inactive_num--;
			force_cmd->force_inactive_num_bitmap =
				affected_link_bitmap;
		} else {
			force_cmd->force_inactive_num = 0;
		}
	}
}

/**
 * ml_nlink_handle_legacy_sap_intf() - Check force inactive needed
 * with legacy SAP
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @force_cmd: force command to be returned
 * @sap_vdev_id: legacy SAP vdev id
 * @sap_freq: legacy SAP channel frequency
 *
 * If legacy SAP is 2g only SAP and MLO STA is 5+6,
 * 2 links have to be force inactive with num 1.
 *
 * Return: void
 */
static void
ml_nlink_handle_legacy_sap_intf(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct ml_link_force_state *force_cmd,
				uint8_t sap_vdev_id,
				qdf_freq_t sap_freq)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0, affected_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t i = 0;
	bool sap_2g_only = false;

	/* SAP MCC with MLO STA link is not preferred.
	 * If SAP is 2Ghz only by ACS and two ML link are
	 * 5/6 band, then force SCC may not happen. In such
	 * case inactive one link.
	 */
	if (policy_mgr_check_2ghz_only_sap_affected_link(
				psoc, sap_vdev_id, sap_freq,
				ml_num_link, ml_freq_lst)) {
		mlo_debug("2G only SAP vdev %d ch freq %d is not SCC with any MLO STA link",
			  sap_vdev_id, sap_freq);
		sap_2g_only = true;
	}
	/*
	 * If SAP is on 5G or 6G, SAP can always force SCC to 5G/6G ML STA or
	 * 2G ML STA, no need force SCC link.
	 */
	if (!sap_2g_only)
		return;

	ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_REMOVED_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	if (ml_num_link < 2)
		return;

	for (i = 0; i < ml_num_link; i++) {
		if (!wlan_reg_is_24ghz_ch_freq(ml_freq_lst[i]))
			affected_link_bitmap |= 1 << ml_linkid_lst[i];
	}

	if (affected_link_bitmap) {
		/* for SBS rd, ML 2G + 5G low, Legacy SAP on 2G.
		 * no force any link
		 * for SBS rd, ML 5G low + 5G high/6G, Legacy SAP on 2G.
		 * set force inactive num 1 with bitmap 5g and 6g.
		 *
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy SAP on 2G.
		 * set force inactive link num to 1 for 5g and 6g
		 * link.
		 */
		force_cmd->force_inactive_num =
			convert_link_bitmap_to_link_ids(
				affected_link_bitmap, 0, NULL);
		if (force_cmd->force_inactive_num > 1) {
			force_cmd->force_inactive_num--;
			force_cmd->force_inactive_num_bitmap =
					affected_link_bitmap;
		} else {
			force_cmd->force_inactive_num = 0;
		}
	}
}

/**
 * ml_nlink_handle_legacy_p2p_intf() - Check force inactive needed
 * with p2p
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @force_cmd: force command to be returned
 * @p2p_vdev_id: p2p vdev id
 * @p2p_freq: p2p channel frequency
 *
 * If P2P has low latency flag and MCC with any link of MLO STA, the mlo link
 * will be forced inactive. And if 3 link MLO case, the left 2 links have to
 * be force inactive with num 1.
 *
 * Return: void
 */
static void
ml_nlink_handle_legacy_p2p_intf(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct ml_link_force_state *force_cmd,
				uint8_t p2p_vdev_id,
				qdf_freq_t p2p_freq)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0, affected_link_bitmap = 0;
	uint32_t force_inactive_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t i = 0;
	uint32_t scc_link_bitmap = 0;

	/* If high tput or low latency is not set, mcc is allowed for p2p */
	if (!policy_mgr_is_vdev_high_tput_or_low_latency(
				psoc, p2p_vdev_id))
		return;
	ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_REMOVED_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	if (ml_num_link < 2)
		return;

	for (i = 0; i < ml_num_link; i++) {
		if (ml_freq_lst[i] == p2p_freq) {
			scc_link_bitmap = 1 << ml_linkid_lst[i];
		} else if (policy_mgr_2_freq_always_on_same_mac(
				psoc, ml_freq_lst[i], p2p_freq)) {
			force_inactive_link_bitmap |= 1 << ml_linkid_lst[i];
		}
	}
	/* If no left active link, don't send the force inactive command for
	 * concurrency purpose.
	 */
	if (!(ml_link_bitmap & ~force_inactive_link_bitmap)) {
		mlo_debug("unexpected ML conc with legacy P2P freq %d",
			  p2p_freq);
		return;
	}

	if (force_inactive_link_bitmap) {
		/* for example SBS rd, ML 2G+5G high, Legacy intf on 5G high,
		 * set force inactive with bitmap of 5g link.
		 *
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 2G.
		 * set force inactive with bitmap 2G link,
		 * and set force inactive link num to 1 for left 5g and 6g
		 * link.
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 5G low.
		 * set force inactive with bitmap 5G low link,
		 * and set force inactive link num to 1 for left 2g and 6g
		 * link.
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 5G high..
		 * set force inactive with bitmap 6G low link,
		 * and set force inactive link num to 1 for left 2g and 5g
		 * link.
		 */
		force_cmd->force_inactive_bitmap = force_inactive_link_bitmap;

		affected_link_bitmap =
			ml_link_bitmap & ~force_inactive_link_bitmap;
		affected_link_bitmap &= ~scc_link_bitmap;
		force_cmd->force_inactive_num =
			convert_link_bitmap_to_link_ids(
				affected_link_bitmap, 0, NULL);
		if (force_cmd->force_inactive_num > 1) {
			force_cmd->force_inactive_num--;
			force_cmd->force_inactive_num_bitmap =
					affected_link_bitmap;

		} else {
			force_cmd->force_inactive_num = 0;
		}
	} else {
		/* for example SBS rd, ML 2G+5G high, Legacy intf on 5G low,
		 * set force inactive num to 1 with bitmap of 2g+5g link.
		 *
		 * for SBS rd, ML 2G + 5G low + 6G, Legacy intf on 5G low SCC.
		 * set force inactive link num to 1 for left 2g and 6g
		 * link.
		 */
		affected_link_bitmap = ml_link_bitmap;
		affected_link_bitmap &= ~scc_link_bitmap;

		force_cmd->force_inactive_num =
			convert_link_bitmap_to_link_ids(
				affected_link_bitmap, 0, NULL);
		if (force_cmd->force_inactive_num > 1) {
			force_cmd->force_inactive_num--;
			force_cmd->force_inactive_num_bitmap =
					affected_link_bitmap;
		} else {
			force_cmd->force_inactive_num = 0;
		}
	}
}

/**
 * ml_nlink_handle_3_port_specific_scenario() - Check some specific corner
 * case that can't be handled general logic in
 * ml_nlink_handle_legacy_intf_3_ports.
 * @psoc: PSOC object information
 * @legacy_intf_freq1: legacy interface 1 channel frequency
 * @legacy_intf_freq2: legacy interface 2 channel frequency
 * @ml_num_link: number of ML STA links
 * @ml_freq_lst: ML STA link channel frequency list
 * @ml_linkid_lst: ML STA link ids
 *
 * Return: link force inactive bitmap
 */
static uint32_t
ml_nlink_handle_3_port_specific_scenario(struct wlan_objmgr_psoc *psoc,
					 qdf_freq_t legacy_intf_freq1,
					 qdf_freq_t legacy_intf_freq2,
					 uint8_t ml_num_link,
					 qdf_freq_t *ml_freq_lst,
					 uint8_t *ml_linkid_lst)
{
	uint32_t force_inactive_link_bitmap = 0;

	if (ml_num_link < 2)
		return 0;

	/* special case handling:
	 * LL P2P on 2.4G, ML STA 5G+6G, SAP on 6G, then
	 * inactive 5G link.
	 * LL P2P on 2.4G, ML STA 5G+6G, SAP on 5G, then
	 * inactive 6G link.
	 */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(legacy_intf_freq1) &&
	    !WLAN_REG_IS_24GHZ_CH_FREQ(ml_freq_lst[0]) &&
	    policy_mgr_are_sbs_chan(psoc, ml_freq_lst[0], ml_freq_lst[1]) &&
	    policy_mgr_2_freq_always_on_same_mac(psoc, ml_freq_lst[0],
						 legacy_intf_freq2))
		force_inactive_link_bitmap |= 1 << ml_linkid_lst[1];
	else if (WLAN_REG_IS_24GHZ_CH_FREQ(legacy_intf_freq1) &&
		 !WLAN_REG_IS_24GHZ_CH_FREQ(ml_freq_lst[1]) &&
		 policy_mgr_are_sbs_chan(psoc, ml_freq_lst[0],
					 ml_freq_lst[1]) &&
		 policy_mgr_2_freq_always_on_same_mac(psoc, ml_freq_lst[1],
						      legacy_intf_freq2))
		force_inactive_link_bitmap |= 1 << ml_linkid_lst[0];

	if (force_inactive_link_bitmap)
		mlo_debug("force inactive 0x%x", force_inactive_link_bitmap);

	return force_inactive_link_bitmap;
}

/**
 * ml_nlink_handle_legacy_intf_3_ports() - Check force inactive needed
 * with 2 legacy interfaces
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @force_cmd: force command to be returned
 * @legacy_intf_freq1: legacy interface frequency
 * @legacy_intf_freq2: legacy interface frequency
 *
 * If legacy interface 1 (which channel frequency legacy_intf_freq1) is
 * mcc with any link based on current hw mode, then force inactive the link.
 * And if standby link is mcc with legacy interface, then disable standby
 * link as well.
 * In 3 Port case, at present only legacy interface 1(which channel frequency
 * legacy_intf_freq1) MCC avoidance requirement can be met. The assignment of
 * legacy_intf_freq1 and legacy_intf_freq2 is based on priority of Port type,
 * check policy_mgr_get_legacy_conn_info for detail.
 * Cornor cases will be handled in ml_nlink_handle_3_port_specific_scenario.
 *
 * Return: void
 */
static void
ml_nlink_handle_legacy_intf_3_ports(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    struct ml_link_force_state *force_cmd,
				    qdf_freq_t legacy_intf_freq1,
				    qdf_freq_t legacy_intf_freq2)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0;
	uint32_t force_inactive_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t i = 0;
	uint32_t scc_link_bitmap = 0;

	ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_REMOVED_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	if (ml_num_link < 2)
		return;

	for (i = 0; i < ml_num_link; i++) {
		if (ml_vdev_lst[i] == WLAN_INVALID_VDEV_ID) {
			/*standby link will be handled later. */
			continue;
		}
		if (ml_freq_lst[i] == legacy_intf_freq1) {
			scc_link_bitmap = 1 << ml_linkid_lst[i];
			if (ml_freq_lst[i] == legacy_intf_freq2) {
				mlo_debug("3 vdev scc no-op");
				return;
			}
		} else if (policy_mgr_are_2_freq_on_same_mac(
				psoc, ml_freq_lst[i], legacy_intf_freq1)) {
			force_inactive_link_bitmap |= 1 << ml_linkid_lst[i];
		} else if (i == 1) {
			force_inactive_link_bitmap |=
			ml_nlink_handle_3_port_specific_scenario(
							psoc,
							legacy_intf_freq1,
							legacy_intf_freq2,
							ml_num_link,
							ml_freq_lst,
							ml_linkid_lst);
		}
	}
	/* usually it can't happen in 3 Port */
	if (!force_inactive_link_bitmap && !scc_link_bitmap) {
		mlo_debug("legacy vdev freq %d standalone on dedicated mac",
			  legacy_intf_freq1);
		return;
	}

	if (force_inactive_link_bitmap)
		force_cmd->force_inactive_bitmap = force_inactive_link_bitmap;
}

static void
ml_nlink_handle_standby_link_3_ports(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_vdev *vdev,
		struct ml_link_force_state *force_cmd,
		uint8_t num_legacy_vdev,
		uint8_t *vdev_lst,
		qdf_freq_t *freq_lst,
		enum policy_mgr_con_mode *mode_lst)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0;
	uint32_t force_inactive_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t i, j;

	if (num_legacy_vdev < 2)
		return;

	ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_REMOVED_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	if (ml_num_link < 2)
		return;
	for (i = 0; i < ml_num_link; i++) {
		if (ml_vdev_lst[i] != WLAN_INVALID_VDEV_ID)
			continue;
		/* standby link will be forced inactive if mcc with
		 * legacy interface
		 */
		for (j = 0; j < num_legacy_vdev; j++) {
			if (ml_freq_lst[i] != freq_lst[j] &&
			    policy_mgr_are_2_freq_on_same_mac(
					psoc, ml_freq_lst[i], freq_lst[j]))
				force_inactive_link_bitmap |=
						1 << ml_linkid_lst[i];
		}
	}

	if (force_inactive_link_bitmap)
		force_cmd->force_inactive_bitmap |= force_inactive_link_bitmap;
}

/**
 * ml_nlink_handle_legacy_intf() - Check force inactive needed
 * with legacy interface
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @force_cmd: force command to be returned
 *
 * Return: void
 */
static void
ml_nlink_handle_legacy_intf(struct wlan_objmgr_psoc *psoc,
			    struct wlan_objmgr_vdev *vdev,
			    struct ml_link_force_state *force_cmd)
{
	uint8_t vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	enum policy_mgr_con_mode mode_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t num_legacy_vdev;

	num_legacy_vdev = policy_mgr_get_legacy_conn_info(
					psoc, vdev_lst,
					freq_lst, mode_lst,
					QDF_ARRAY_SIZE(vdev_lst));
	if (!num_legacy_vdev)
		return;
	/* 2 port case with 2 ml sta links or
	 * 2 port case with 3 ml sta links
	 */
	if (num_legacy_vdev == 1) {
		switch (mode_lst[0]) {
		case PM_STA_MODE:
			ml_nlink_handle_legacy_sta_intf(
				psoc, vdev, force_cmd, vdev_lst[0],
				freq_lst[0]);
			break;
		case PM_SAP_MODE:
			ml_nlink_handle_legacy_sap_intf(
				psoc, vdev, force_cmd, vdev_lst[0],
				freq_lst[0]);
			break;
		case PM_P2P_CLIENT_MODE:
		case PM_P2P_GO_MODE:
			ml_nlink_handle_legacy_p2p_intf(
				psoc, vdev, force_cmd, vdev_lst[0],
				freq_lst[0]);
			break;
		default:
			/* unexpected legacy connection count */
			mlo_debug("unexpected legacy intf mode %d",
				  mode_lst[0]);
			return;
		}
		ml_nlink_dump_force_state(force_cmd, "");
		return;
	}
	/* 3 ports case with ml sta 2 or 3 links, suppose port 3 vdev is
	 * low latency legacy vdev:
	 * 6G: ML Link + Port2 + Port3 | 5G: ML Link
	 *	=> no op
	 * 6G: ML Link + Port2	       | 5G: ML Link + Port3
	 *	=> disable 5G link if 5G mcc
	 * 6G: ML Link + Port3	       | 5G: ML Link + Port2
	 *	=> disable 6G link if 6G mcc
	 * 6G: ML Link		       | 5G: ML Link + Port3 | 2G: Port2
	 *	=> disable 5G link if 5G mcc.
	 * 6G: ML Link		       | 5G: ML Link + Port2 | 2G: Port3
	 *	=> disable 6g link.
	 * 6G: ML Link + Port3	       | 5G: ML Link | 2G: Port2
	 *	=> disable 6G link if 6G mcc.
	 * 6G: ML Link + Port2	       | 5G: ML Link | 2G: Port3
	 *	=> disable 6g link.
	 * 6G: ML Link + Port2 + Port3 | 2G: ML Link
	 *	=> no op
	 * 6G: ML Link + Port2	       | 2G: ML Link + Port3
	 *	=> disable 2G link if 2G mcc
	 * 6G: ML Link + Port3	       | 2G: ML Link + Port2
	 *	=> disable 6G link if 6G mcc
	 * 6G: ML Link		       | 2G: ML Link + Port3 | 5G: Port2
	 *	=> disable 2G link if 2G mcc.
	 * 6G: ML Link		       | 2G: ML Link + Port2 | 5GL: Port3
	 *	=> disable 6G link
	 * 6G: ML Link + Port3	       | 2G: ML Link | 5G: Port2
	 *	=> disable 6G link if 6G mcc.
	 * 6G: ML Link + Port2	       | 2G: ML Link | 5GL: Port3
	 *	=> disable 2G link
	 * general rule:
	 * If Port3 is mcc with any link based on current hw mode, then
	 * force inactive the link.
	 * And if standby link is mcc with Port3, then disable standby
	 * link as well.
	 */
	switch (mode_lst[0]) {
	case PM_P2P_CLIENT_MODE:
	case PM_P2P_GO_MODE:
		if (!policy_mgr_is_vdev_high_tput_or_low_latency(
					psoc, vdev_lst[0]))
			break;
		fallthrough;
	case PM_STA_MODE:
		ml_nlink_handle_legacy_intf_3_ports(
			psoc, vdev, force_cmd, freq_lst[0], freq_lst[1]);
		break;
	case PM_SAP_MODE:
		/* if 2g only sap present, force inactive num to fw. */
		ml_nlink_handle_legacy_sap_intf(
			psoc, vdev, force_cmd, vdev_lst[0], freq_lst[0]);
		break;
	default:
		/* unexpected legacy connection count */
		mlo_debug("unexpected legacy intf mode %d", mode_lst[0]);
		return;
	}
	ml_nlink_handle_standby_link_3_ports(psoc, vdev, force_cmd,
					     num_legacy_vdev,
					     vdev_lst,
					     freq_lst,
					     mode_lst);
	ml_nlink_dump_force_state(force_cmd, "");
}

/**
 * ml_nlink_handle_dynamic_inactive() - Handle dynamic force inactive num
 * with legacy SAP
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @curr: current force command state
 * @new: new force command
 *
 * If ML STA 2 or 3 links are present and force inactive num = 1 with dynamic
 * flag enabled for some reason, FW will report the current inactive links,
 * host will select one and save to curr_dynamic_inactive_bitmap.
 * If SAP starting on channel which is same mac as links in
 * the curr_dynamic_inactive_bitmap, host will force inactive the links in
 * curr_dynamic_inactive_bitmap to avoid FW link switch between the dynamic
 * inactive links.
 *
 * Return: void
 */
static void
ml_nlink_handle_dynamic_inactive(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev,
				 struct ml_link_force_state *curr,
				 struct ml_link_force_state *new)
{
	uint8_t vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	enum policy_mgr_con_mode mode_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t num;
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap;
	uint32_t force_inactive_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t i, j;

	/* If force inactive num wasn't sent to fw, no need to handle
	 * dynamic inactive links.
	 */
	if (!curr->force_inactive_num ||
	    !curr->force_inactive_num_bitmap ||
	    !curr->curr_dynamic_inactive_bitmap)
		return;
	if (curr->force_inactive_num != new->force_inactive_num ||
	    curr->force_inactive_num_bitmap !=
				new->force_inactive_num_bitmap)
		return;
	/* If links have been forced inactive by bitmap, no need to force
	 * again.
	 */
	if ((new->force_inactive_bitmap &
	     curr->curr_dynamic_inactive_bitmap) ==
	    curr->curr_dynamic_inactive_bitmap)
		return;

	num = policy_mgr_get_legacy_conn_info(
					psoc, vdev_lst,
					freq_lst, mode_lst,
					QDF_ARRAY_SIZE(vdev_lst));
	if (!num)
		return;
	ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_REMOVED_LINK,
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	if (ml_num_link < 2)
		return;
	for (i = 0; i < ml_num_link; i++) {
		if (!((1 << ml_linkid_lst[i]) &
		      curr->curr_dynamic_inactive_bitmap))
			continue;
		for (j = 0; j < num; j++) {
			if (mode_lst[j] != PM_SAP_MODE)
				continue;
			if (policy_mgr_2_freq_always_on_same_mac(
				psoc, freq_lst[j], ml_freq_lst[i])) {
				force_inactive_link_bitmap |=
					1 << ml_linkid_lst[i];
				mlo_debug("force dynamic inactive link id %d freq %d for sap freq %d",
					  ml_linkid_lst[i], ml_freq_lst[i],
					  freq_lst[j]);
			} else if (num > 1 &&
				   policy_mgr_are_2_freq_on_same_mac(
					psoc, freq_lst[j], ml_freq_lst[i])) {
				force_inactive_link_bitmap |=
					1 << ml_linkid_lst[i];
				mlo_debug("force dynamic inactive link id %d freq %d for sap freq %d",
					  ml_linkid_lst[i], ml_freq_lst[i],
					  freq_lst[j]);
			}
		}
	}
	if (force_inactive_link_bitmap) {
		new->force_inactive_bitmap |= force_inactive_link_bitmap;
		ml_nlink_dump_force_state(new, "");
	}
}

/**
 * ml_nlink_sta_inactivity_allowed_with_quiet() - Check force inactive allowed
 * for links in bitmap
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @force_inactive_bitmap: force inactive link bimap
 *
 * If left links (exclude removed link and QUITE link) are zero, the force
 * inactive bitmap is not allowed.
 *
 * Return: true if allow to force inactive links in force_inactive_bitmap
 */
static bool ml_nlink_sta_inactivity_allowed_with_quiet(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				uint16_t force_inactive_bitmap)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];

	ml_nlink_get_link_info(psoc, vdev, (NLINK_EXCLUDE_REMOVED_LINK |
					    NLINK_EXCLUDE_QUIET_LINK |
					    NLINK_EXCLUDE_STANDBY_LINK),
			       QDF_ARRAY_SIZE(ml_linkid_lst),
			       ml_link_info, ml_freq_lst, ml_vdev_lst,
			       ml_linkid_lst, &ml_num_link,
			       &ml_link_bitmap);
	ml_link_bitmap &= ~force_inactive_bitmap;
	if (!ml_link_bitmap) {
		mlo_debug("not allow - no active link after force inactive 0x%x",
			  force_inactive_bitmap);
		return false;
	}

	return true;
}

/**
 * ml_nlink_allow_conc() - Check force inactive allowed for links in bitmap
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @no_forced_bitmap: no force link bitmap
 * @force_inactive_bitmap: force inactive link bimap
 *
 * Check the no force bitmap and force inactive bitmap are allowed to send
 * to firmware
 *
 * Return: true if allow to "no force" and force inactive links.
 */
static bool
ml_nlink_allow_conc(struct wlan_objmgr_psoc *psoc,
		    struct wlan_objmgr_vdev *vdev,
		    uint16_t no_forced_bitmap,
		    uint16_t force_inactive_bitmap)
{
	uint8_t vdev_id_num = 0;
	uint8_t vdev_ids[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t vdev_id_bitmap_sz;
	uint32_t vdev_id_bitmap[MLO_VDEV_BITMAP_SZ];
	uint32_t i;
	union conc_ext_flag conc_ext_flags;
	struct wlan_objmgr_vdev *ml_vdev;
	bool allow = true;
	qdf_freq_t freq = 0;
	struct wlan_channel *bss_chan;

	if (!ml_nlink_sta_inactivity_allowed_with_quiet(
			psoc, vdev, force_inactive_bitmap))
		return false;

	ml_nlink_convert_linkid_bitmap_to_vdev_bitmap(
		psoc, vdev, no_forced_bitmap, NULL, &vdev_id_bitmap_sz,
		vdev_id_bitmap,	&vdev_id_num, vdev_ids);

	for (i = 0; i < vdev_id_num; i++) {
		ml_vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						     vdev_ids[i],
						     WLAN_MLO_MGR_ID);
		if (!ml_vdev) {
			mlo_err("invalid vdev id %d ", vdev_ids[i]);
			continue;
		}

		/* If link is active, no need to check allow conc */
		if (!policy_mgr_vdev_is_force_inactive(psoc, vdev_ids[i])) {
			wlan_objmgr_vdev_release_ref(ml_vdev,
						     WLAN_MLO_MGR_ID);
			continue;
		}

		conc_ext_flags.value =
		policy_mgr_get_conc_ext_flags(ml_vdev, true);

		bss_chan = wlan_vdev_mlme_get_bss_chan(ml_vdev);
		if (bss_chan)
			freq = bss_chan->ch_freq;

		if (!policy_mgr_is_concurrency_allowed(psoc, PM_STA_MODE,
						       freq,
						       HW_MODE_20_MHZ,
						       conc_ext_flags.value,
						       NULL)) {
			wlan_objmgr_vdev_release_ref(ml_vdev,
						     WLAN_MLO_MGR_ID);
			break;
		}

		wlan_objmgr_vdev_release_ref(ml_vdev, WLAN_MLO_MGR_ID);
	}

	if (i < vdev_id_num) {
		mlo_err("not allow - vdev %d freq %d active due to conc",
			vdev_ids[i], freq);
		allow = false;
	}

	return allow;
}

static QDF_STATUS
ml_nlink_update_no_force_for_all(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev,
				 struct ml_link_force_state *curr,
				 struct ml_link_force_state *new,
				 enum mlo_link_force_reason reason)
{
	uint16_t no_force_links;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Special handling for clear all force mode in target.
	 * send MLO_LINK_FORCE_MODE_NO_FORCE to clear "all"
	 * to target
	 */
	if (!new->force_inactive_bitmap &&
	    !new->force_inactive_num &&
	    !new->force_active_bitmap &&
	    !new->force_active_num &&
	    (curr->force_inactive_bitmap ||
	     curr->force_inactive_num ||
	     curr->force_active_bitmap ||
	     curr->force_active_num)) {
		/* If link is force inactive already, but new command will
		 * mark it non-force, need to check conc allow or not.
		 */
		no_force_links = curr->force_inactive_bitmap;
		/* Check non forced links allowed by conc */
		if (!ml_nlink_allow_conc(psoc, vdev, no_force_links, 0)) {
			status = QDF_STATUS_E_INVAL;
			goto end;
		}

		status = policy_mgr_mlo_sta_set_nlink(
						psoc, wlan_vdev_get_id(vdev),
						reason,
						MLO_LINK_FORCE_MODE_NO_FORCE,
						0, 0, 0, 0);
	}

end:
	return status;
}

static QDF_STATUS
ml_nlink_update_force_inactive(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_vdev *vdev,
			       struct ml_link_force_state *curr,
			       struct ml_link_force_state *new,
			       enum mlo_link_force_reason reason)
{
	uint16_t no_force_links;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (new->force_inactive_bitmap != curr->force_inactive_bitmap) {
		/* If link is force inactive already, but new command will
		 * mark it non-force, need to check conc allow or not.
		 */
		no_force_links = curr->force_inactive_bitmap &
				 new->force_inactive_bitmap;
		no_force_links ^= curr->force_inactive_bitmap;

		/* Check non forced links allowed by conc */
		if (!ml_nlink_allow_conc(psoc, vdev, no_force_links,
					 new->force_inactive_bitmap)) {
			status = QDF_STATUS_E_NOSUPPORT;
			goto end;
		}
		status = policy_mgr_mlo_sta_set_nlink(
				psoc, wlan_vdev_get_id(vdev), reason,
				MLO_LINK_FORCE_MODE_INACTIVE,
				0,
				new->force_inactive_bitmap,
				0,
				link_ctrl_f_overwrite_inactive_bitmap |
				link_ctrl_f_post_re_evaluate);
	}

end:
	return status;
}

static QDF_STATUS
ml_nlink_update_force_inactive_num(struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_vdev *vdev,
				   struct ml_link_force_state *curr,
				   struct ml_link_force_state *new,
				   enum mlo_link_force_reason reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (new->force_inactive_num !=
			curr->force_inactive_num ||
	    new->force_inactive_num_bitmap !=
			curr->force_inactive_num_bitmap) {
		status = policy_mgr_mlo_sta_set_nlink(
					psoc, wlan_vdev_get_id(vdev), reason,
					MLO_LINK_FORCE_MODE_INACTIVE_NUM,
					new->force_inactive_num,
					new->force_inactive_num_bitmap,
					0,
					link_ctrl_f_dynamic_force_link_num |
					link_ctrl_f_post_re_evaluate);
	}

	return status;
}

static QDF_STATUS
ml_nlink_update_force_active(struct wlan_objmgr_psoc *psoc,
			     struct wlan_objmgr_vdev *vdev,
			     struct ml_link_force_state *curr,
			     struct ml_link_force_state *new,
			     enum mlo_link_force_reason reason)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ml_nlink_update_force_active_num(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev,
				 struct ml_link_force_state *curr,
				 struct ml_link_force_state *new,
				 enum mlo_link_force_reason reason)
{
	return QDF_STATUS_SUCCESS;
}

static bool
ml_nlink_all_links_ready_for_state_change(struct wlan_objmgr_psoc *psoc,
					  struct wlan_objmgr_vdev *vdev,
					  enum ml_nlink_change_event_type evt)
{
	uint8_t ml_num_link = 0;
	uint32_t ml_link_bitmap = 0;
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_linkid_lst[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ml_link_info ml_link_info[MAX_NUMBER_OF_CONC_CONNECTIONS];

	if (!mlo_check_if_all_links_up(vdev))
		return false;

	if (mlo_mgr_is_link_switch_in_progress(vdev) &&
	    evt != ml_nlink_connect_completion_evt) {
		mlo_debug("mlo vdev %d link switch in progress!",
			  wlan_vdev_get_id(vdev));
		return false;
	}
	/* For initial connecting to 2 or 3 links ML ap, assoc link and
	 * non assoc link connected one by one, avoid changing link state
	 * before link vdev connect completion, to check connected link count.
	 * If < 2, means non assoc link connect is not completed, disallow
	 * link state change.
	 */
	if (!mlo_mgr_is_link_switch_in_progress(vdev) &&
	    evt == ml_nlink_connect_completion_evt) {
		ml_nlink_get_link_info(psoc, vdev, NLINK_EXCLUDE_STANDBY_LINK,
				       QDF_ARRAY_SIZE(ml_linkid_lst),
				       ml_link_info, ml_freq_lst, ml_vdev_lst,
				       ml_linkid_lst, &ml_num_link,
				       &ml_link_bitmap);
		if (ml_num_link < 2)
			return false;
	}

	return true;
}

/**
 * ml_nlink_state_change() - Handle ML STA link force
 * with concurrency internal function
 * @psoc: PSOC object information
 * @reason: reason code of trigger force mode change.
 * @evt: event type
 * @data: event data
 *
 * This API handle link force for connected ML STA.
 * At present we only support one ML STA. so ml_nlink_get_affect_ml_sta
 * is invoked to get one ML STA vdev from policy mgr table.
 *
 * The flow is to get current force command which has been sent to target
 * and compute a new force command based on current connection table.
 * If any difference between "current" and "new", driver sends update
 * command to target. Driver will update the current force command
 * record after get successful respone from target.
 *
 * Return: QDF_STATUS_SUCCESS if no new command updated to target.
 *	   QDF_STATUS_E_PENDING if new command is sent to target.
 *	   otherwise QDF_STATUS error code
 */
static QDF_STATUS ml_nlink_state_change(struct wlan_objmgr_psoc *psoc,
					enum mlo_link_force_reason reason,
					enum ml_nlink_change_event_type evt,
					struct ml_nlink_change_event *data)
{
	struct ml_link_force_state force_state = {0};
	struct ml_link_force_state legacy_intf_force_state = {0};
	struct ml_link_force_state curr_force_state = {0};
	struct wlan_objmgr_vdev *vdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/*
	 * eMLSR is allowed in MCC mode also. So, don't disable any links
	 * if current connection happens in eMLSR mode.
	 * eMLSR is handled by wlan_handle_emlsr_sta_concurrency
	 */
	if (policy_mgr_is_mlo_in_mode_emlsr(psoc, NULL, NULL)) {
		mlo_debug("Don't disable eMLSR links");
		goto end;
	}

	vdev = ml_nlink_get_affect_ml_sta(psoc);
	if (!vdev)
		goto end;
	if (!ml_nlink_all_links_ready_for_state_change(psoc, vdev, evt))
		goto end;

	ml_nlink_get_curr_force_state(psoc, vdev, &curr_force_state);

	ml_nlink_handle_mcc_links(psoc, vdev, &force_state);

	ml_nlink_handle_legacy_intf(psoc, vdev, &legacy_intf_force_state);

	force_state.force_inactive_bitmap |=
		legacy_intf_force_state.force_inactive_bitmap;

	if (legacy_intf_force_state.force_inactive_num &&
	    legacy_intf_force_state.force_inactive_num >=
			force_state.force_inactive_num) {
		force_state.force_inactive_num =
			legacy_intf_force_state.force_inactive_num;
		force_state.force_inactive_num_bitmap =
		legacy_intf_force_state.force_inactive_num_bitmap;
	}

	ml_nlink_handle_dynamic_inactive(psoc, vdev, &curr_force_state,
					 &force_state);

	ml_nlink_update_concurrency_link_request(psoc, vdev,
						 &force_state,
						 reason);

	status = ml_nlink_update_no_force_for_all(psoc, vdev,
						  &curr_force_state,
						  &force_state,
						  reason);
	if (status == QDF_STATUS_E_PENDING || status != QDF_STATUS_SUCCESS)
		goto end;

	status = ml_nlink_update_force_inactive(psoc, vdev,
						&curr_force_state,
						&force_state,
						reason);
	if (status == QDF_STATUS_E_PENDING ||
	    (status != QDF_STATUS_SUCCESS && status != QDF_STATUS_E_NOSUPPORT))
		goto end;

	status = ml_nlink_update_force_inactive_num(psoc, vdev,
						    &curr_force_state,
						    &force_state,
						    reason);
	if (status == QDF_STATUS_E_PENDING || status != QDF_STATUS_SUCCESS)
		goto end;

	/* At present, only force inactive/inactive num mode have been used
	 * to avoid MCC, force active/active num APIs are no-op for now.
	 */
	status = ml_nlink_update_force_active(psoc, vdev,
					      &curr_force_state,
					      &force_state,
					      reason);
	if (status == QDF_STATUS_E_PENDING || status != QDF_STATUS_SUCCESS)
		goto end;

	status = ml_nlink_update_force_active_num(psoc, vdev,
						  &curr_force_state,
						  &force_state,
						  reason);
end:
	if (vdev) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

		if (status == QDF_STATUS_SUCCESS)
			mlo_debug("exit no force state change");
		else if (status == QDF_STATUS_E_PENDING)
			mlo_debug("exit pending force state change");
		else
			mlo_err("exit err %d state change", status);
	}

	return status;
}

/**
 * ml_nlink_state_change_handler() - Handle ML STA link force
 * with concurrency
 * @psoc: PSOC object information
 * @vdev: ml sta vdev object
 * @reason: reason code of trigger force mode change.
 * @evt: event type
 * @data: event data
 *
 * Return: QDF_STATUS_SUCCESS if successfully
 */
static QDF_STATUS
ml_nlink_state_change_handler(struct wlan_objmgr_psoc *psoc,
			      struct wlan_objmgr_vdev *vdev,
			      enum mlo_link_force_reason reason,
			      enum ml_nlink_change_event_type evt,
			      struct ml_nlink_change_event *data)
{
	enum QDF_OPMODE mode = wlan_vdev_mlme_get_opmode(vdev);
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* If WMI_SERVICE_N_LINK_MLO_SUPPORT = 381 is enabled,
	 * indicate FW support N MLO link & vdev re-purpose between links,
	 * host will use linkid bitmap to force inactive/active links
	 * by API ml_nlink_state_change.
	 * Otherwise, use legacy policy mgr API to inactive/active based
	 * on vdev id bitmap.
	 */
	if (ml_is_nlink_service_supported(psoc))
		status = ml_nlink_state_change(psoc, reason, evt, data);
	else if (reason == MLO_LINK_FORCE_REASON_CONNECT)
		policy_mgr_handle_ml_sta_links_on_vdev_up_csa(psoc, mode,
							      vdev_id);
	else
		policy_mgr_handle_ml_sta_links_on_vdev_down(psoc, mode,
							    vdev_id);

	return status;
}

static QDF_STATUS
ml_nlink_tdls_event_handler(struct wlan_objmgr_psoc *psoc,
			    struct wlan_objmgr_vdev *vdev,
			    enum ml_nlink_change_event_type evt,
			    struct ml_nlink_change_event *data)
{
	struct ml_link_force_state curr_force_state = {0};
	QDF_STATUS status;
	struct set_link_req vendor_req = {0};

	ml_nlink_get_curr_force_state(psoc, vdev, &curr_force_state);

	switch (data->evt.tdls.mode) {
	case MLO_LINK_FORCE_MODE_ACTIVE:
		if ((data->evt.tdls.link_bitmap &
		    curr_force_state.force_active_bitmap) ==
		    data->evt.tdls.link_bitmap) {
			mlo_debug("link_bitmap 0x%x already active, 0x%x",
				  data->evt.tdls.link_bitmap,
				  curr_force_state.force_active_bitmap);
			return QDF_STATUS_SUCCESS;
		}
		if (data->evt.tdls.link_bitmap &
		    (curr_force_state.force_inactive_bitmap |
		     curr_force_state.curr_dynamic_inactive_bitmap)) {
			mlo_debug("link_bitmap 0x%x can't be active due to concurrency, 0x%x 0x%x",
				  data->evt.tdls.link_bitmap,
				  curr_force_state.force_inactive_bitmap,
				  curr_force_state.
				  curr_dynamic_inactive_bitmap);
			return QDF_STATUS_E_INVAL;
		}
		break;
	case MLO_LINK_FORCE_MODE_NO_FORCE:
		if (data->evt.tdls.link_bitmap &
		    curr_force_state.force_inactive_bitmap) {
			mlo_debug("link_bitmap 0x%x can't be no_force due to concurrency, 0x%x",
				  data->evt.tdls.link_bitmap,
				  curr_force_state.force_inactive_bitmap);
			return QDF_STATUS_E_INVAL;
		}
		if (!(data->evt.tdls.link_bitmap &
			curr_force_state.force_active_bitmap)) {
			mlo_debug("link_bitmap 0x%x already no force, 0x%x",
				  data->evt.tdls.link_bitmap,
				  curr_force_state.force_active_bitmap);
			return QDF_STATUS_SUCCESS;
		}
		ml_nlink_get_force_link_request(psoc, vdev, &vendor_req,
						SET_LINK_FROM_VENDOR_CMD);
		if (data->evt.tdls.link_bitmap &
		    vendor_req.force_active_bitmap) {
			mlo_debug("link_bitmap 0x%x active hold by vendor cmd, 0x%x",
				  data->evt.tdls.link_bitmap,
				  vendor_req.force_active_bitmap);
			return QDF_STATUS_SUCCESS;
		}
		break;
	default:
		mlo_err("unhandled for tdls force mode %d",
			data->evt.tdls.mode);
		return QDF_STATUS_E_INVAL;
	}

	if (ml_is_nlink_service_supported(psoc))
		status = policy_mgr_mlo_sta_set_nlink(
				psoc, wlan_vdev_get_id(vdev),
				data->evt.tdls.reason,
				data->evt.tdls.mode,
				0,
				data->evt.tdls.link_bitmap,
				0,
				0);
	else
		status =
		policy_mgr_mlo_sta_set_link(psoc,
					    data->evt.tdls.reason,
					    data->evt.tdls.mode,
					    data->evt.tdls.vdev_count,
					    data->evt.tdls.mlo_vdev_lst);

	return status;
}

static QDF_STATUS
ml_nlink_vendor_cmd_handler(struct wlan_objmgr_psoc *psoc,
			    struct wlan_objmgr_vdev *vdev,
			    enum ml_nlink_change_event_type evt,
			    struct ml_nlink_change_event *data)
{
	struct set_link_req req = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	mlo_debug("link ctrl %d mode %d reason %d num %d 0x%x 0x%x",
		  data->evt.vendor.link_ctrl_mode,
		  data->evt.vendor.mode,
		  data->evt.vendor.reason,
		  data->evt.vendor.link_num,
		  data->evt.vendor.link_bitmap,
		  data->evt.vendor.link_bitmap2);
	switch (data->evt.vendor.link_ctrl_mode) {
	case LINK_CONTROL_MODE_DEFAULT:
		ml_nlink_clr_force_link_request(psoc, vdev,
						SET_LINK_FROM_VENDOR_CMD);
		break;
	case LINK_CONTROL_MODE_USER:
		req.mode = data->evt.vendor.mode;
		req.reason = data->evt.vendor.reason;
		req.force_active_bitmap = data->evt.vendor.link_bitmap;
		req.force_inactive_bitmap = data->evt.vendor.link_bitmap2;
		ml_nlink_update_force_link_request(psoc, vdev, &req,
						   SET_LINK_FROM_VENDOR_CMD);
		break;
	case LINK_CONTROL_MODE_MIXED:
		req.mode = data->evt.vendor.mode;
		req.reason = data->evt.vendor.reason;
		req.force_active_num = data->evt.vendor.link_num;
		req.force_active_num_bitmap = data->evt.vendor.link_bitmap;
		ml_nlink_update_force_link_request(psoc, vdev, &req,
						   SET_LINK_FROM_VENDOR_CMD);
		break;
	default:
		status = QDF_STATUS_E_INVAL;
	}

	return status;
}

static QDF_STATUS
ml_nlink_swtich_dynamic_inactive_link(struct wlan_objmgr_psoc *psoc,
				      struct wlan_objmgr_vdev *vdev)
{
	uint8_t link_id;
	uint32_t standby_link_bitmap, dynamic_inactive_bitmap;
	struct ml_link_force_state curr_force_state = {0};
	uint8_t link_ids[MAX_MLO_LINK_ID];
	uint8_t num_ids;

	link_id = wlan_vdev_get_link_id(vdev);
	if (link_id >= MAX_MLO_LINK_ID) {
		mlo_err("invalid link id %d", link_id);
		return QDF_STATUS_E_INVAL;
	}

	ml_nlink_get_curr_force_state(psoc, vdev, &curr_force_state);
	standby_link_bitmap = ml_nlink_get_standby_link_bitmap(psoc, vdev);
	standby_link_bitmap &= curr_force_state.force_inactive_num_bitmap &
				~(1 << link_id);
	/* In DBS RD, ML STA 2+5+6(standby link), force inactive num = 1 and
	 * force inactive bitmap with 5 + 6 links will be sent to FW, host
	 * will select 6G as dynamic inactive link, 5G vdev will be kept in
	 * policy mgr active connection table.
	 * If FW link switch and repurpose 5G vdev to 6G, host will need to
	 * select 5G standby link as dynamic inactive.
	 * Then 6G vdev can be moved to policy mgr active connection table.
	 */
	if (((1 << link_id) & curr_force_state.curr_dynamic_inactive_bitmap) &&
	    ((1 << link_id) & curr_force_state.force_inactive_num_bitmap) &&
	    !(standby_link_bitmap &
			curr_force_state.curr_dynamic_inactive_bitmap) &&
	    (standby_link_bitmap &
			curr_force_state.force_inactive_num_bitmap)) {
		num_ids = convert_link_bitmap_to_link_ids(
						standby_link_bitmap,
						QDF_ARRAY_SIZE(link_ids),
						link_ids);
		if (!num_ids) {
			mlo_err("unexpected 0 link ids for bitmap 0x%x",
				standby_link_bitmap);
			return QDF_STATUS_E_INVAL;
		}
		/* Remove the link from dynamic inactive bitmap,
		 * add the standby link to dynamic inactive bitmap.
		 */
		dynamic_inactive_bitmap =
			curr_force_state.curr_dynamic_inactive_bitmap &
						~(1 << link_id);
		dynamic_inactive_bitmap |= 1 << link_ids[0];
		mlo_debug("move out vdev %d link id %d from dynamic inactive, add standby link id %d",
			  wlan_vdev_get_id(vdev), link_id, link_ids[0]);
		ml_nlink_set_dynamic_inactive_links(psoc, vdev,
						    dynamic_inactive_bitmap);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ml_nlink_conn_change_notify(struct wlan_objmgr_psoc *psoc,
			    uint8_t vdev_id,
			    enum ml_nlink_change_event_type evt,
			    struct ml_nlink_change_event *data)
{
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE mode;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct ml_link_force_state curr_force_state = {0};
	bool is_set_link_in_progress = policy_mgr_is_set_link_in_progress(psoc);
	bool is_host_force;

	mlo_debug("vdev %d %s(%d)", vdev_id, link_evt_to_string(evt),
		  evt);
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLO_MGR_ID);
	if (!vdev) {
		mlo_err("invalid vdev id %d ", vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	mode = wlan_vdev_mlme_get_opmode(vdev);

	switch (evt) {
	case ml_nlink_link_switch_start_evt:
		if (data->evt.link_switch.reason ==
		    MLO_LINK_SWITCH_REASON_HOST_FORCE) {
			is_host_force = true;
		} else {
			is_host_force = false;
		}

		mlo_debug("set_link_in_prog %d reason %d",
			  is_set_link_in_progress,
			  data->evt.link_switch.reason);

		if (is_set_link_in_progress) {
			/* If set active is in progress then only accept host
			 * force link switch requests from FW
			 */
			if (is_host_force)
				status = QDF_STATUS_SUCCESS;
			else
				status = QDF_STATUS_E_INVAL;
			break;
		} else if (is_host_force) {
			/* If set active is not in progress but FW sent host
			 * force then reject the link switch
			 */
			status = QDF_STATUS_E_INVAL;
			break;
		}

		ml_nlink_get_curr_force_state(psoc, vdev, &curr_force_state);
		if ((1 << data->evt.link_switch.new_ieee_link_id) &
		    curr_force_state.force_inactive_bitmap) {
			mlo_debug("target link %d is force inactive, don't switch to it",
				  data->evt.link_switch.new_ieee_link_id);
			status = QDF_STATUS_E_INVAL;
		}
		break;
	case ml_nlink_link_switch_pre_completion_evt:
		status = ml_nlink_swtich_dynamic_inactive_link(
				psoc, vdev);
		break;
	case ml_nlink_roam_sync_start_evt:
		ml_nlink_clr_force_state(psoc, vdev);
		break;
	case ml_nlink_roam_sync_completion_evt:
		status = ml_nlink_state_change_handler(
			psoc, vdev, MLO_LINK_FORCE_REASON_CONNECT,
			evt, data);
		break;
	case ml_nlink_connect_start_evt:
		ml_nlink_clr_force_state(psoc, vdev);
		break;
	case ml_nlink_connect_completion_evt:
		status = ml_nlink_state_change_handler(
			psoc, vdev, MLO_LINK_FORCE_REASON_CONNECT,
			evt, data);
		break;
	case ml_nlink_disconnect_start_evt:
		ml_nlink_clr_force_state(psoc, vdev);
		break;
	case ml_nlink_disconnect_completion_evt:
		status = ml_nlink_state_change_handler(
			psoc, vdev, MLO_LINK_FORCE_REASON_DISCONNECT,
			evt, data);
		break;
	case ml_nlink_ap_started_evt:
		status = ml_nlink_state_change_handler(
			psoc, vdev, MLO_LINK_FORCE_REASON_CONNECT,
			evt, data);
		break;
	case ml_nlink_ap_stopped_evt:
		status = ml_nlink_state_change_handler(
			psoc, vdev, MLO_LINK_FORCE_REASON_DISCONNECT,
			evt, data);
		break;
	case ml_nlink_connection_updated_evt:
		if (mode == QDF_STA_MODE &&
		    (MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) ||
		     MLME_IS_MLO_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id))) {
			mlo_debug("vdev id %d in roam sync", vdev_id);
			break;
		}
		status = ml_nlink_state_change_handler(
			psoc, vdev, MLO_LINK_FORCE_REASON_CONNECT,
			evt, data);
		break;
	case ml_nlink_tdls_request_evt:
		status = ml_nlink_tdls_event_handler(
			psoc, vdev, evt, data);
		break;
	case ml_nlink_vendor_cmd_request_evt:
		status = ml_nlink_vendor_cmd_handler(
			psoc, vdev, evt, data);
		break;
	default:
		break;
	}

	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	return status;
}

void
ml_nlink_vendor_command_set_link(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 enum link_control_modes link_control_mode,
				 enum mlo_link_force_reason reason,
				 enum mlo_link_force_mode mode,
				 uint8_t link_num,
				 uint16_t link_bitmap,
				 uint16_t link_bitmap2)
{
	struct ml_nlink_change_event data;

	qdf_mem_zero(&data, sizeof(data));
	data.evt.vendor.link_ctrl_mode = link_control_mode;
	data.evt.vendor.mode = mode;
	data.evt.vendor.reason = reason;
	data.evt.vendor.link_num = link_num;
	data.evt.vendor.link_bitmap = link_bitmap;
	data.evt.vendor.link_bitmap2 = link_bitmap2;

	ml_nlink_conn_change_notify(
			psoc, vdev_id,
			ml_nlink_vendor_cmd_request_evt, &data);
}
