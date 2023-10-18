/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: declare internal APIs related to the denylist component
 */

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_dlm_core.h>
#include <qdf_mc_timer.h>
#include <wlan_scan_public_structs.h>
#include <wlan_scan_utils_api.h>
#include "wlan_dlm_tgt_api.h"
#include <wlan_cm_bss_score_param.h>

#define SECONDS_TO_MS(params)       ((params) * 1000)
#define MINUTES_TO_MS(params)       (SECONDS_TO_MS(params) * 60)
#define RSSI_TIMEOUT_VALUE          60

static void
dlm_update_ap_info(struct dlm_reject_ap *dlm_entry, struct dlm_config *cfg,
		   struct scan_cache_entry *scan_entry)
{
	qdf_time_t cur_timestamp = qdf_mc_timer_get_system_time();
	qdf_time_t entry_add_time = 0;
	bool update_done = false;
	uint8_t old_reject_ap_type;

	old_reject_ap_type = dlm_entry->reject_ap_type;

	if (DLM_IS_AP_AVOIDED_BY_USERSPACE(dlm_entry)) {
		entry_add_time =
			dlm_entry->ap_timestamp.userspace_avoid_timestamp;

		if ((cur_timestamp - entry_add_time) >=
		     MINUTES_TO_MS(cfg->avoid_list_exipry_time)) {
			/* Move AP to monitor list as avoid list time is over */
			dlm_entry->userspace_avoidlist = false;
			dlm_entry->avoid_userspace = false;
			dlm_entry->driver_monitorlist = true;

			dlm_entry->ap_timestamp.driver_monitor_timestamp =
								cur_timestamp;
			dlm_debug("Userspace avoid list timer expired, moved to monitor list");
			update_done = true;
		}
	}

	if (DLM_IS_AP_AVOIDED_BY_DRIVER(dlm_entry)) {
		entry_add_time = dlm_entry->ap_timestamp.driver_avoid_timestamp;

		if ((cur_timestamp - entry_add_time) >=
		     MINUTES_TO_MS(cfg->avoid_list_exipry_time)) {
			/* Move AP to monitor list as avoid list time is over */
			dlm_entry->driver_avoidlist = false;
			dlm_entry->nud_fail = false;
			dlm_entry->sta_kickout = false;
			dlm_entry->ho_fail = false;
			dlm_entry->driver_monitorlist = true;

			dlm_entry->ap_timestamp.driver_monitor_timestamp =
								cur_timestamp;
			dlm_debug("Driver avoid list timer expired, moved to monitor list");
			update_done = true;
		}
	}

	if (DLM_IS_AP_DENYLISTED_BY_DRIVER(dlm_entry)) {
		entry_add_time =
			dlm_entry->ap_timestamp.driver_denylist_timestamp;

		if ((cur_timestamp - entry_add_time) >=
		     MINUTES_TO_MS(cfg->deny_list_exipry_time)) {
			/* Move AP to monitor list as deny list time is over */
			dlm_entry->driver_denylist = false;
			dlm_entry->driver_monitorlist = true;
			dlm_entry->nud_fail = false;
			dlm_entry->sta_kickout = false;
			dlm_entry->ho_fail = false;
			dlm_entry->ap_timestamp.driver_monitor_timestamp =
								cur_timestamp;
			dlm_debug("Driver denylist timer expired, moved to monitor list");
			update_done = true;
		}
	}

	if (DLM_IS_AP_IN_RSSI_REJECT_LIST(dlm_entry)) {
		qdf_time_t entry_age = cur_timestamp -
			    dlm_entry->ap_timestamp.rssi_reject_timestamp;

		if ((dlm_entry->rssi_reject_params.retry_delay &&
		     entry_age >= dlm_entry->rssi_reject_params.retry_delay) ||
		    (scan_entry && scan_entry->rssi_raw >=
		      dlm_entry->rssi_reject_params.expected_rssi)) {
			/*
			 * Remove from the rssi reject list as:-
			 * 1. In case of OCE reject, both the time, and RSSI
			 *    param are present, and one of them have improved
			 *    now, so the STA can now connect to the AP.
			 *
			 * 2. In case of BTM message received from the FW,
			 *    the STA just needs to wait for a certain time,
			 *    hence RSSI is not a restriction (MIN RSSI needed
			 *    in that case is filled as 0).
			 *    Hence the above check will still pass, if BTM
			 *    delay is over, and will fail is not. RSSI check
			 *    for BTM message will fail (expected), as BTM does
			 *    not care about the same.
			 */
			dlm_entry->poor_rssi = false;
			dlm_entry->oce_assoc_reject = false;
			dlm_entry->btm_bss_termination = false;
			dlm_entry->btm_disassoc_imminent = false;
			dlm_entry->btm_mbo_retry = false;
			dlm_entry->no_more_stas = false;
			dlm_entry->reassoc_rssi_reject = false;
			dlm_entry->rssi_reject_list = false;
			dlm_debug("Remove BSSID from rssi reject expected RSSI = %d, current RSSI = %d, retry delay required = %d ms, delay = %lu ms",
				  dlm_entry->rssi_reject_params.expected_rssi,
				  scan_entry ? scan_entry->rssi_raw : 0,
				  dlm_entry->rssi_reject_params.retry_delay,
				  entry_age);
			update_done = true;
		}
	}

	if (!update_done)
		return;

	dlm_debug(QDF_MAC_ADDR_FMT " Old %d Updated reject ap type = %x",
		  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes),
		  old_reject_ap_type,
		  dlm_entry->reject_ap_type);
}

#define MAX_BL_TIME 255000

