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
 * DOC: This file contains all SCAN component's APIs
 */

#include "cfg_ucfg_api.h"
#include "wlan_scan_api.h"
#include "../../core/src/wlan_scan_manager.h"
#ifdef WLAN_POLICY_MGR_ENABLE
#include <wlan_policy_mgr_api.h>
#include "wlan_policy_mgr_public_struct.h"
#endif

void wlan_scan_cfg_get_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					 uint32_t *dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;
	*dwell_time = scan_obj->scan_def.passive_dwell;
}

void wlan_scan_cfg_set_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					 uint32_t dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;
	scan_obj->scan_def.passive_dwell = dwell_time;
}

void wlan_scan_cfg_get_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					uint32_t *dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;
	*dwell_time = scan_obj->scan_def.active_dwell;
}

void wlan_scan_cfg_set_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					uint32_t dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;
	scan_obj->scan_def.active_dwell = dwell_time;
}

void wlan_scan_cfg_get_active_2g_dwelltime(struct wlan_objmgr_psoc *psoc,
					   uint32_t *dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	*dwell_time = scan_obj->scan_def.active_dwell_2g;
}

void wlan_scan_cfg_set_active_2g_dwelltime(struct wlan_objmgr_psoc *psoc,
					   uint32_t dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;
	scan_obj->scan_def.active_dwell_2g = dwell_time;
}

#ifdef CONFIG_BAND_6GHZ
QDF_STATUS wlan_scan_cfg_get_active_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						 uint32_t *dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return QDF_STATUS_E_INVAL;

	*dwell_time = scan_obj->scan_def.active_dwell_6g;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_scan_cfg_set_active_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						 uint32_t dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return QDF_STATUS_E_INVAL;

	scan_obj->scan_def.active_dwell_6g = dwell_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_scan_cfg_get_passive_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						  uint32_t *dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return QDF_STATUS_E_INVAL;

	*dwell_time = scan_obj->scan_def.passive_dwell_6g;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_scan_cfg_set_passive_6g_dwelltime(struct wlan_objmgr_psoc *psoc,
						  uint32_t dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return QDF_STATUS_E_INVAL;

	scan_obj->scan_def.passive_dwell_6g = dwell_time;

	return QDF_STATUS_SUCCESS;
}

void wlan_scan_cfg_get_min_dwelltime_6g(struct wlan_objmgr_psoc *psoc,
					uint32_t *min_dwell_time_6ghz)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;
	*min_dwell_time_6ghz = scan_obj->scan_def.min_dwell_time_6g;
}
#endif

#ifdef WLAN_POLICY_MGR_ENABLE
void wlan_scan_update_pno_dwell_time(struct wlan_objmgr_vdev *vdev,
				     struct pno_scan_req_params *req,
				     struct scan_default_params *scan_def)
{
	bool sap_or_p2p_present;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);

	if (!psoc)
		return;

	sap_or_p2p_present = policy_mgr_mode_specific_connection_count
			       (psoc,
				PM_SAP_MODE, NULL) ||
				policy_mgr_mode_specific_connection_count
			       (psoc,
				PM_P2P_GO_MODE, NULL) ||
				policy_mgr_mode_specific_connection_count
			       (psoc,
				PM_P2P_CLIENT_MODE, NULL);

	if (sap_or_p2p_present) {
		req->active_dwell_time = scan_def->conc_active_dwell;
		req->passive_dwell_time = scan_def->conc_passive_dwell;
	}
}
#endif

void wlan_scan_cfg_get_conc_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					     uint32_t *dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	*dwell_time = scan_obj->scan_def.conc_active_dwell;
}

void wlan_scan_cfg_set_conc_active_dwelltime(struct wlan_objmgr_psoc *psoc,
					     uint32_t dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	scan_obj->scan_def.conc_active_dwell = dwell_time;
}

void wlan_scan_cfg_get_conc_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					      uint32_t *dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	*dwell_time = scan_obj->scan_def.conc_passive_dwell;
}

