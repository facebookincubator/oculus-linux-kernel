/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: contains interface prototypes for OS_IF layer
 */
#ifndef _SPATIAL_REUSE_UCFG_API_H_
#define _SPATIAL_REUSE_UCFG_API_H_

#include <qdf_trace.h>
#include <wlan_objmgr_vdev_obj.h>
#include <spatial_reuse_api.h>

#ifdef WLAN_FEATURE_SR
/**
 * ucfg_spatial_reuse_register_cb() - Registers CB for SR
 * @psoc: pointer to psoc
 * @cb: SR osif event callback
 *
 * Return: void
 */
void ucfg_spatial_reuse_register_cb(struct wlan_objmgr_psoc *psoc,
				    sr_osif_event_cb cb);

/**
 * ucfg_spatial_reuse_get_sr_config() - Spatial reuse config get
 *
 * @vdev: object manager vdev
 * @sr_ctrl: spatial reuse sr control
 * @non_srg_max_pd_offset: non-srg max pd offset
 * @he_spr_enabled: spatial reuse enabled
 *
 * Return: void
 */
void ucfg_spatial_reuse_get_sr_config(struct wlan_objmgr_vdev *vdev,
				      uint8_t *sr_ctrl,
				      uint8_t *non_srg_max_pd_offset,
				      bool *he_spr_enabled);
/**
 * ucfg_spatial_reuse_set_sr_config() - Spatial reuse config set
 *
 * @vdev: object manager vdev
 * @sr_ctrl: spatial reuse sr control
 * @non_srg_max_pd_offset: non-srg max pd offset
 *
 * Return: void
 */
void ucfg_spatial_reuse_set_sr_config(struct wlan_objmgr_vdev *vdev,
				      uint8_t sr_ctrl,
				      uint8_t non_srg_max_pd_offset);

/**
 * ucfg_spatial_reuse_is_sr_disabled_due_conc() - Spatial reuse get concurrency
 *						  status
 *
 * @vdev: object manager vdev
 *
 * Return: True when SR is disabled due to concurrency or else False
 */
bool ucfg_spatial_reuse_is_sr_disabled_due_conc(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_spatial_reuse_set_sr_conc_stat() - Spatial reuse disable config set
 *
 * @vdev: object manager vdev
 * @sr_conc_disabled: spatial reuse disabled due to concurrency
 *
 * Return: void
 */
void ucfg_spatial_reuse_set_sr_conc_stat(struct wlan_objmgr_vdev *vdev,
					 bool sr_conc_disabled);

/**
 * ucfg_spatial_reuse_send_sr_config() - Send spatial reuse config to fw
 *
 * @vdev: object manager vdev
 * @enable: spatial reuse config to be enabled or not
 *
 * Return: void
 */
void ucfg_spatial_reuse_send_sr_config(struct wlan_objmgr_vdev *vdev,
				       bool enable);

/**
 * ucfg_spatial_reuse_set_sr_enable() - set enable/disable Spatial reuse
 *
 * @vdev: object manager vdev
 * @enable: spatial reuse to be enabled or not
 *
 * Return: void
 */
void ucfg_spatial_reuse_set_sr_enable(struct wlan_objmgr_vdev *vdev,
				      bool enable);

/**
 * ucfg_spatial_reuse_send_sr_prohibit() - Send spatial reuse config to enable
 *					   or disable he_siga_val15_allowed
 *
 * @vdev: object manager vdev
 * @enable_he_siga_val15_prohibit: enable/disable he_siga_val15_allowed
 *
 * Return: success/failure
 */
QDF_STATUS
ucfg_spatial_reuse_send_sr_prohibit(struct wlan_objmgr_vdev *vdev,
				    bool enable_he_siga_val15_prohibit);

/**
 * ucfg_spatial_reuse_setup_req() - To enable/disable SR (Spatial Reuse)
 * @vdev: object manager vdev
 * @pdev: object manager pdev
 * @is_sr_enable: SR enable/disable
 * @srg_pd_threshold: SRG pd threshold
 * @non_srg_pd_threshold: NON SRG pd threshold
 *
 * Return: Success/Failure
 */
QDF_STATUS
ucfg_spatial_reuse_setup_req(struct wlan_objmgr_vdev *vdev,
			     struct wlan_objmgr_pdev *pdev,
			     bool is_sr_enable, int32_t srg_pd_threshold,
			     int32_t non_srg_pd_threshold);

/**
 * ucfg_spatial_reuse_operation_allowed() - Checks whether SR is allowed or not
 * @psoc: Object manager psoc
 * @vdev: object manager vdev
 *
 * Return: QDF_STATUS_SUCCESS when SR is allowed else Failure
 */
QDF_STATUS
ucfg_spatial_reuse_operation_allowed(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_vdev *vdev);
#else
static inline
void ucfg_spatial_reuse_register_cb(struct wlan_objmgr_psoc *psoc,
				    sr_osif_event_cb cb)
{
}
#endif
#endif
