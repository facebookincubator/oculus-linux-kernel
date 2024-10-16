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
 * DOC: contains interface prototypes for spatial_reuse api
 */

#ifndef _SPATIAL_REUSE_API_H_
#define _SPATIAL_REUSE_API_H_

#include <qdf_types.h>
#include <qdf_trace.h>
#include <wlan_objmgr_vdev_obj.h>

/**
 * enum sr_osif_operation - Spatial Reuse operation
 * @SR_OPERATION_SUSPEND: Spatial Reuse suspend indication
 * @SR_OPERATION_RESUME: Spatial Reuse resume indication
 * @SR_OPERATION_UPDATE_PARAMS: Spatial Reuse parameters are updated
 */
enum sr_osif_operation {
	SR_OPERATION_SUSPEND = 0,
	SR_OPERATION_RESUME = 1,
	SR_OPERATION_UPDATE_PARAMS = 2,
};

/**
 * enum sr_osif_reason_code - Spatial Reuse reason codes
 * @SR_REASON_CODE_ROAMING: Spatial Reuse reason code is Roaming will be
 *			     set when SR is suspended / resumed due to roaming
 * @SR_REASON_CODE_CONCURRENCY: Spatial Reuse reason code is concurrency
 *				 will be set when SR is suspended / resumed
 *				 due to concurrency
 * @SR_REASON_CODE_BCN_IE_CHANGE: Spatial Reuse reason code is SRP IE change
 *				  in the beacon/probe rsp of the associated AP
 */
enum sr_osif_reason_code {
	SR_REASON_CODE_ROAMING = 0,
	SR_REASON_CODE_CONCURRENCY = 1,
	SR_REASON_CODE_BCN_IE_CHANGE = 2,
};

/**
 * typedef sr_osif_event_cb() - CB to deliver SR events
 * @vdev: objmgr manager vdev
 * @sr_osif_oper: SR Operation like suspend / resume
 * @sr_osif_rc: Event reason code
 *
 * Return: void
 */
typedef void (*sr_osif_event_cb)(struct wlan_objmgr_vdev *vdev,
				 enum sr_osif_operation sr_osif_oper,
				 enum sr_osif_reason_code sr_osif_rc);

#ifdef WLAN_FEATURE_SR
/**
 * wlan_spatial_reuse_config_set() - Set spatial reuse config
 * @vdev: objmgr manager vdev
 * @sr_ctrl: spatial reuse control
 * @non_srg_max_pd_offset: non-srg max pd offset
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_spatial_reuse_config_set(struct wlan_objmgr_vdev *vdev,
					 uint8_t sr_ctrl,
					 uint8_t non_srg_max_pd_offset);

/**
 * wlan_sr_register_callback() - registers SR osif events
 * @psoc: pointer to psoc
 * @cb: Callback to be registered
 *
 * Return: void
 */
void wlan_sr_register_callback(struct wlan_objmgr_psoc *psoc,
			       sr_osif_event_cb cb);

/**
 * wlan_spatial_reuse_osif_event() - Send SR asynchronous events
 * @vdev: objmgr manager vdev
 * @sr_osif_oper: SR Operation like suspend / resume
 * @sr_osif_rc: Event reason code
 *
 * Return: void
 */
void wlan_spatial_reuse_osif_event(struct wlan_objmgr_vdev *vdev,
				   enum sr_osif_operation sr_osif_oper,
				   enum sr_osif_reason_code sr_osif_rc);
#else
static inline
QDF_STATUS wlan_spatial_reuse_config_set(struct wlan_objmgr_vdev *vdev,
					 uint8_t sr_ctrl,
					 uint8_t non_srg_max_pd_offset)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void wlan_spatial_reuse_osif_event(struct wlan_objmgr_vdev *vdev,
				   enum sr_osif_operation sr_osif_oper,
				   enum sr_osif_reason_code sr_osif_rc)
{
}
#endif

/**
 * wlan_spatial_reuse_he_siga_val15_allowed_set() - Set spatial reuse config
 *						    he_siga_val15_allowed
 * @vdev: objmgr manager vdev
 * @he_siga_va15_allowed: enable/disable he_siga_val15_allowed
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_spatial_reuse_he_siga_val15_allowed_set(
					struct wlan_objmgr_vdev *vdev,
					bool he_siga_va15_allowed);

/**
 * wlan_sr_setup_req() - Enable SR with provided pd threshold
 * @vdev: objmgr vdev
 * @pdev: objmgr pdev
 * @is_sr_enable: sr enable/disable
 * @srg_pd_threshold: SRG pd threshold
 * @non_srg_pd_threshold: NON SRG PD threshold
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_sr_setup_req(struct wlan_objmgr_vdev *vdev,
		  struct wlan_objmgr_pdev *pdev, bool is_sr_enable,
		  int32_t srg_pd_threshold, int32_t non_srg_pd_threshold);
#endif