void wlan_scan_cfg_set_conc_passive_dwelltime(struct wlan_objmgr_psoc *psoc,
					      uint32_t dwell_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	scan_obj->scan_def.conc_passive_dwell = dwell_time;
}

void
wlan_scan_cfg_get_dfs_chan_scan_allowed(struct wlan_objmgr_psoc *psoc,
					bool *enable_dfs_scan)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	*enable_dfs_scan = scan_obj->scan_def.allow_dfs_chan_in_scan;
}

void
wlan_scan_cfg_set_dfs_chan_scan_allowed(struct wlan_objmgr_psoc *psoc,
					bool enable_dfs_scan)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	scan_obj->scan_def.allow_dfs_chan_in_scan = enable_dfs_scan;
}

bool wlan_scan_cfg_honour_nl_scan_policy_flags(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return false;

	return scan_obj->scan_def.honour_nl_scan_policy_flags;
}

void wlan_scan_cfg_get_conc_max_resttime(struct wlan_objmgr_psoc *psoc,
					 uint32_t *rest_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	*rest_time = scan_obj->scan_def.conc_max_rest_time;
}

void wlan_scan_cfg_get_conc_min_resttime(struct wlan_objmgr_psoc *psoc,
					 uint32_t *rest_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return;

	*rest_time = scan_obj->scan_def.conc_min_rest_time;
}

bool wlan_scan_is_snr_monitor_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return cfg_default(CFG_ENABLE_SNR_MONITORING);

	return scan_obj->scan_def.scan_f_chan_stat_evnt;
}

QDF_STATUS
wlan_scan_process_bcn_probe_rx_sync(struct wlan_objmgr_psoc *psoc,
				    qdf_nbuf_t buf,
				    struct mgmt_rx_event_params *rx_param,
				    enum mgmt_frame_type frm_type)
{
	struct scan_bcn_probe_event *bcn = NULL;
	QDF_STATUS status;

	if ((frm_type != MGMT_PROBE_RESP) &&
	    (frm_type != MGMT_BEACON)) {
		scm_err("frame is not beacon or probe resp");
		status = QDF_STATUS_E_INVAL;
		goto free;
	}

	bcn = qdf_mem_malloc_atomic(sizeof(*bcn));
	if (!bcn) {
		status = QDF_STATUS_E_NOMEM;
		goto free;
	}
	bcn->rx_data =
		qdf_mem_malloc_atomic(sizeof(*rx_param));
	if (!bcn->rx_data) {
		status = QDF_STATUS_E_NOMEM;
		goto free;
	}

	if (frm_type == MGMT_PROBE_RESP)
		bcn->frm_type = MGMT_SUBTYPE_PROBE_RESP;
	else
		bcn->frm_type = MGMT_SUBTYPE_BEACON;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_SCAN_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_info("unable to get reference");
		goto free;
	}

	bcn->psoc = psoc;
	bcn->buf = buf;
	qdf_mem_copy(bcn->rx_data, rx_param, sizeof(*rx_param));

	return __scm_handle_bcn_probe(bcn);
free:
	if (bcn && bcn->rx_data)
		qdf_mem_free(bcn->rx_data);
	if (bcn)
		qdf_mem_free(bcn);
	if (buf)
		qdf_nbuf_free(buf);

	return status;
}

qdf_time_t wlan_scan_get_aging_time(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return cfg_default(CFG_SCAN_AGING_TIME) * 1000;

	return scan_obj->scan_def.scan_cache_aging_time;
}

QDF_STATUS wlan_scan_set_aging_time(struct wlan_objmgr_psoc *psoc,
				    qdf_time_t time)
{
	struct wlan_scan_obj *scan_obj;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return status;

	if (!cfg_in_range(CFG_SCAN_AGING_TIME, time / 1000)) {
		status = QDF_STATUS_E_RANGE;
		return status;
	}

	scan_obj->scan_def.scan_cache_aging_time = time;
	status = QDF_STATUS_SUCCESS;
	return status;
}

