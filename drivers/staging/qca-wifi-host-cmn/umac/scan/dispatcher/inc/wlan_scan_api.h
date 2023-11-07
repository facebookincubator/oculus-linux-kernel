/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: contains scan api
 */

#ifndef _WLAN_SCAN_API_H_
#define _WLAN_SCAN_API_H_

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include "../../core/src/wlan_scan_main.h"

#ifdef FEATURE_SET
/**
 * wlan_scan_get_feature_info() - Get scan feature set info
 * @psoc: pointer to psoc object
 * @scan_feature_set: feature set info which needs to be filled
 *
 * Return: none
 */
void wlan_scan_get_feature_info(struct wlan_objmgr_psoc *psoc,
				struct wlan_scan_features *scan_feature_set);
#endif

/**
 * wlan_scan_get_scan_entry_by_mac_freq() - API to get scan entry
 * info from scan db by mac addr
 * @pdev: pointer to pdev object
 * @bssid: pointer to mac addr
 * @freq: frequency for scan filter
 * @cache_entry: pointer to scan cache_entry
 *
 * Return: success if scan entry is found in scan db
 */
QDF_STATUS
wlan_scan_get_scan_entry_by_mac_freq(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr *bssid,
				     uint16_t freq,
				     struct scan_cache_entry
				     *cache_entry);

/**
 * wlan_scan_cfg_set_active_2g_dwelltime() - API to set scan active 2g dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwell time
 *
 * Return: none
 */
void wlan_scan_cfg_set_active_2g_dwelltime(struct wlan_objmgr_psoc *psoc,
					   uint32_t dwell_time);

/**
 * wlan_scan_cfg_get_active_2g_dwelltime() - API to get active 2g dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwelltime
 *
 * Return: scan active dwell time
 */
void wlan_scan_cfg_get_active_2g_dwelltime(struct wlan_objmgr_psoc *psoc,
					   uint32_t *dwell_time);

#ifdef CONFIG_BAND_6GHZ
/**
 * wlan_scan_cfg_set_active_6g_dwelltime() - API to set scan active 6g dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwell time
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_scan_cfg_set_active_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						 uint32_t dwell_time);

/**
 * wlan_scan_cfg_get_active_6g_dwelltime() - API to get active 6g dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwelltime
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_scan_cfg_get_active_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						 uint32_t *dwell_time);

/**
 * wlan_scan_cfg_set_passive_6g_dwelltime() - API to set scan passive 6g
 *                                            dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan passive dwell time
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_scan_cfg_set_passive_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						  uint32_t dwell_time);

/**
 * wlan_scan_cfg_get_passive_6g_dwelltime() - API to get passive 6g dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan passive dwelltime
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_scan_cfg_get_passive_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						  uint32_t *dwell_time);

/**
 * wlan_scan_cfg_get_min_dwelltime_6g() - API to get minimum 6g dwelltime
 * @psoc: pointer to psoc object
 * @min_dwell_time_6ghz: minimum dwelltime 6g
 *
 * Return: QDF_STATUS
 */
void wlan_scan_cfg_get_min_dwelltime_6g(struct wlan_objmgr_psoc *psoc,
					uint32_t *min_dwell_time_6ghz);
#else
static inline
void wlan_scan_cfg_get_min_dwelltime_6g(struct wlan_objmgr_psoc *psoc,
					uint32_t *min_dwell_time_6ghz)
{
}
#endif

/**
 * wlan_scan_cfg_set_active_dwelltime() - API to set scan active dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwell time
 *
 * Return: none
 */
void wlan_scan_cfg_set_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					uint32_t dwell_time);
/**
 * wlan_scan_cfg_get_active_dwelltime() - API to get active dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwelltime
 *
 * Return: scan active dwell time
 */
void wlan_scan_cfg_get_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					uint32_t *dwell_time);

/**
 * wlan_scan_cfg_set_passive_dwelltime() - API to set scan passive dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan passive dwell time
 *
 * Return: none
 */