static enum cm_denylist_action
dlm_prune_old_entries_and_get_action(struct dlm_reject_ap *dlm_entry,
				     struct dlm_config *cfg,
				     struct scan_cache_entry *entry,
				     qdf_list_t *reject_ap_list)
{
	dlm_update_ap_info(dlm_entry, cfg, entry);

	/*
	 * If all entities have cleared the bits of reject ap type, then
	 * the AP is not needed in the database,(reject_ap_type should be 0),
	 * then remove the entry from the reject ap list.
	 */
	if (!dlm_entry->reject_ap_type) {
		dlm_debug(QDF_MAC_ADDR_FMT " cleared from list",
			  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
		qdf_list_remove_node(reject_ap_list, &dlm_entry->node);
		qdf_mem_free(dlm_entry);
		return CM_DLM_NO_ACTION;
	}

	if (DLM_IS_AP_IN_RSSI_REJECT_LIST(dlm_entry) &&
	    !dlm_entry->userspace_denylist && !dlm_entry->driver_denylist &&
	    dlm_entry->rssi_reject_params.original_timeout > MAX_BL_TIME) {
		dlm_info("Allow BSSID " QDF_MAC_ADDR_FMT " as the retry delay is greater than %u ms, expected RSSI = %d, current RSSI = %d, retry delay = %u ms original timeout %u time added %lu source %d reason %d",
			 QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes), MAX_BL_TIME,
			 dlm_entry->rssi_reject_params.expected_rssi,
			 entry ? entry->rssi_raw : 0,
			 dlm_entry->rssi_reject_params.retry_delay,
			 dlm_entry->rssi_reject_params.original_timeout,
			 dlm_entry->rssi_reject_params.received_time,
			 dlm_entry->source, dlm_entry->reject_ap_reason);

		if (DLM_IS_AP_IN_AVOIDLIST(dlm_entry)) {
			dlm_debug(QDF_MAC_ADDR_FMT " in avoid list, deprioritize it",
				  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
			return CM_DLM_AVOID;
		}

		return CM_DLM_NO_ACTION;
	}
	if (DLM_IS_AP_IN_DENYLIST(dlm_entry)) {
		dlm_debug(QDF_MAC_ADDR_FMT " in denylist list, reject ap type %d removing from candidate list",
			  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes),
			  dlm_entry->reject_ap_type);

		if (DLM_IS_AP_DENYLISTED_BY_USERSPACE(dlm_entry) ||
		    DLM_IS_AP_IN_RSSI_REJECT_LIST(dlm_entry))
			return CM_DLM_FORCE_REMOVE;

		return CM_DLM_REMOVE;
	}

	if (DLM_IS_AP_IN_AVOIDLIST(dlm_entry)) {
		dlm_debug(QDF_MAC_ADDR_FMT " in avoid list, deprioritize it",
			  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
		return CM_DLM_AVOID;
	}

	return CM_DLM_NO_ACTION;
}

static enum cm_denylist_action
dlm_action_on_bssid(struct wlan_objmgr_pdev *pdev,
		    struct scan_cache_entry *entry)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	struct dlm_config *cfg;
	struct dlm_reject_ap *dlm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;
	enum cm_denylist_action action = CM_DLM_NO_ACTION;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));
	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return CM_DLM_NO_ACTION;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return CM_DLM_NO_ACTION;
	}

	cfg = &dlm_psoc_obj->dlm_cfg;

	qdf_list_peek_front(&dlm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&dlm_ctx->reject_ap_list, cur_node,
				   &next_node);

		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);

		if (qdf_is_macaddr_equal(&dlm_entry->bssid, &entry->bssid)) {
			action = dlm_prune_old_entries_and_get_action(dlm_entry,
					cfg, entry, &dlm_ctx->reject_ap_list);
			qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
			return action;
		}
		cur_node = next_node;
		next_node = NULL;
	}
	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);

	return CM_DLM_NO_ACTION;
}

enum cm_denylist_action
wlan_denylist_action_on_bssid(struct wlan_objmgr_pdev *pdev,
			      struct scan_cache_entry *entry)
{
	return dlm_action_on_bssid(pdev, entry);
}

static void
dlm_update_avoidlist_reject_reason(struct dlm_reject_ap *entry,
				   enum dlm_reject_ap_reason reject_reason)
{
	entry->nud_fail = false;
	entry->sta_kickout = false;
	entry->ho_fail = false;

	switch (reject_reason) {
	case REASON_NUD_FAILURE:
		entry->nud_fail = true;
		break;
	case REASON_STA_KICKOUT:
		entry->sta_kickout = true;
		break;
	case REASON_ROAM_HO_FAILURE:
		entry->ho_fail = true;
		break;
	default:
		dlm_err("Invalid reason passed %d", reject_reason);
	}
}

static void
dlm_handle_avoid_list(struct dlm_reject_ap *entry,
		      struct dlm_config *cfg,
		      struct reject_ap_info *ap_info)
{
	qdf_time_t cur_timestamp = qdf_mc_timer_get_system_time();

	if (ap_info->reject_ap_type == USERSPACE_AVOID_TYPE) {
		entry->userspace_avoidlist = true;
		entry->avoid_userspace = true;
		entry->ap_timestamp.userspace_avoid_timestamp = cur_timestamp;
	} else if (ap_info->reject_ap_type == DRIVER_AVOID_TYPE) {
		entry->driver_avoidlist = true;
		dlm_update_avoidlist_reject_reason(entry,
						   ap_info->reject_reason);
		entry->ap_timestamp.driver_avoid_timestamp = cur_timestamp;
	} else {
		return;
	}
	entry->source = ap_info->source;
	/* Update bssid info for new entry */
	entry->bssid = ap_info->bssid;

	/* Clear the monitor list bit if the AP was present in monitor list */
	entry->driver_monitorlist = false;

	/* Increment bad bssid counter as NUD failure happenend with this ap */
	entry->bad_bssid_counter++;