QDF_STATUS wlan_scan_start(struct scan_start_request *req)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	if (!req || !req->vdev) {
		scm_err("req or vdev within req is NULL");
		scm_scan_free_scan_request_mem(req);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!scm_is_scan_allowed(req->vdev)) {
		scm_err_rl("scan disabled, rejecting the scan req");
		scm_scan_free_scan_request_mem(req);
		return QDF_STATUS_E_AGAIN;
	}

	/*
	 * Try to get vdev reference. Return if reference could
	 * not be taken. Reference will be released once scan
	 * request handling completes along with free of @req.
	 */
	status = wlan_objmgr_vdev_try_get_ref(req->vdev, WLAN_SCAN_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_info("unable to get reference");
		scm_scan_free_scan_request_mem(req);
		return status;
	}

	msg.bodyptr = req;
	msg.callback = scm_scan_start_req;
	msg.flush_callback = scm_scan_start_flush_callback;

	status = scheduler_post_message(QDF_MODULE_ID_OS_IF,
					QDF_MODULE_ID_SCAN,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_SCAN_ID);
		scm_scan_free_scan_request_mem(req);
	}

	return status;
}

QDF_STATUS wlan_scan_cancel(struct scan_cancel_request *req)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	if (!req || !req->vdev) {
		scm_err("req or vdev within req is NULL");
		if (req)
			qdf_mem_free(req);
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = wlan_objmgr_vdev_try_get_ref(req->vdev, WLAN_SCAN_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_info("Failed to get vdev ref; status:%d", status);
		goto req_free;
	}

	msg.bodyptr = req;
	msg.callback = scm_scan_cancel_req;
	msg.flush_callback = scm_scan_cancel_flush_callback;

	status = scheduler_post_message(QDF_MODULE_ID_OS_IF,
					QDF_MODULE_ID_SCAN,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		goto vdev_put;

	return QDF_STATUS_SUCCESS;

vdev_put:
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_SCAN_ID);

req_free:
	qdf_mem_free(req);

	return status;
}

wlan_scan_id
wlan_scan_get_scan_id(struct wlan_objmgr_psoc *psoc)
{
	wlan_scan_id id;
	struct wlan_scan_obj *scan;

	if (!psoc) {
		QDF_ASSERT(0);
		scm_err("null psoc");
		return 0;
	}

	scan = wlan_psoc_get_scan_obj(psoc);
	if (!scan) {
		scm_err("scan object null");
		return 0;
	}

	id = qdf_atomic_inc_return(&scan->scan_ids);
	id =  id & WLAN_SCAN_ID_MASK;
	/* Mark this scan request as triggered by host
	 * by setting WLAN_HOST_SCAN_REQ_ID_PREFIX flag.
	 */
	id =  id | WLAN_HOST_SCAN_REQ_ID_PREFIX;
	scm_debug("scan_id: 0x%x", id);

	return id;
}

QDF_STATUS
wlan_scan_init_default_params(struct wlan_objmgr_vdev *vdev,
			      struct scan_start_request *req)
{
	struct scan_default_params *def;

