/*
 * Copyright (c) 2018, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_cp_stats_ucfg_api.h
 *
 * This header file maintain API declaration required for northbound interaction
 */

#ifndef __WLAN_CP_STATS_UCFG_API_H__
#define __WLAN_CP_STATS_UCFG_API_H__

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_utils_api.h>
#include <wlan_cp_stats_chipset_stats.h>
#include "../../core/src/wlan_cp_stats_defs.h"
#include "../../core/src/wlan_cp_stats_cmn_api_i.h"
#include <wlan_cp_stats_chipset_stats.h>

/**
 * ucfg_infra_cp_stats_register_resp_cb() - Register the response callback
 * and cookie in the psoc mc_stats object
 * @psoc: pointer to psoc object
 * @req: pointer to request parameter structure
 *
 * Return: QDF_STATUS_SUCCESS on Success, other QDF_STATUS error codes on
 * failure
 */
QDF_STATUS
ucfg_infra_cp_stats_register_resp_cb(struct wlan_objmgr_psoc *psoc,
				     struct infra_cp_stats_cmd_info *req);

/**
 * ucfg_infra_cp_stats_deregister_resp_cb() - Deregister the response callback
 * and cookie in the psoc mc_stats object
 * @psoc: pointer to psoc object
 *
 * Return: QDF_STATUS_SUCCESS on Success, other QDF_STATUS error codes on
 * failure
 */
QDF_STATUS
ucfg_infra_cp_stats_deregister_resp_cb(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_send_infra_cp_stats_request() - send a infra cp stats command
 * @vdev: pointer to vdev object
 * @req: pointer to request parameter structure
 *
 * Return: QDF_STATUS_SUCCESS on Success, other QDF_STATUS error codes
 * on failure
 */
QDF_STATUS
ucfg_send_infra_cp_stats_request(struct wlan_objmgr_vdev *vdev,
				 struct infra_cp_stats_cmd_info *req);

#ifdef WLAN_CONFIG_TELEMETRY_AGENT
/**
 * ucfg_send_telemetry_cp_stats_request() - send a telemetry cp stats command
 * @pdev: pointer to pdev object
 * @req: pointer to request parameter structure
 *
 * Return: QDF_STATUS_SUCCESS on Success, other QDF_STATUS error codes
 * on failure
 */
QDF_STATUS
ucfg_send_telemetry_cp_stats_request(struct wlan_objmgr_pdev *pdev,
				     struct infra_cp_stats_cmd_info *req);
#endif

#if defined(WLAN_SUPPORT_TWT) && defined (WLAN_TWT_CONV_SUPPORTED)
int ucfg_cp_stats_twt_get_peer_session_params(
					struct wlan_objmgr_psoc *psoc_obj,
					struct twt_session_stats_info *params);
#endif

#ifdef WLAN_CHIPSET_STATS
/**
 * ucfg_cp_stats_get_chipset_stats_enable() - Returns INI CHIPSET_STATS_ENABLE
 *
 * @psoc: psoc object
 *
 * Return: True if Chipset Stats is enabled
 *        False if Chipset Stats is not supported or disabled
 */
bool ucfg_cp_stats_get_chipset_stats_enable(struct wlan_objmgr_psoc *psoc);
#else
static inline
bool ucfg_cp_stats_get_chipset_stats_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif
#endif /* QCA_SUPPORT_CP_STATS */

/**
 * ucfg_cp_stats_cstats_register_tx_rx_ops() - Register chipset stats ops
 *
 * @ops : pointer to tx/rx ops structure
 *
 * Return: void
 */
void ucfg_cp_stats_cstats_register_tx_rx_ops(struct cstats_tx_rx_ops *ops);

/*
 * ucfg_cp_stats_cstats_send_buffer_to_user() - ucfg api to Flush chipset stats
 * to the middleware
 * @type - Type of chipset stats to be sent
 *
 * Return : 0 on success and errno on failure
 */
int ucfg_cp_stats_cstats_send_buffer_to_user(enum cstats_types type);
#endif /* __WLAN_CP_STATS_UCFG_API_H__ */