	/* If bad bssid counter has reached threshold, move it to denylist */
	if (entry->bad_bssid_counter >= cfg->bad_bssid_counter_thresh) {
		if (ap_info->reject_ap_type == USERSPACE_AVOID_TYPE)
			entry->userspace_avoidlist = false;
		else if (ap_info->reject_ap_type == DRIVER_AVOID_TYPE)
			entry->driver_avoidlist = false;

		/* Move AP to denylist list */
		entry->driver_denylist = true;
		entry->ap_timestamp.driver_denylist_timestamp = cur_timestamp;

		dlm_debug(QDF_MAC_ADDR_FMT " moved to deny list with counter %d",
			  QDF_MAC_ADDR_REF(entry->bssid.bytes),
			  entry->bad_bssid_counter);
		return;
	}
	dlm_debug("Added " QDF_MAC_ADDR_FMT " to avoid list type %d, counter %d reason %d updated reject reason %d source %d",
		  QDF_MAC_ADDR_REF(entry->bssid.bytes), ap_info->reject_ap_type,
		  entry->bad_bssid_counter, ap_info->reject_reason,
		  entry->reject_ap_reason, entry->source);

	entry->connect_timestamp = qdf_mc_timer_get_system_time();
}

static void
dlm_handle_denylist(struct dlm_reject_ap *entry,
		    struct reject_ap_info *ap_info)
{
	/*
	 * No entity will denylist an AP internal to driver, so only
	 * userspace denylist is the case to be taken care. Driver denylist
	 * will only happen when the bad bssid counter has reached the max
	 * threshold.
	 */
	entry->bssid = ap_info->bssid;
	entry->userspace_denylist = true;
	entry->ap_timestamp.userspace_denylist_timestamp =
						qdf_mc_timer_get_system_time();

	entry->source = ADDED_BY_DRIVER;
	entry->denylist_userspace = true;
	dlm_debug(QDF_MAC_ADDR_FMT " added to userspace denylist",
		  QDF_MAC_ADDR_REF(entry->bssid.bytes));
}

static void
dlm_update_rssi_reject_reason(struct dlm_reject_ap *entry,
			      enum dlm_reject_ap_reason reject_reason)
{
	entry->poor_rssi = false;
	entry->oce_assoc_reject = false;
	entry->btm_bss_termination = false;
	entry->btm_disassoc_imminent = false;
	entry->btm_mbo_retry = false;
	entry->no_more_stas = false;
	entry->reassoc_rssi_reject = false;

	switch (reject_reason) {
	case REASON_ASSOC_REJECT_POOR_RSSI:
		entry->poor_rssi = true;
		break;
	case REASON_ASSOC_REJECT_OCE:
		entry->oce_assoc_reject = true;
		break;
	case REASON_BTM_DISASSOC_IMMINENT:
		entry->btm_disassoc_imminent = true;
		break;
	case REASON_BTM_BSS_TERMINATION:
		entry->btm_bss_termination = true;
		break;
	case REASON_BTM_MBO_RETRY:
		entry->btm_mbo_retry = true;
		break;
	case REASON_REASSOC_RSSI_REJECT:
		entry->reassoc_rssi_reject = true;
		break;
	case REASON_REASSOC_NO_MORE_STAS:
		entry->no_more_stas = true;
		break;
	default:
		dlm_err("Invalid reason passed %d", reject_reason);
	}
}

static void
dlm_handle_rssi_reject_list(struct dlm_reject_ap *entry,
			    struct reject_ap_info *ap_info)
{
	bool bssid_newly_added;

	if (entry->rssi_reject_list) {
		bssid_newly_added = false;
	} else {
		entry->rssi_reject_params.source = ap_info->source;
		entry->bssid = ap_info->bssid;
		entry->rssi_reject_list = true;
		bssid_newly_added = true;
	}

	entry->ap_timestamp.rssi_reject_timestamp =
					qdf_mc_timer_get_system_time();
	entry->rssi_reject_params = ap_info->rssi_reject_params;
	dlm_update_rssi_reject_reason(entry, ap_info->reject_reason);
	dlm_info(QDF_MAC_ADDR_FMT " %s to rssi reject list, expected RSSI %d retry delay %u source %d original timeout %u received time %lu reject reason %d updated reason %d",
		 QDF_MAC_ADDR_REF(entry->bssid.bytes),
		 bssid_newly_added ? "ADDED" : "UPDATED",
		 entry->rssi_reject_params.expected_rssi,
		 entry->rssi_reject_params.retry_delay,
		 entry->rssi_reject_params.source,
		 entry->rssi_reject_params.original_timeout,
		 entry->rssi_reject_params.received_time,
		 ap_info->reject_reason, entry->reject_ap_reason);
}

static void
dlm_modify_entry(struct dlm_reject_ap *entry, struct dlm_config *cfg,
		 struct reject_ap_info *ap_info)
{
	/* Modify the entry according to the ap_info */
	switch (ap_info->reject_ap_type) {
	case USERSPACE_AVOID_TYPE:
	case DRIVER_AVOID_TYPE:
		dlm_handle_avoid_list(entry, cfg, ap_info);
		break;
	case USERSPACE_DENYLIST_TYPE:
		dlm_handle_denylist(entry, ap_info);
		break;
	case DRIVER_RSSI_REJECT_TYPE:
		dlm_handle_rssi_reject_list(entry, ap_info);
		break;
	default:
		dlm_debug("Invalid input of ap type %d",
			  ap_info->reject_ap_type);
	}
}

static bool
dlm_is_bssid_present_only_in_list_type(enum dlm_reject_ap_type list_type,
				       struct dlm_reject_ap *dlm_entry)
{
	switch (list_type) {
	case USERSPACE_AVOID_TYPE:
		return IS_AP_IN_USERSPACE_AVOID_LIST_ONLY(dlm_entry);
	case USERSPACE_DENYLIST_TYPE:
		return IS_AP_IN_USERSPACE_DENYLIST_ONLY(dlm_entry);
	case DRIVER_AVOID_TYPE:
		return IS_AP_IN_DRIVER_AVOID_LIST_ONLY(dlm_entry);
	case DRIVER_DENYLIST_TYPE:
		return IS_AP_IN_DRIVER_DENYLIST_ONLY(dlm_entry);
	case DRIVER_RSSI_REJECT_TYPE:
		return IS_AP_IN_RSSI_REJECT_LIST_ONLY(dlm_entry);
	case DRIVER_MONITOR_TYPE:
		return IS_AP_IN_MONITOR_LIST_ONLY(dlm_entry);
	default:
		dlm_debug("Wrong list type %d passed", list_type);
		return false;
	}
}