	if (!vdev | !req) {
		scm_err("vdev: 0x%pK, req: 0x%pK", vdev, req);
		return QDF_STATUS_E_INVAL;
	}
	def = wlan_vdev_get_def_scan_params(vdev);
	if (!def) {
		scm_err("wlan_vdev_get_def_scan_params returned NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* Zero out everything and explicitly set fields as required */
	qdf_mem_zero(req, sizeof(*req));

	req->vdev = vdev;
	req->scan_req.vdev_id = wlan_vdev_get_id(vdev);
	req->scan_req.scan_type = SCAN_TYPE_DEFAULT;
	req->scan_req.scan_priority = def->scan_priority;
	req->scan_req.dwell_time_active = def->active_dwell;
	req->scan_req.dwell_time_active_2g = def->active_dwell_2g;
	req->scan_req.min_dwell_time_6g = def->min_dwell_time_6g;
	req->scan_req.dwell_time_active_6g = def->active_dwell_6g;
	req->scan_req.dwell_time_passive_6g = def->passive_dwell_6g;
	req->scan_req.dwell_time_passive = def->passive_dwell;
	req->scan_req.min_rest_time = def->min_rest_time;
	req->scan_req.max_rest_time = def->max_rest_time;
	req->scan_req.repeat_probe_time = def->repeat_probe_time;
	req->scan_req.probe_spacing_time = def->probe_spacing_time;
	req->scan_req.idle_time = def->idle_time;
	req->scan_req.max_scan_time = def->max_scan_time;
	req->scan_req.probe_delay = def->probe_delay;
	req->scan_req.burst_duration = def->burst_duration;
	req->scan_req.n_probes = def->num_probes;
	req->scan_req.adaptive_dwell_time_mode =
		def->adaptive_dwell_time_mode;
	req->scan_req.scan_flags = def->scan_flags;
	req->scan_req.scan_events = def->scan_events;
	req->scan_req.scan_random.randomize = def->enable_mac_spoofing;

	return QDF_STATUS_SUCCESS;
}

wlan_scan_requester
wlan_scan_register_requester(struct wlan_objmgr_psoc *psoc,
			     uint8_t *name,
			     scan_event_handler event_cb,
			     void *arg)
{
	int i, j;
	struct wlan_scan_obj *scan;
	struct scan_requester_info *requesters;
	wlan_scan_requester requester = {0};

	if (!psoc) {
		scm_err("null psoc");
		return 0;
	}
	scan = wlan_psoc_get_scan_obj(psoc);
	if (!scan)
		return 0;

	requesters = scan->requesters;
	qdf_spin_lock_bh(&scan->lock);
	for (i = 0; i < WLAN_MAX_REQUESTORS; ++i) {
		if (requesters[i].requester == 0) {
			requesters[i].requester =
				WLAN_SCAN_REQUESTER_ID_PREFIX | i;
			j = 0;
			while (name[j] && (j < (WLAN_MAX_MODULE_NAME - 1))) {
				requesters[i].module[j] = name[j];
				++j;
			}
			requesters[i].module[j] = 0;
			requesters[i].ev_handler.func = event_cb;
			requesters[i].ev_handler.arg = arg;
			requester = requesters[i].requester;
			break;
		}
	}
	qdf_spin_unlock_bh(&scan->lock);
	scm_debug("module: %s, event_cb: 0x%pK, arg: 0x%pK, reqid: %d",
		  name, event_cb, arg, requester);

	return requester;
}

void
wlan_scan_unregister_requester(struct wlan_objmgr_psoc *psoc,
			       wlan_scan_requester requester)
{
	int idx;
	struct wlan_scan_obj *scan;
	struct scan_requester_info *requesters;

	idx = requester & WLAN_SCAN_REQUESTER_ID_PREFIX;
	if (idx != WLAN_SCAN_REQUESTER_ID_PREFIX) {
		scm_err("prefix didn't match for requester id %d", requester);
		return;
	}

	idx = requester & WLAN_SCAN_REQUESTER_ID_MASK;
	if (idx >= WLAN_MAX_REQUESTORS) {
		scm_err("requester id %d greater than max value", requester);
		return;
	}

	if (!psoc) {
		scm_err("null psoc");
		return;
	}
	scan = wlan_psoc_get_scan_obj(psoc);
	if (!scan)
		return;
	requesters = scan->requesters;
	scm_debug("reqid: %d", requester);

	qdf_spin_lock_bh(&scan->lock);
	requesters[idx].requester = 0;
	requesters[idx].module[0] = 0;
	requesters[idx].ev_handler.func = NULL;
	requesters[idx].ev_handler.arg = NULL;
	qdf_spin_unlock_bh(&scan->lock);
}

bool wlan_scan_cfg_skip_6g_and_indoor_freq(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj)
		return false;

	return scan_obj->scan_def.skip_6g_and_indoor_freq;
}

void wlan_scan_get_last_scan_ageout_time(struct wlan_objmgr_psoc *psoc,
					 uint32_t *last_scan_ageout_time)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj) {
		*last_scan_ageout_time = 0;
		return;
	}
	*last_scan_ageout_time =
	scan_obj->scan_def.last_scan_ageout_time;
}

