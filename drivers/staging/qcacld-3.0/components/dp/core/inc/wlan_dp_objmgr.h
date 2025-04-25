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

/**
 * DOC: This file contains various object manager related wrappers and helpers
 */

#ifndef __WLAN_DP_OBJMGR_H
#define __WLAN_DP_OBJMGR_H

#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_peer_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_utility.h"

struct wlan_dp_intf;
struct wlan_dp_link;

/* Get/Put Ref */

#define dp_comp_peer_get_ref(peer) wlan_objmgr_peer_try_get_ref(peer, WLAN_DP_ID)
#define dp_comp_peer_put_ref(peer) wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID)

#define dp_comp_vdev_get_ref(vdev) wlan_objmgr_vdev_try_get_ref(vdev, WLAN_DP_ID)
#define dp_comp_vdev_put_ref(vdev) wlan_objmgr_vdev_release_ref(vdev, WLAN_DP_ID)

#define dp_comp_pdev_get_ref(pdev) wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DP_ID)
#define dp_comp_pdev_put_ref(pdev) wlan_objmgr_pdev_release_ref(pdev, WLAN_DP_ID)

#define dp_comp_psoc_get_ref(psoc) wlan_objmgr_psoc_try_get_ref(psoc, WLAN_DP_ID)
#define dp_comp_psoc_put_ref(psoc) wlan_objmgr_psoc_release_ref(psoc, WLAN_DP_ID)

/**
 * dp_get_peer_priv_obj: get DP priv object from peer object
 * @peer: pointer to peer object
 *
 * Return: pointer to DP peer private object
 */
static inline struct wlan_dp_sta_info *
dp_get_peer_priv_obj(struct wlan_objmgr_peer *peer)
{
	struct wlan_dp_sta_info *peer_info;

	peer_info = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_COMP_DP);
	if (!peer_info) {
		dp_err("peer is null");
		return NULL;
	}

	return peer_info;
}

/**
 * dp_get_vdev_priv_obj() - Wrapper to retrieve vdev priv obj
 * @vdev: vdev pointer
 *
 * Return: DP vdev private object
 */
static inline struct wlan_dp_link *
dp_get_vdev_priv_obj(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_link *obj;

	if (!vdev) {
		dp_err("vdev is null");
		return NULL;
	}

	obj = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_COMP_DP);

	return obj;
}

/**
 * dp_psoc_get_priv() - Wrapper to retrieve psoc priv obj
 * @psoc: psoc pointer
 *
 * Return: DP psoc private object
 */
static inline struct wlan_dp_psoc_context *
dp_psoc_get_priv(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_COMP_DP);
	QDF_BUG(dp_ctx);

	return dp_ctx;
}

/**
 * dp_objmgr_get_vdev_by_user() - Get reference of vdev from dp_link
 *  with user id
 * @dp_link: DP link handle
 * @dbgid: reference count dbg id
 *
 * Return: pointer to vdev object for success, NULL for failure
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define dp_objmgr_get_vdev_by_user(dp_link, dbgid) \
	__dp_objmgr_get_vdev_by_user(dp_link, dbgid, __func__, __LINE__)
struct wlan_objmgr_vdev *
__dp_objmgr_get_vdev_by_user(struct wlan_dp_link *dp_link,
			     wlan_objmgr_ref_dbgid id,
			     const char *func,
			     int line);
#else
#define dp_objmgr_get_vdev_by_user(dp_link, dbgid) \
	__dp_objmgr_get_vdev_by_user(dp_link, dbgid, __func__)
struct wlan_objmgr_vdev *
__dp_objmgr_get_vdev_by_user(struct wlan_dp_link *dp_link,
			     wlan_objmgr_ref_dbgid id,
			     const char *func);
#endif

/**
 * dp_objmgr_put_vdev_by_user() - Release reference of vdev object with
 *  user id
 * @vdev: pointer to vdev object
 * @dbgid: reference count dbg id
 *
 * This API releases vdev object reference which was acquired using
 * dp_objmgr_get_vdev_by_user().
 *
 * Return: void
 */
#ifdef WLAN_OBJMGR_REF_ID_TRACE
#define dp_objmgr_put_vdev_by_user(vdev, dbgid) \
	__dp_objmgr_put_vdev_by_user(vdev, dbgid, __func__, __LINE__)
void
__dp_objmgr_put_vdev_by_user(struct wlan_objmgr_vdev *vdev,
			     wlan_objmgr_ref_dbgid id, const char *func,
			     int line);
#else
#define dp_objmgr_put_vdev_by_user(vdev, dbgid) \
	__dp_objmgr_put_vdev_by_user(vdev, dbgid, __func__)
void
__dp_objmgr_put_vdev_by_user(struct wlan_objmgr_vdev *vdev,
			     wlan_objmgr_ref_dbgid id, const char *func);
#endif

#endif /* __WLAN_DP_OBJMGR_H */