static bool
dlm_is_bssid_of_type(enum dlm_reject_ap_type reject_ap_type,
		     struct dlm_reject_ap *dlm_entry)
{
	switch (reject_ap_type) {
	case USERSPACE_AVOID_TYPE:
		return DLM_IS_AP_AVOIDED_BY_USERSPACE(dlm_entry);
	case USERSPACE_DENYLIST_TYPE:
		return DLM_IS_AP_DENYLISTED_BY_USERSPACE(dlm_entry);
	case DRIVER_AVOID_TYPE:
		return DLM_IS_AP_AVOIDED_BY_DRIVER(dlm_entry);
	case DRIVER_DENYLIST_TYPE:
		return DLM_IS_AP_DENYLISTED_BY_DRIVER(dlm_entry);
	case DRIVER_RSSI_REJECT_TYPE:
		return DLM_IS_AP_IN_RSSI_REJECT_LIST(dlm_entry);
	case DRIVER_MONITOR_TYPE:
		return DLM_IS_AP_IN_MONITOR_LIST(dlm_entry);
	default:
		dlm_err("Wrong list type %d passed", reject_ap_type);
		return false;
	}
}

static qdf_time_t
dlm_get_delta_of_bssid(enum dlm_reject_ap_type list_type,
		       struct dlm_reject_ap *dlm_entry,
		       struct dlm_config *cfg)
{
	qdf_time_t cur_timestamp = qdf_mc_timer_get_system_time();
	int32_t disallowed_time;
	/*
	 * For all the list types, delta would be the entry age only. Hence the
	 * oldest entry would be removed first in case of list is full, and the
	 * driver needs to make space for newer entries.
	 */

	switch (list_type) {
	case USERSPACE_AVOID_TYPE:
		return MINUTES_TO_MS(cfg->avoid_list_exipry_time) -
			(cur_timestamp -
			 dlm_entry->ap_timestamp.userspace_avoid_timestamp);
	case USERSPACE_DENYLIST_TYPE:
		return cur_timestamp -
			  dlm_entry->ap_timestamp.userspace_denylist_timestamp;
	case DRIVER_AVOID_TYPE:
		return MINUTES_TO_MS(cfg->avoid_list_exipry_time) -
			(cur_timestamp -
			 dlm_entry->ap_timestamp.driver_avoid_timestamp);
	case DRIVER_DENYLIST_TYPE:
		return MINUTES_TO_MS(cfg->deny_list_exipry_time) -
			(cur_timestamp -
			 dlm_entry->ap_timestamp.driver_denylist_timestamp);

	/*
	 * For RSSI reject lowest delta would be the BSSID whose retry delay
	 * is about to expire, hence the delta would be remaining duration for
	 * de-denylisting the AP from rssi reject list.
	 */
	case DRIVER_RSSI_REJECT_TYPE:
		if (dlm_entry->rssi_reject_params.retry_delay)
			disallowed_time =
				dlm_entry->rssi_reject_params.retry_delay -
				(cur_timestamp -
				dlm_entry->ap_timestamp.rssi_reject_timestamp);
		else
			disallowed_time =
				(int32_t)(MINUTES_TO_MS(RSSI_TIMEOUT_VALUE) -
				(cur_timestamp -
				 dlm_entry->ap_timestamp.rssi_reject_timestamp)
				 );
		return ((disallowed_time < 0) ? 0 : disallowed_time);
	case DRIVER_MONITOR_TYPE:
		return cur_timestamp -
			       dlm_entry->ap_timestamp.driver_monitor_timestamp;
	default:
		dlm_debug("Wrong list type %d passed", list_type);
		return 0;
	}
}

static bool
dlm_is_oldest_entry(enum dlm_reject_ap_type list_type,
		    qdf_time_t cur_node_delta,
		    qdf_time_t oldest_node_delta)
{
	switch (list_type) {
	/*
	 * For RSSI reject, userspace avoid, driver avoid/denylist type the
	 * lowest retry delay has to be found out hence if oldest_node_delta is
	 * 0, mean this is the first entry and thus return true, If
	 * oldest_node_delta is non zero, compare the delta and return true if
	 * the cur entry has lower retry delta.
	 */
	case DRIVER_RSSI_REJECT_TYPE:
	case USERSPACE_AVOID_TYPE:
	case DRIVER_AVOID_TYPE:
	case DRIVER_DENYLIST_TYPE:
		if (!oldest_node_delta || cur_node_delta < oldest_node_delta)
			return true;
		break;
	case USERSPACE_DENYLIST_TYPE:
	case DRIVER_MONITOR_TYPE:
		if (cur_node_delta > oldest_node_delta)
			return true;
		break;
	default:
		dlm_debug("Wrong list type passed %d", list_type);
		return false;
	}

	return false;
}

static QDF_STATUS
dlm_try_delete_bssid_in_list(qdf_list_t *reject_ap_list,
			     enum dlm_reject_ap_type list_type,
			     struct dlm_config *cfg)
{
	struct dlm_reject_ap *dlm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct dlm_reject_ap *oldest_dlm_entry = NULL;
	qdf_time_t oldest_node_delta = 0;
	qdf_time_t cur_node_delta = 0;

	qdf_list_peek_front(reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(reject_ap_list, cur_node, &next_node);

		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);