#ifdef FEATURE_SET
/**
 * wlan_scan_get_pno_scan_support() - Check if pno scan support is enabled
 * @psoc: pointer to psoc object
 *
 * Return: pno scan_support_enabled flag
 */
static bool wlan_scan_get_pno_scan_support(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj) {
		scm_err("NULL scan obj");
		return cfg_default(CFG_PNO_SCAN_SUPPORT);
	}

	return scan_obj->pno_cfg.scan_support_enabled;
}

/**
 * wlan_scan_is_connected_scan_enabled() - API to get scan enabled after connect
 * @psoc: pointer to psoc object
 *
 * Return: value.
 */
static bool wlan_scan_is_connected_scan_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_scan_obj *scan_obj;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj) {
		scm_err("Failed to get scan object");
		return cfg_default(CFG_ENABLE_CONNECTED_SCAN);
	}

	return scan_obj->scan_def.enable_connected_scan;
}

void wlan_scan_get_feature_info(struct wlan_objmgr_psoc *psoc,
				struct wlan_scan_features *scan_feature_set)
{
	scan_feature_set->pno_in_unassoc_state =
					wlan_scan_get_pno_scan_support(psoc);
	if (scan_feature_set->pno_in_unassoc_state)
		scan_feature_set->pno_in_assoc_state =
				wlan_scan_is_connected_scan_enabled(psoc);
}
#endif

#ifdef WLAN_POLICY_MGR_ENABLE
/**
 * wlan_scan_update_hint_bssid() - Update rnr hint bssid info
 * @psoc: objmgr psoc
 * @req: Scan request
 * @ll_sap_freq: ll sap freq
 *
 * Use to update hint_bssid if low latency Sap is UP
 *
 * Return: void
 */
static void
wlan_scan_update_hint_bssid(struct wlan_objmgr_psoc *psoc,
			    struct scan_start_request *req,
			    qdf_freq_t ll_sap_freq)
{
	struct hint_bssid hint_bssid[WLAN_SCAN_MAX_HINT_BSSID] = {0};
	uint32_t i;
	uint32_t count = 0;
	qdf_freq_t freq;

	if (!req->scan_req.num_hint_bssid)
		return;

	for (i = 0; i < req->scan_req.num_hint_bssid; i++) {
		freq = req->scan_req.hint_bssid[i].freq_flags >> 16;
		if (!freq)
			continue;
		if (!policy_mgr_2_freq_always_on_same_mac(psoc,
							  ll_sap_freq,
							  freq)) {
			qdf_mem_copy(
				&hint_bssid[count].bssid,
				&req->scan_req.hint_bssid[i].bssid,
				sizeof(hint_bssid[i].bssid));
			hint_bssid[count].freq_flags =
				req->scan_req.hint_bssid[i].freq_flags;
			count++;
		}
	}
	qdf_mem_zero(req->scan_req.hint_bssid,
		     sizeof(req->scan_req.hint_bssid));
	if (count)
		qdf_mem_copy(req->scan_req.hint_bssid, hint_bssid,
			     sizeof(hint_bssid));
	req->scan_req.num_hint_bssid = count;
}

/**
 * wlan_scan_update_hint_s_ssid() - Update rnr hint short ssid info
 * @psoc: objmgr psoc
 * @req: Scan request
 * @ll_sap_freq: ll sap freq
 *
 * Use to update hint_s_ssid if low latency Sap is UP
 *
 * Return: void
 */
