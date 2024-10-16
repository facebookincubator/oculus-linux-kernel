/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: WLAN Host Device Driver periodic STA statistics related implementation
 */

#if !defined(WLAN_DP_PERIODIC_STA_STATS_H)
#define WLAN_DP_PERIODIC_STA_STATS_H

#include "wlan_dp_priv.h"
#include "wlan_objmgr_psoc_obj.h"

#ifdef WLAN_FEATURE_PERIODIC_STA_STATS

/**
 * dp_periodic_sta_stats_config() - Initialize periodic stats configuration
 * @config: Pointer to dp configuration
 * @psoc: Pointer to psoc
 *
 * Return: none
 */
void dp_periodic_sta_stats_config(struct wlan_dp_psoc_cfg *config,
				  struct wlan_objmgr_psoc *psoc);

/**
 * dp_periodic_sta_stats_init() - Initialize periodic stats display flag
 * @dp_intf: Pointer to the station interface
 *
 * Return: none
 */
void dp_periodic_sta_stats_init(struct wlan_dp_intf *dp_intf);

/**
 * dp_periodic_sta_stats_display() - Display periodic stats at STA
 * @dp_ctx: dp context
 *
 * Return: none
 */
void dp_periodic_sta_stats_display(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_periodic_sta_stats_start() - Start displaying periodic stats for STA
 * @vdev: vdev handle
 *
 * Return: none
 */
void dp_periodic_sta_stats_start(struct wlan_objmgr_vdev *vdev);

/**
 * dp_periodic_sta_stats_stop() - Stop displaying periodic stats for STA
 * @vdev: vdev handle
 *
 * Return: none
 */
void dp_periodic_sta_stats_stop(struct wlan_objmgr_vdev *vdev);

/**
 * dp_periodic_sta_stats_mutex_create() - Create mutex for STA periodic stats
 * @dp_intf: Pointer to the station interface
 *
 * Return: none
 */
void dp_periodic_sta_stats_mutex_create(struct wlan_dp_intf *dp_intf);

/**
 * dp_periodic_sta_stats_mutex_destroy() - Destroy STA periodic stats mutex
 * @dp_intf: Pointer to the station interface
 *
 * Return: none
 */
void dp_periodic_sta_stats_mutex_destroy(struct wlan_dp_intf *dp_intf);

#else
static inline void
dp_periodic_sta_stats_display(struct wlan_dp_psoc_context *dp_ctx)
{
}

static inline void
dp_periodic_sta_stats_config(struct wlan_dp_psoc_cfg *config,
			     struct wlan_objmgr_psoc *psoc)
{
}

static inline void dp_periodic_sta_stats_start(struct wlan_objmgr_vdev *vdev)
{
}

static inline void dp_periodic_sta_stats_stop(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
dp_periodic_sta_stats_init(struct wlan_dp_intf *dp_intf)
{
}

static inline void
dp_periodic_sta_stats_mutex_create(struct wlan_dp_intf *dp_intf)
{
}

static inline void
dp_periodic_sta_stats_mutex_destroy(struct wlan_dp_intf *dp_intf)
{
}
#endif /* end #ifdef WLAN_FEATURE_PERIODIC_STA_STATS */
#endif /* end #if !defined(WLAN_DP_PERIODIC_STA_STATS_H) */