		if (dlm_is_bssid_present_only_in_list_type(list_type,
							   dlm_entry)) {
			cur_node_delta = dlm_get_delta_of_bssid(list_type,
								dlm_entry, cfg);

			if (dlm_is_oldest_entry(list_type, cur_node_delta,
						oldest_node_delta)) {
				/* now this is the oldest entry*/
				oldest_dlm_entry = dlm_entry;
				oldest_node_delta = cur_node_delta;
			}
		}
		cur_node = next_node;
		next_node = NULL;
	}

	if (oldest_dlm_entry) {
		/* Remove this entry to make space for the next entry */
		dlm_debug("Removed " QDF_MAC_ADDR_FMT ", type = %d",
			  QDF_MAC_ADDR_REF(oldest_dlm_entry->bssid.bytes),
			  list_type);
		qdf_list_remove_node(reject_ap_list, &oldest_dlm_entry->node);
		qdf_mem_free(oldest_dlm_entry);
		return QDF_STATUS_SUCCESS;
	}
	/* If the flow has reached here, that means no entry could be removed */

	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
dlm_remove_lowest_delta_entry(qdf_list_t *reject_ap_list,
			      struct dlm_config *cfg)
{
	QDF_STATUS status;

	/*
	 * According to the Priority, the driver will try to remove the entries,
	 * as the least priority list, that is monitor list would not penalize
	 * the BSSIDs for connection. The priority order for the removal is:-
	 * 1. Monitor list
	 * 2. Driver avoid list
	 * 3. Userspace avoid list.
	 * 4. RSSI reject list.
	 * 5. Driver Denylist.
	 * 6. Userspace Denylist.
	 */

	status = dlm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_MONITOR_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = dlm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_AVOID_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = dlm_try_delete_bssid_in_list(reject_ap_list,
					      USERSPACE_AVOID_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = dlm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_RSSI_REJECT_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = dlm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_DENYLIST_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = dlm_try_delete_bssid_in_list(reject_ap_list,
					      USERSPACE_DENYLIST_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	dlm_debug("Failed to remove AP from denylist manager");

	return QDF_STATUS_E_FAILURE;
}

static enum dlm_reject_ap_reason
dlm_get_rssi_reject_reason(struct dlm_reject_ap *dlm_entry)
{
	if (dlm_entry->poor_rssi)
		return REASON_ASSOC_REJECT_POOR_RSSI;
	else if (dlm_entry->oce_assoc_reject)
		return REASON_ASSOC_REJECT_OCE;
	else if (dlm_entry->btm_bss_termination)
		return REASON_BTM_BSS_TERMINATION;
	else if (dlm_entry->btm_disassoc_imminent)
		return REASON_BTM_DISASSOC_IMMINENT;
	else if (dlm_entry->btm_mbo_retry)
		return REASON_BTM_MBO_RETRY;
	else if (dlm_entry->no_more_stas)
		return REASON_REASSOC_NO_MORE_STAS;
	else if (dlm_entry->reassoc_rssi_reject)
		return REASON_REASSOC_RSSI_REJECT;

	return REASON_UNKNOWN;
}

static void
dlm_fill_rssi_reject_params(struct dlm_reject_ap *dlm_entry,
			    enum dlm_reject_ap_type reject_ap_type,
			    struct reject_ap_config_params *dlm_reject_list)
{
	if (reject_ap_type != DRIVER_RSSI_REJECT_TYPE)
		return;

	dlm_reject_list->source = dlm_entry->rssi_reject_params.source;
	dlm_reject_list->original_timeout =
			dlm_entry->rssi_reject_params.original_timeout;
	dlm_reject_list->received_time =
			dlm_entry->rssi_reject_params.received_time;
	dlm_reject_list->reject_reason = dlm_get_rssi_reject_reason(dlm_entry);
	dlm_debug(QDF_MAC_ADDR_FMT " source %d original timeout %u received time %lu reject reason %d",
		  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes),
		  dlm_reject_list->source,
		  dlm_reject_list->original_timeout,
		  dlm_reject_list->received_time,
		  dlm_reject_list->reject_reason);
}

/**
 * dlm_find_reject_type_string() - Function to convert int to string
 * @reject_ap_type:   dlm_reject_ap_type
 *
 * This function is used to convert int value of enum dlm_reject_ap_type
 * to string format.
 *
 * Return: String
 *
 */
static const char *
dlm_find_reject_type_string(enum dlm_reject_ap_type reject_ap_type)
{
	switch (reject_ap_type) {
	CASE_RETURN_STRING(USERSPACE_AVOID_TYPE);
	CASE_RETURN_STRING(USERSPACE_DENYLIST_TYPE);
	CASE_RETURN_STRING(DRIVER_AVOID_TYPE);
	CASE_RETURN_STRING(DRIVER_DENYLIST_TYPE);
	CASE_RETURN_STRING(DRIVER_RSSI_REJECT_TYPE);
	CASE_RETURN_STRING(DRIVER_MONITOR_TYPE);
	default:
		return "REJECT_REASON_UNKNOWN";
	}
}

/**
 * dlm_get_reject_ap_type() - Function to find reject ap type
 * @dlm_entry:   dlm_reject_ap
 *
 * This function is used to get reject ap type.
 *
 * Return: dlm_reject_ap_type
 *
 */
static enum dlm_reject_ap_type
dlm_get_reject_ap_type(struct dlm_reject_ap *dlm_entry)
{
	if (DLM_IS_AP_AVOIDED_BY_USERSPACE(dlm_entry))
		return USERSPACE_AVOID_TYPE;
	if (DLM_IS_AP_DENYLISTED_BY_USERSPACE(dlm_entry))
		return USERSPACE_DENYLIST_TYPE;
	if (DLM_IS_AP_AVOIDED_BY_DRIVER(dlm_entry))
		return DRIVER_AVOID_TYPE;
	if (DLM_IS_AP_DENYLISTED_BY_DRIVER(dlm_entry))
		return DRIVER_DENYLIST_TYPE;
	if (DLM_IS_AP_IN_RSSI_REJECT_LIST(dlm_entry))
		return DRIVER_RSSI_REJECT_TYPE;
	if (DLM_IS_AP_IN_MONITOR_LIST(dlm_entry))
		return DRIVER_MONITOR_TYPE;

	return REJECT_REASON_UNKNOWN;
}

bool dlm_is_bssid_in_reject_list(struct wlan_objmgr_pdev *pdev,
				 struct qdf_mac_addr *bssid)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	struct dlm_reject_ap *dlm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	if (!dlm_ctx) {
		dlm_err("dlm_ctx is NULL");
		return false;
	}
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));
	if (!dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return false;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return false;
	}

	qdf_list_peek_front(&dlm_ctx->reject_ap_list, &cur_node);
	while (cur_node) {
		qdf_list_peek_next(&dlm_ctx->reject_ap_list, cur_node,
				   &next_node);
		dlm_entry =
			qdf_container_of(cur_node, struct dlm_reject_ap, node);
		/* Update the AP info to the latest list first */
		dlm_update_ap_info(dlm_entry, &dlm_psoc_obj->dlm_cfg, NULL);
		if (!dlm_entry->reject_ap_type) {
			dlm_debug(QDF_MAC_ADDR_FMT " cleared from list",
				  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
			qdf_list_remove_node(&dlm_ctx->reject_ap_list,
					     &dlm_entry->node);
			qdf_mem_free(dlm_entry);
			cur_node = next_node;
			next_node = NULL;
			continue;
		}

		if (qdf_is_macaddr_equal(&dlm_entry->bssid, bssid)) {
			dlm_debug("BSSID reject_ap_type 0x%x",
				  dlm_entry->reject_ap_type);
			if (DLM_IS_AP_IN_DENYLIST(dlm_entry)) {
				dlm_debug("BSSID is present in deny list");
				qdf_mutex_release(
					&dlm_ctx->reject_ap_list_lock);
				return true;
			}
			qdf_mutex_release(
				&dlm_ctx->reject_ap_list_lock);
			return false;
		}
		cur_node = next_node;
		next_node = NULL;
	}

	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);

	return false;
}