static
void wlan_scan_update_hint_s_ssid(struct wlan_objmgr_psoc *psoc,
				  struct scan_start_request *req,
				  qdf_freq_t ll_sap_freq)
{
	struct hint_short_ssid hint_s_ssid[WLAN_SCAN_MAX_HINT_BSSID] = {0};
	uint32_t i;
	uint32_t count = 0;
	qdf_freq_t freq;

	if (!req->scan_req.num_hint_s_ssid)
		return;

	for (i = 0; i < req->scan_req.num_hint_s_ssid; i++) {
		freq = req->scan_req.hint_s_ssid[i].freq_flags >> 16;
		if (!freq)
			continue;
		if (!policy_mgr_2_freq_always_on_same_mac(psoc,
							  ll_sap_freq,
							  freq)) {
			qdf_mem_copy(
				&hint_s_ssid[count].short_ssid,
				&req->scan_req.hint_s_ssid[i].short_ssid,
				sizeof(hint_s_ssid[i].short_ssid));
			hint_s_ssid[count].freq_flags =
				req->scan_req.hint_s_ssid[i].freq_flags;
			count++;
		}
	}
	qdf_mem_zero(req->scan_req.hint_s_ssid,
		     sizeof(req->scan_req.hint_s_ssid));
	if (count)
		qdf_mem_copy(req->scan_req.hint_s_ssid, hint_s_ssid,
			     sizeof(hint_s_ssid));
	req->scan_req.num_hint_s_ssid = count;
}

void wlan_scan_update_low_latency_profile_chnlist(
				struct wlan_objmgr_vdev *vdev,
				struct scan_start_request *req)
{
	uint32_t num_scan_channels = 0, i;
	struct wlan_objmgr_psoc *psoc;
	qdf_freq_t freq, ll_sap_freq;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		scm_err("psoc is null");
		return;
	}

	ll_sap_freq = policy_mgr_get_ll_sap_freq(psoc);
	if (!ll_sap_freq)
		return;

	wlan_scan_update_hint_bssid(psoc, req, ll_sap_freq);
	wlan_scan_update_hint_s_ssid(psoc, req, ll_sap_freq);
	/*
	 * Scenario: LL SAP is present and scan is requested.
	 * Allow scan on freq on mutually exclusive mac.
	 */
	for (i = 0; i < req->scan_req.chan_list.num_chan; i++) {
		freq = req->scan_req.chan_list.chan[i].freq;
		if (policy_mgr_2_freq_always_on_same_mac(psoc,
							 ll_sap_freq,
							 freq))
			continue;

		req->scan_req.chan_list.chan[num_scan_channels++] =
					req->scan_req.chan_list.chan[i];
	}
	if (num_scan_channels < req->scan_req.chan_list.num_chan)
		scm_debug("For DBS: only 2.4Ghz chan and for SBS: mutually exclusive ll-sap 5GHz chan allowed, total-chan %d, remaining-chan %d, ll-sap chan %d",
			  req->scan_req.chan_list.num_chan,
			  num_scan_channels,
			  ll_sap_freq);
	req->scan_req.chan_list.num_chan = num_scan_channels;
}
#endif

QDF_STATUS
wlan_scan_get_entry_by_mac_addr(struct wlan_objmgr_pdev *pdev,
				struct qdf_mac_addr *bssid,
				struct element_info *frame)
{
	return scm_scan_get_entry_by_mac_addr(pdev, bssid, frame);
}

QDF_STATUS wlan_scan_register_mbssid_cb(struct wlan_objmgr_psoc *psoc,
					update_mbssid_bcn_prb_rsp cb)
{
	return scm_scan_register_mbssid_cb(psoc, cb);
}

struct scan_cache_entry *
wlan_scan_get_entry_by_bssid(struct wlan_objmgr_pdev *pdev,
			     struct qdf_mac_addr *bssid)
{
	return scm_scan_get_entry_by_bssid(pdev, bssid);
}

QDF_STATUS
wlan_scan_get_mld_addr_by_link_addr(struct wlan_objmgr_pdev *pdev,
				    struct qdf_mac_addr *link_addr,
				    struct qdf_mac_addr *mld_mac_addr)
{
	return scm_get_mld_addr_by_link_addr(pdev, link_addr, mld_mac_addr);
}

QDF_STATUS
wlan_scan_get_scan_entry_by_mac_freq(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr *bssid,
				     uint16_t freq,
				     struct scan_cache_entry
				     *cache_entry)
{
	return scm_scan_get_scan_entry_by_mac_freq(pdev, bssid, freq,
						   cache_entry);
}
