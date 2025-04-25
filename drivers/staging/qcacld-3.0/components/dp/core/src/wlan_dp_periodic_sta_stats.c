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
 * DOC: WLAN Host Device Driver periodic STA statistics related implementation
 */

#include "wlan_dp_periodic_sta_stats.h"

void dp_periodic_sta_stats_display(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_dp_intf *dp_intf, next_dp_intf = NULL;
	struct dp_stats sta_stats;
	struct wlan_dp_psoc_cfg *dp_cfg;
	char *dev_name;
	bool should_log;

	if (!dp_ctx)
		return;

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, next_dp_intf) {
		should_log = false;

		if (dp_intf->device_mode != QDF_STA_MODE)
			continue;

		dp_cfg = dp_ctx->dp_cfg;
		qdf_mutex_acquire(&dp_intf->sta_periodic_stats_lock);

		if (!dp_intf->is_sta_periodic_stats_enabled) {
			qdf_mutex_release(&dp_intf->sta_periodic_stats_lock);
			continue;
		}

		dp_intf->periodic_stats_timer_counter++;
		if ((dp_intf->periodic_stats_timer_counter *
		    dp_cfg->bus_bw_compute_interval) >=
				dp_cfg->periodic_stats_timer_interval) {
			should_log = true;

			dp_intf->periodic_stats_timer_count--;
			if (dp_intf->periodic_stats_timer_count == 0)
				dp_intf->is_sta_periodic_stats_enabled = false;
			dp_intf->periodic_stats_timer_counter = 0;
		}
		qdf_mutex_release(&dp_intf->sta_periodic_stats_lock);

		if (should_log) {
			dev_name = qdf_netdev_get_devname(dp_intf->dev);
			sta_stats = dp_intf->dp_stats;
			dp_nofl_info("%s: Tx ARP requests: %d", dev_name,
				     sta_stats.dp_arp_stats.tx_arp_req_count);
			dp_nofl_info("%s: Rx ARP responses: %d", dev_name,
				     sta_stats.dp_arp_stats.rx_arp_rsp_count);
			dp_nofl_info("%s: Tx DNS requests: %d", dev_name,
				     sta_stats.dp_dns_stats.tx_dns_req_count);
			dp_nofl_info("%s: Rx DNS responses: %d", dev_name,
				     sta_stats.dp_dns_stats.rx_dns_rsp_count);
		}
	}
}

void dp_periodic_sta_stats_config(struct dp_config *config,
				  struct wlan_objmgr_psoc *psoc)
{
	config->periodic_stats_timer_interval =
		cfg_get(psoc, CFG_PERIODIC_STATS_TIMER_INTERVAL);
	config->periodic_stats_timer_duration =
		cfg_get(psoc, CFG_PERIODIC_STATS_TIMER_DURATION);
}

void dp_periodic_sta_stats_start(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;
	struct dp_config *dp_cfg;

	if (!dp_link) {
		dp_nofl_err("Unable to get DP link");
		return;
	}

	dp_intf = dp_link->dp_intf;
	dp_cfg = dp_intf->dp_ctx->dp_cfg;

	if ((dp_intf->device_mode == QDF_STA_MODE) &&
	    (dp_cfg->periodic_stats_timer_interval > 0)) {
		qdf_mutex_acquire(&dp_intf->sta_periodic_stats_lock);

		/* Stop the periodic ARP and DNS stats timer */
		dp_intf->periodic_stats_timer_count = 0;
		dp_intf->is_sta_periodic_stats_enabled = false;

		qdf_mutex_release(&dp_intf->sta_periodic_stats_lock);
	}
}

void dp_periodic_sta_stats_stop(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;
	struct dp_config *dp_cfg;

	if (!dp_link) {
		dp_nofl_err("Unable to get DP link");
		return;
	}

	dp_intf = dp_link->dp_intf;
	dp_cfg = dp_intf->dp_ctx->dp_cfg;

	if ((dp_intf->device_mode == QDF_STA_MODE) &&
	    (dp_cfg->periodic_stats_timer_interval > 0)) {
		qdf_mutex_acquire(&dp_intf->sta_periodic_stats_lock);

		dp_intf->periodic_stats_timer_count =
			dp_cfg->periodic_stats_timer_duration /
			dp_cfg->periodic_stats_timer_interval;
		dp_intf->periodic_stats_timer_counter = 0;
		if (dp_intf->periodic_stats_timer_count > 0)
			dp_intf->is_sta_periodic_stats_enabled = true;

		qdf_mutex_release(&dp_intf->sta_periodic_stats_lock);
	}
}

void dp_periodic_sta_stats_init(struct wlan_dp_intf *dp_intf)
{
	dp_intf->is_sta_periodic_stats_enabled = false;
}

void dp_periodic_sta_stats_mutex_create(struct wlan_dp_intf *dp_intf)
{
	qdf_mutex_create(&dp_intf->sta_periodic_stats_lock);
}

void dp_periodic_sta_stats_mutex_destroy(struct wlan_dp_intf *dp_intf)
{
qdf_mutex_destroy(&dp_intf->sta_periodic_stats_lock);
}