/**
 * dlm_dump_denylist_bssid() - Function to dump denylisted bssid
 * @pdev:  pdev object
 *
 * This function is used to dump denylisted bssid along with reject
 * ap type, source, delay and required rssi
 *
 * Return: None
 *
 */
void dlm_dump_denylist_bssid(struct wlan_objmgr_pdev *pdev)
{
	struct dlm_reject_ap *dlm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	uint32_t reject_duration;
	enum dlm_reject_ap_type reject_ap_type;
	qdf_list_t *reject_db_list;
	QDF_STATUS status;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	reject_db_list = &dlm_ctx->reject_ap_list;
	qdf_list_peek_front(reject_db_list, &cur_node);
	while (cur_node) {
		qdf_list_peek_next(reject_db_list, cur_node, &next_node);

		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);

		reject_ap_type = dlm_get_reject_ap_type(dlm_entry);

		reject_duration = dlm_get_delta_of_bssid(reject_ap_type,
							 dlm_entry,
							 &dlm_psoc_obj->dlm_cfg);

		dlm_nofl_debug("DENYLIST BSSID " QDF_MAC_ADDR_FMT " type %s retry delay %dms expected RSSI %d reject reason %d rejection source %d",
			       QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes),
			       dlm_find_reject_type_string(reject_ap_type),
			       reject_duration,
			       dlm_entry->rssi_reject_params.expected_rssi,
			       dlm_entry->reject_ap_reason,
			       dlm_entry->rssi_reject_params.source);
		cur_node = next_node;
		next_node = NULL;
	}

	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
}

static void dlm_fill_reject_list(qdf_list_t *reject_db_list,
				 struct reject_ap_config_params *reject_list,
				 uint8_t *num_of_reject_bssid,
				 enum dlm_reject_ap_type reject_ap_type,
				 uint8_t max_bssid_to_be_filled,
				 struct dlm_config *cfg)
{
	struct dlm_reject_ap *dlm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;

	qdf_list_peek_front(reject_db_list, &cur_node);
	while (cur_node) {
		if (*num_of_reject_bssid == max_bssid_to_be_filled) {
			dlm_debug("Max size reached in list, reject_ap_type %d",
				  reject_ap_type);
			return;
		}
		qdf_list_peek_next(reject_db_list, cur_node, &next_node);

		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);

		dlm_update_ap_info(dlm_entry, cfg, NULL);
		if (!dlm_entry->reject_ap_type) {
			dlm_debug(QDF_MAC_ADDR_FMT " cleared from list",
				  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
			qdf_list_remove_node(reject_db_list, &dlm_entry->node);
			qdf_mem_free(dlm_entry);
			cur_node = next_node;
			next_node = NULL;
			continue;
		}

		if (dlm_is_bssid_of_type(reject_ap_type, dlm_entry)) {
			struct reject_ap_config_params *dlm_reject_list;

			dlm_reject_list = &reject_list[*num_of_reject_bssid];
			dlm_reject_list->expected_rssi =
				    dlm_entry->rssi_reject_params.expected_rssi;
			dlm_reject_list->reject_duration =
			       dlm_get_delta_of_bssid(reject_ap_type, dlm_entry,
						      cfg);

			dlm_fill_rssi_reject_params(dlm_entry, reject_ap_type,
						    dlm_reject_list);
			dlm_reject_list->reject_ap_type = reject_ap_type;
			dlm_reject_list->bssid = dlm_entry->bssid;
			(*num_of_reject_bssid)++;
			dlm_debug("Adding BSSID " QDF_MAC_ADDR_FMT " of type %d retry delay %d expected RSSI %d, entries added = %d reject reason %d",
				  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes),
				  reject_ap_type,
				  reject_list[*num_of_reject_bssid - 1].reject_duration,
				  dlm_entry->rssi_reject_params.expected_rssi,
				  *num_of_reject_bssid,
				  dlm_entry->reject_ap_reason);
		}
		cur_node = next_node;
		next_node = NULL;
	}
}