void wlan_scan_cfg_set_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					 uint32_t dwell_time);
/**
 * wlan_scan_cfg_get_passive_dwelltime() - API to get passive dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan passive dwelltime
 *
 * Return: scan passive dwell time
 */
void wlan_scan_cfg_get_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					 uint32_t *dwell_time);

#ifdef WLAN_POLICY_MGR_ENABLE
/*
 * wlan_scan_update_pno_dwell_time() - update active and passive dwell time
 * depending on active concurrency modes
 * @vdev: vdev object pointer
 * @req: scan request
 *
 * Return: void
 */
void wlan_scan_update_pno_dwell_time(struct wlan_objmgr_vdev *vdev,
				     struct pno_scan_req_params *req,
				     struct scan_default_params *scan_def);

/*
 * wlan_scan_update_low_latency_profile_chnlist() - Low latency SAP + scan
 * concurrencies
 * @vdev: vdev object pointer
 * @req: scan request
 *
 * Return: void
 */
void wlan_scan_update_low_latency_profile_chnlist(
				struct wlan_objmgr_vdev *vdev,
				struct scan_start_request *req);
#else
static inline
void wlan_scan_update_low_latency_profile_chnlist(
				struct wlan_objmgr_vdev *vdev,
				struct scan_start_request *req)
{
}

#endif

/**
 * wlan_scan_cfg_get_conc_active_dwelltime() - Get concurrent active dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwelltime
 *
 * Return: scan concurrent active dwell time
 */
void wlan_scan_cfg_get_conc_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					     uint32_t *dwell_time);

/**
 * wlan_scan_cfg_set_conc_active_dwelltime() - Set concurrent active dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan active dwelltime
 *
 * Return: scan concurrent active dwell time
 */
void wlan_scan_cfg_set_conc_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					     uint32_t dwell_time);

/**
 * wlan_scan_cfg_get_conc_passive_dwelltime() - Get passive concurrent dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan passive dwelltime
 *
 * Return: scan concurrent passive dwell time
 */
void wlan_scan_cfg_get_conc_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					      uint32_t *dwell_time);

/**
 * wlan_scan_cfg_set_conc_passive_dwelltime() - Set passive concurrent dwelltime
 * @psoc: pointer to psoc object
 * @dwell_time: scan passive dwelltime
 *
 * Return: scan concurrent passive dwell time
 */
void wlan_scan_cfg_set_conc_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					      uint32_t dwell_time);

/**
 * wlan_scan_cfg_honour_nl_scan_policy_flags() - API to get nl scan policy
 * flags honoured
 * @psoc: pointer to psoc object
 *
 * Return: nl scan policy flags honoured or not
 */