#if defined(WLAN_FEATURE_ROAM_OFFLOAD)
void dlm_update_reject_ap_list_to_fw(struct wlan_objmgr_psoc *psoc)
{
	struct dlm_config *cfg;
	struct wlan_objmgr_pdev *pdev;
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	QDF_STATUS status;

	dlm_psoc_obj = dlm_get_psoc_obj(psoc);
	if (!dlm_psoc_obj) {
		dlm_err("DLM psoc obj NULL");
		return;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, dlm_psoc_obj->pdev_id,
					  WLAN_MLME_CM_ID);
	if (!pdev) {
		dlm_err("pdev obj NULL");
		return;
	}

	dlm_ctx = dlm_get_pdev_obj(pdev);
	if (!dlm_ctx) {
		dlm_err("DLM pdev obj NULL");
		goto end;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		goto end;
	}

	cfg = &dlm_psoc_obj->dlm_cfg;
	dlm_send_reject_ap_list_to_fw(pdev, &dlm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);

end:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_CM_ID);
}

static void dlm_store_pdevid_in_dlm_psocpriv(struct wlan_objmgr_pdev *pdev)
{
	struct dlm_psoc_priv_obj *dlm_psoc_obj;

	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_psoc_obj) {
		dlm_err("DLM psoc obj NULL");
		return;
	}

	dlm_psoc_obj->pdev_id = pdev->pdev_objmgr.wlan_pdev_id;
}

void
dlm_send_reject_ap_list_to_fw(struct wlan_objmgr_pdev *pdev,
			      qdf_list_t *reject_db_list,
			      struct dlm_config *cfg)
{
	QDF_STATUS status;
	bool is_dlm_suspended;
	struct reject_ap_params reject_params = {0};

	ucfg_dlm_psoc_get_suspended(wlan_pdev_get_psoc(pdev),
				    &is_dlm_suspended);
	if (is_dlm_suspended) {
		dlm_store_pdevid_in_dlm_psocpriv(pdev);
		dlm_debug("Failed to send reject AP list to FW as DLM is suspended");
		return;
	}

	reject_params.bssid_list =
			qdf_mem_malloc(sizeof(*reject_params.bssid_list) *
				       PDEV_MAX_NUM_BSSID_DISALLOW_LIST);
	if (!reject_params.bssid_list)
		return;

	/* The priority for filling is as below */
	dlm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     USERSPACE_DENYLIST_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	dlm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     DRIVER_DENYLIST_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	dlm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     DRIVER_RSSI_REJECT_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	dlm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     USERSPACE_AVOID_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	dlm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     DRIVER_AVOID_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);

	status = tgt_dlm_send_reject_list_to_fw(pdev, &reject_params);

	if (QDF_IS_STATUS_ERROR(status))
		dlm_err("failed to send the reject Ap list to FW");

	qdf_mem_free(reject_params.bssid_list);
}
#endif

QDF_STATUS
dlm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
			     struct reject_ap_info *ap_info)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	struct dlm_config *cfg;
	struct dlm_reject_ap *dlm_entry;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_is_macaddr_zero(&ap_info->bssid) ||
	    qdf_is_macaddr_group(&ap_info->bssid)) {
		dlm_err("Zero/Broadcast BSSID received, entry not added");
		return QDF_STATUS_E_INVAL;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return status;
	}

	cfg = &dlm_psoc_obj->dlm_cfg;

	qdf_list_peek_front(&dlm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&dlm_ctx->reject_ap_list,
				   cur_node, &next_node);

		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);

		/* Update the AP info to the latest list first */
		dlm_update_ap_info(dlm_entry, cfg, NULL);
		if (!dlm_entry->reject_ap_type) {
			dlm_debug(QDF_MAC_ADDR_FMT " cleared from list",
				  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
			qdf_list_remove_node(&dlm_ctx->reject_ap_list,
					     &dlm_entry->node);
			qdf_mem_free(dlm_entry);
			cur_node = next_node;
			next_node = NULL;
			continue;
		}

		if (qdf_is_macaddr_equal(&dlm_entry->bssid, &ap_info->bssid)) {
			dlm_modify_entry(dlm_entry, cfg, ap_info);
			goto end;
		}

		cur_node = next_node;
		next_node = NULL;
	}

	if (qdf_list_size(&dlm_ctx->reject_ap_list) == MAX_BAD_AP_LIST_SIZE) {
		/* List is FULL, need to delete entries */
		status =
			dlm_remove_lowest_delta_entry(&dlm_ctx->reject_ap_list,
						      cfg);

		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
			return status;
		}
	}

	dlm_entry = qdf_mem_malloc(sizeof(*dlm_entry));
	if (!dlm_entry) {
		qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_list_insert_back(&dlm_ctx->reject_ap_list, &dlm_entry->node);
	dlm_modify_entry(dlm_entry, cfg, ap_info);

end:
	dlm_send_reject_ap_list_to_fw(pdev, &dlm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dlm_clear_userspace_denylist_info(struct wlan_objmgr_pdev *pdev)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_reject_ap *dlm_entry;
	QDF_STATUS status;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	if (!dlm_ctx) {
		dlm_err("dlm_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return QDF_STATUS_E_RESOURCES;
	}

	qdf_list_peek_front(&dlm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&dlm_ctx->reject_ap_list, cur_node,
				   &next_node);
		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);

		if (IS_AP_IN_USERSPACE_DENYLIST_ONLY(dlm_entry)) {
			dlm_debug("removing bssid: " QDF_MAC_ADDR_FMT,
				  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
			qdf_list_remove_node(&dlm_ctx->reject_ap_list,
					     &dlm_entry->node);
			qdf_mem_free(dlm_entry);
		} else if (DLM_IS_AP_DENYLISTED_BY_USERSPACE(dlm_entry)) {
			dlm_debug("Clearing userspace denylist bit for "
				   QDF_MAC_ADDR_FMT,
				   QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
			dlm_entry->userspace_denylist = false;
			dlm_entry->denylist_userspace = false;
		}
		cur_node = next_node;
		next_node = NULL;
	}
	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dlm_add_userspace_deny_list(struct wlan_objmgr_pdev *pdev,
			    struct qdf_mac_addr *bssid_deny_list,
			    uint8_t num_of_bssid)
{
	uint8_t i = 0;
	struct reject_ap_info ap_info;
	QDF_STATUS status;
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	struct dlm_config *cfg;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Clear all the info of APs already existing in DLM first */
	dlm_clear_userspace_denylist_info(pdev);
	cfg = &dlm_psoc_obj->dlm_cfg;

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return status;
	}

	dlm_send_reject_ap_list_to_fw(pdev, &dlm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);

	if (!bssid_deny_list || !num_of_bssid) {
		dlm_debug("Userspace denylist/num of denylist NULL");
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < num_of_bssid; i++) {
		qdf_mem_zero(&ap_info, sizeof(struct reject_ap_info));
		ap_info.bssid = bssid_deny_list[i];
		ap_info.reject_ap_type = USERSPACE_DENYLIST_TYPE;
		ap_info.source = ADDED_BY_DRIVER;
		ap_info.reject_reason = REASON_USERSPACE_BL;
		status = dlm_add_bssid_to_reject_list(pdev, &ap_info);
		if (QDF_IS_STATUS_ERROR(status)) {
			dlm_err("Failed to add bssid to userspace denylist");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

void
dlm_flush_reject_ap_list(struct dlm_pdev_priv_obj *dlm_ctx)
{
	struct dlm_reject_ap *dlm_entry = NULL;
	QDF_STATUS status;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	qdf_list_peek_front(&dlm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&dlm_ctx->reject_ap_list, cur_node,
				   &next_node);
		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);
		qdf_list_remove_node(&dlm_ctx->reject_ap_list,
				     &dlm_entry->node);
		qdf_mem_free(dlm_entry);
		cur_node = next_node;
		next_node = NULL;
	}

	dlm_debug("DLM reject ap list flushed");
	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
}

uint8_t
dlm_get_bssid_reject_list(struct wlan_objmgr_pdev *pdev,
			  struct reject_ap_config_params *reject_list,
			  uint8_t max_bssid_to_be_filled,
			  enum dlm_reject_ap_type reject_ap_type)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	uint8_t num_of_reject_bssid = 0;
	QDF_STATUS status;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return 0;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return 0;
	}

	dlm_fill_reject_list(&dlm_ctx->reject_ap_list, reject_list,
			     &num_of_reject_bssid, reject_ap_type,
			     max_bssid_to_be_filled, &dlm_psoc_obj->dlm_cfg);

	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);

	return num_of_reject_bssid;
}

void
dlm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				struct qdf_mac_addr bssid,
				enum dlm_connection_state con_state)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;
	struct dlm_reject_ap *dlm_entry = NULL;
	qdf_time_t connection_age = 0;
	bool entry_found = false;
	qdf_time_t max_entry_time;
	qdf_time_t bad_bssid_reset_time;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	qdf_list_peek_front(&dlm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&dlm_ctx->reject_ap_list, cur_node,
				   &next_node);
		dlm_entry = qdf_container_of(cur_node, struct dlm_reject_ap,
					     node);

		if (!qdf_mem_cmp(dlm_entry->bssid.bytes, bssid.bytes,
				 QDF_MAC_ADDR_SIZE)) {
			dlm_debug(QDF_MAC_ADDR_FMT " present in DLM reject list, updating connect info con_state = %d",
				  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes),
				  con_state);
			entry_found = true;
			break;
		}
		cur_node = next_node;
		next_node = NULL;
	}

	/* This means that the BSSID was not added in the reject list of DLM */
	if (!entry_found) {
		qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
		return;
	}
	switch (con_state) {
	case DLM_AP_CONNECTED:
		dlm_entry->connect_timestamp = qdf_mc_timer_get_system_time();
		break;
	case DLM_AP_DISCONNECTED:
		/* Update the dlm info first */
		dlm_update_ap_info(dlm_entry, &dlm_psoc_obj->dlm_cfg, NULL);

		max_entry_time = dlm_entry->connect_timestamp;
		if (dlm_entry->driver_denylist) {
			max_entry_time =
			   dlm_entry->ap_timestamp.driver_denylist_timestamp;
		} else if (dlm_entry->driver_avoidlist) {
			max_entry_time =
			 QDF_MAX(dlm_entry->ap_timestamp.driver_avoid_timestamp,
				 dlm_entry->connect_timestamp);
		}
		connection_age = qdf_mc_timer_get_system_time() -
							max_entry_time;
		bad_bssid_reset_time =
			dlm_psoc_obj->dlm_cfg.bad_bssid_counter_reset_time;
		if (connection_age > SECONDS_TO_MS(bad_bssid_reset_time)) {
			dlm_entry->driver_avoidlist = false;
			dlm_entry->driver_denylist = false;
			dlm_entry->driver_monitorlist = false;
			dlm_entry->userspace_avoidlist = false;
			dlm_debug("updated reject ap type %d ",
				  dlm_entry->reject_ap_type);
			if (!dlm_entry->reject_ap_type) {
				dlm_debug("Bad Bssid timer expired/AP cleared from all denylisting, removed " QDF_MAC_ADDR_FMT " from list",
					  QDF_MAC_ADDR_REF(dlm_entry->bssid.bytes));
				qdf_list_remove_node(&dlm_ctx->reject_ap_list,
						     &dlm_entry->node);
				qdf_mem_free(dlm_entry);
				dlm_send_reject_ap_list_to_fw(pdev,
					&dlm_ctx->reject_ap_list,
					&dlm_psoc_obj->dlm_cfg);
			}
		}
		break;
	default:
		dlm_debug("Invalid AP connection state received %d", con_state);
	};

	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
}

int32_t dlm_get_rssi_denylist_threshold(struct wlan_objmgr_pdev *pdev)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	struct dlm_config *cfg;

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return 0;
	}

	cfg = &dlm_psoc_obj->dlm_cfg;
	return cfg->delta_rssi;
}