bool wlan_scan_cfg_honour_nl_scan_policy_flags(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_scan_cfg_get_conc_max_resttime() - API to get max rest time
 * @psoc: pointer to psoc object
 * @rest_time: scan concurrent max resttime
 *
 * Return: scan concurrent max rest time
 */
void wlan_scan_cfg_get_conc_max_resttime(struct wlan_objmgr_psoc *psoc,
					 uint32_t *rest_time);

/**
 * wlan_scan_cfg_get_dfs_chan_scan_allowed() - API to get dfs scan enabled
 * @psoc: pointer to psoc object
 * @enable_dfs_scan: DFS scan enabled or not.
 *
 * Return: None
 */
void wlan_scan_cfg_get_dfs_chan_scan_allowed(struct wlan_objmgr_psoc *psoc,
					     bool *enable_dfs_scan);

/**
 * wlan_scan_cfg_set_dfs_chan_scan_allowed() - API to set dfs scan enabled.
 * @psoc: pointer to psoc object
 * @enable_dfs_scan: Set dfs scan enabled or not.
 *
 * Return: None
 */
void wlan_scan_cfg_set_dfs_chan_scan_allowed(struct wlan_objmgr_psoc *psoc,
					     bool enable_dfs_scan);

/**
 * wlan_scan_cfg_get_conc_min_resttime() - API to get concurrent min rest time
 * @psoc: pointer to psoc object
 * @rest_time: scan concurrent min rest time
 *
 * Return: scan concurrent min rest time
 */
void wlan_scan_cfg_get_conc_min_resttime(struct wlan_objmgr_psoc *psoc,
					 uint32_t *rest_time);

/**
 * wlan_scan_is_snr_monitor_enabled() - API to get SNR monitoring enabled or not
 * @psoc: pointer to psoc object
 *
 * Return: enable/disable snr monitor mode.
 */
bool wlan_scan_is_snr_monitor_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_scan_process_bcn_probe_rx_sync() - handle bcn without posting to
 * scheduler thread
 * @psoc: psoc context
 * @buf: frame buf
 * @rx_param: rx event params
 * @frm_type: frame type
 *
 * handle bcn without posting to scheduler thread, this should be called
 * while caller is already in scheduler thread context
 *
 * Return: success or error code.
 */
QDF_STATUS
wlan_scan_process_bcn_probe_rx_sync(struct wlan_objmgr_psoc *psoc,
				    qdf_nbuf_t buf,
				    struct mgmt_rx_event_params *rx_param,
				    enum mgmt_frame_type frm_type);

/**
 * wlan_scan_get_aging_time  - Get the scan aging time config
 * @psoc: psoc context
 *
 * Return: Scan aging time config
 */
qdf_time_t wlan_scan_get_aging_time(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_scan_set_aging_time  - Set the scan aging time config
 * @psoc: psoc context
 * @time: scan aging time
 *
 * Return: success or error code.
 */
QDF_STATUS wlan_scan_set_aging_time(struct wlan_objmgr_psoc *psoc,
				    qdf_time_t time);

/**
 * wlan_scan_purge_results() - purge the scan list
 * @scan_list: scan list to be purged
 *
 * This function purge the temp scan list
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS wlan_scan_purge_results(qdf_list_t *scan_list)
{
	return scm_purge_scan_results(scan_list);
}

/**
 * wlan_scan_get_result() - The Public API to get scan results
 * @pdev: pdev info
 * @filter: Filters
 *
 * This function fetches scan result
 *
 * Return: scan list pointer
 */
static inline qdf_list_t *wlan_scan_get_result(struct wlan_objmgr_pdev *pdev,
					       struct scan_filter *filter)
{
	return scm_get_scan_result(pdev, filter);
}

/**
 * wlan_scan_update_mlme_by_bssinfo() - The Public API to update mlme
 * info in the scan entry
 * @pdev: pdev object
 * @bss_info: bssid info to find the matching scan entry
 * @mlme_info: mlme info to be updated.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
wlan_scan_update_mlme_by_bssinfo(struct wlan_objmgr_pdev *pdev,
				 struct bss_info *bss_info,
				 struct mlme_info *mlme_info)
{
	return scm_scan_update_mlme_by_bssinfo(pdev, bss_info, mlme_info);
}

/**
 * wlan_scan_start() - Public API to start a scan
 * @req: start scan req params
 *
 * The Public API to start a scan. Post a msg to target_if queue
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wlan_scan_start(struct scan_start_request *req);

/**
 * wlan_scan_cancel() - Public API to stop a scan
 * @req: stop scan request params
 *
 * The Public API to stop a scan. Post a msg to target_if queue
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wlan_scan_cancel(struct scan_cancel_request *req);

/**
 * wlan_scan_get_scan_id() - Public API to allocate scan ID
 * @psoc: psoc object
 *
 * Public API, allocates a new scan id for caller
 *
 * Return: newly allocated scan ID
 */
wlan_scan_id
wlan_scan_get_scan_id(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_scan_init_default_params() - Public API to initialize scan params
 * @vdev: vdev object
 * @req: scan request object
 *
 * Public API to initialize scan start request with defaults scan params
 *
 * Return: QDF_STATUS_SUCCESS or error code
 */
QDF_STATUS
wlan_scan_init_default_params(struct wlan_objmgr_vdev *vdev,
			      struct scan_start_request *req);

/**
 * wlan_scan_register_requester() - Public API, assigns requester ID
 * to caller and registers scan event call back handler
 * @psoc:       psoc object
 * @module_name:name of requester module
 * @event_cb:   event callback function pointer
 * @arg:        argument to @event_cb
 *
 * API, allows other components to allocate requester id.
 * Normally used by modules at init time to register their callback
 * and get one requester id. @event_cb will be invoked for
 * all scan events whose requester id matches with @requester.
 *
 * Return: assigned non zero requester id for success
 *         zero (0) for failure
 */
wlan_scan_requester
wlan_scan_register_requester(struct wlan_objmgr_psoc *psoc,
			     uint8_t *module_name,
			     scan_event_handler event_cb,
			     void *arg);

/**
 * wlan_scan_unregister_requester() -Public API, reclaims previously
 * allocated requester ID
 * @psoc:       psoc object
 * @requester:  requester ID to reclaim.
 *
 * API, reclaims previously allocated requester id.
 *
 * Return: void
 */
void
wlan_scan_unregister_requester(struct wlan_objmgr_psoc *psoc,
			       wlan_scan_requester requester);

/**
 * wlan_scan_cfg_skip_6g_and_indoor_freq() - API to get 6g and indoor freq
 * scan ini val
 * @psoc: psoc object
 *
 * Return: skip 6g and indoor freq scan or not
 */
bool wlan_scan_cfg_skip_6g_and_indoor_freq(
			struct wlan_objmgr_psoc *psoc);

/**
 * wlan_scan_register_mbssid_cb() - register api to inform bcn/probe rsp
 * @psoc: psoc object
 * @cb: callback to be registered
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_scan_register_mbssid_cb(struct wlan_objmgr_psoc *psoc,
					update_mbssid_bcn_prb_rsp cb);

/**
 * wlan_scan_get_entry_by_mac_addr() - Get bcn/probe rsp from scan db
 * @pdev: pdev info
 * @bssid: BSSID of the bcn/probe response to be fetched from scan db
 * @frame: Frame from scan db with given bssid.
 *
 * This is a wrapper to fetch the bcn/probe rsp frame with given mac address
 * through scm_scan_get_entry_by_mac_addr(). scm_scan_get_entry_by_mac_add()
 * allocates memory for the frame and it's caller responsibility to free
 * the memory once it's done with the usage i.e. frame->ptr.
 *
 * Return: QDF_STATUS_SUCCESS if scan entry is present in db
 */
QDF_STATUS
wlan_scan_get_entry_by_mac_addr(struct wlan_objmgr_pdev *pdev,
				struct qdf_mac_addr *bssid,
				struct element_info *frame);

/**
 * wlan_scan_get_entry_by_bssid() - function to get scan entry by bssid
 * @pdev: pdev object
 * @bssid: bssid to be fetched from scan db
 *
 * Return : scan entry if found, else NULL
 */
struct scan_cache_entry *
wlan_scan_get_entry_by_bssid(struct wlan_objmgr_pdev *pdev,
			     struct qdf_mac_addr *bssid);
/**
 * wlan_scan_get_last_scan_ageout_time() - API to get last scan
 * ageout time
 * @psoc: psoc object
 * @last_scan_ageout_time: last scan ageout time
 *
 * Return: void
 */
void
wlan_scan_get_last_scan_ageout_time(struct wlan_objmgr_psoc *psoc,
				    uint32_t *last_scan_ageout_time);
/**
 * wlan_scan_get_mld_addr_by_link_addr() - Function to get MLD address
 * in the scan entry from the link BSSID.
 * @pdev: pdev object
 * @link_addr: Link BSSID to match the scan filter
 * @mld_mac_addr: Pointer to fill the MLD address.
 *
 * A wrapper API which fills @mld_mac_addr with MLD address of scan entry
 * whose bssid field matches @link_addr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_scan_get_mld_addr_by_link_addr(struct wlan_objmgr_pdev *pdev,
				    struct qdf_mac_addr *link_addr,
				    struct qdf_mac_addr *mld_mac_addr);
#endif
