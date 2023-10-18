/*
 * Copyright (c) 2018-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Implements VDEV MLME public APIs
 */

#include <wlan_objmgr_vdev_obj.h>
#include <wlan_mlme_dbg.h>
#include "include/wlan_vdev_mlme.h"
#include "../../core/src/vdev_mlme_sm.h"
#include <wlan_vdev_mlme_api.h>
#include <include/wlan_mlme_cmn.h>
#include <qdf_module.h>
#include "wlan_objmgr_vdev_obj.h"

struct vdev_mlme_obj *wlan_vdev_mlme_get_cmpt_obj(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	if (!vdev) {
		mlme_err("vdev is NULL");
		return NULL;
	}

	vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							  WLAN_UMAC_COMP_MLME);
	if (!vdev_mlme) {
		mlme_err(" MLME component object is NULL");
		return NULL;
	}

	return vdev_mlme;
}

qdf_export_symbol(wlan_vdev_mlme_get_cmpt_obj);

void wlan_vdev_mlme_set_ext_hdl(struct wlan_objmgr_vdev *vdev,
				mlme_vdev_ext_t *ext_hdl)
{
	struct vdev_mlme_obj *vdev_mlme;

	if (!ext_hdl) {
		mlme_err("Invalid input");
		return;
	}

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (vdev_mlme)
		vdev_mlme->ext_vdev_ptr = ext_hdl;
}

qdf_export_symbol(wlan_vdev_mlme_set_ext_hdl);

mlme_vdev_ext_t *wlan_vdev_mlme_get_ext_hdl(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (vdev_mlme)
		return vdev_mlme->ext_vdev_ptr;

	return NULL;
}

qdf_export_symbol(wlan_vdev_mlme_get_ext_hdl);

QDF_STATUS wlan_vdev_mlme_sm_deliver_evt(struct wlan_objmgr_vdev *vdev,
					 enum wlan_vdev_sm_evt event,
					 uint16_t event_data_len,
					 void *event_data)
{
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status;
	enum wlan_vdev_state state_entry, state_exit;
	enum wlan_vdev_state substate_entry, substate_exit;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_vdev_sm_spin_lock(vdev_mlme);

	/* store entry state and sub state for prints */
	state_entry = wlan_vdev_mlme_get_state(vdev);
	substate_entry = wlan_vdev_mlme_get_substate(vdev);
	mlme_vdev_sm_print_state_event(vdev_mlme, event);

	status = mlme_vdev_sm_deliver_event(vdev_mlme, event, event_data_len,
					    event_data);
	/* Take exit state, exit substate for prints */
	state_exit = wlan_vdev_mlme_get_state(vdev);
	substate_exit = wlan_vdev_mlme_get_substate(vdev);
	/* If no state and substate change, don't print */
	if (!((state_entry == state_exit) && (substate_entry == substate_exit)))
		mlme_vdev_sm_print_state(vdev_mlme);
	mlme_vdev_sm_spin_unlock(vdev_mlme);

	return status;
}

qdf_export_symbol(wlan_vdev_mlme_sm_deliver_evt);

QDF_STATUS wlan_vdev_mlme_sm_deliver_evt_sync(struct wlan_objmgr_vdev *vdev,
					      enum wlan_vdev_sm_evt event,
					      uint16_t event_data_len,
					      void *event_data)
{
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = mlme_vdev_sm_deliver_event(vdev_mlme, event, event_data_len,
					    event_data);

	return status;
}

qdf_export_symbol(wlan_vdev_mlme_sm_deliver_evt_sync);

#ifdef SM_ENG_HIST_ENABLE
void wlan_vdev_mlme_sm_history_print(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("vdev component object is NULL");
		return;
	}

	mlme_vdev_sm_history_print(vdev_mlme);
}
#endif

QDF_STATUS wlan_vdev_allow_connect_n_tx(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);
	if ((state == WLAN_VDEV_S_UP && substate == WLAN_VDEV_SS_UP_ACTIVE) ||
	    ((state == WLAN_VDEV_S_SUSPEND) &&
	     (substate == WLAN_VDEV_SS_SUSPEND_CSA_RESTART)))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wlan_vdev_mlme_is_active(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;

	state = wlan_vdev_mlme_get_state(vdev);
	if ((state == WLAN_VDEV_S_UP) ||  (state == WLAN_VDEV_S_DFS_CAC_WAIT) ||
	    (state == WLAN_VDEV_S_SUSPEND))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(wlan_vdev_mlme_is_active);

QDF_STATUS wlan_vdev_chan_config_valid(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);
	if (!((state == WLAN_VDEV_S_INIT) || (state == WLAN_VDEV_S_STOP)))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(wlan_vdev_chan_config_valid);

QDF_STATUS wlan_vdev_mlme_is_csa_restart(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);
	if ((state == WLAN_VDEV_S_SUSPEND) &&
	    (substate == WLAN_VDEV_SS_SUSPEND_CSA_RESTART))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(wlan_vdev_mlme_is_csa_restart);

QDF_STATUS wlan_vdev_is_going_down(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);
	if ((state == WLAN_VDEV_S_STOP) ||
	    ((state == WLAN_VDEV_S_SUSPEND) &&
	     (substate == WLAN_VDEV_SS_SUSPEND_SUSPEND_DOWN)))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wlan_vdev_is_mlo_peer_create_allowed(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;
	bool acs_in_progress;
	QDF_STATUS ret;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!vdev) {
		mlme_err("vdev is null");
		return status;
	}

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);

	acs_in_progress = false;
	ret = mlme_ext_hdl_get_acs_in_progress(vdev, &acs_in_progress);
	if (ret != QDF_STATUS_SUCCESS) {
		mlme_err("Unable to get ACS in progress status");
		return status;
	}

	if (!acs_in_progress)
		if ((state == WLAN_VDEV_S_UP) ||
		    ((state == WLAN_VDEV_S_SUSPEND) &&
		     (substate == WLAN_VDEV_SS_SUSPEND_CSA_RESTART)) ||
		    (state == WLAN_VDEV_S_DFS_CAC_WAIT))
			status = QDF_STATUS_SUCCESS;

	/* with link rejection feature, this check can be removed */
	if (wlan_vdev_mlme_op_flags_get(vdev, WLAN_VDEV_OP_MLO_STOP_LINK_DEL) ||
	    wlan_vdev_mlme_op_flags_get(vdev,
					WLAN_VDEV_OP_MLO_LINK_TBTT_COMPLETE))
		status = QDF_STATUS_E_FAILURE;

	return status;
}

QDF_STATUS wlan_vdev_is_restart_progress(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);
	if ((state == WLAN_VDEV_S_START) &&
	    (substate == WLAN_VDEV_SS_START_RESTART_PROGRESS))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wlan_vdev_is_dfs_cac_wait(struct wlan_objmgr_vdev *vdev)
{
	if (wlan_vdev_mlme_get_state(vdev) == WLAN_VDEV_S_DFS_CAC_WAIT)
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

void wlan_vdev_mlme_cmd_lock(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("vdev component object is NULL");
		return;
	}

	mlme_vdev_cmd_mutex_acquire(vdev_mlme);
}

void wlan_vdev_mlme_cmd_unlock(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("vdev component object is NULL");
		return;
	}

	mlme_vdev_cmd_mutex_release(vdev_mlme);
}

QDF_STATUS wlan_vdev_mlme_is_scan_allowed(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);
	if ((state == WLAN_VDEV_S_INIT) ||
	    (state == WLAN_VDEV_S_UP && substate == WLAN_VDEV_SS_UP_ACTIVE) ||
	    (state == WLAN_VDEV_S_STOP))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wlan_vdev_mlme_is_init_state(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;

	state = wlan_vdev_mlme_get_state(vdev);
	if (state == WLAN_VDEV_S_INIT)
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wlan_vdev_is_up_active_state(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);
	if (state == WLAN_VDEV_S_UP && substate == WLAN_VDEV_SS_UP_ACTIVE)
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(wlan_vdev_is_up_active_state);

#ifdef WLAN_FEATURE_11BE_MLO
bool
wlan_vdev_mlme_get_is_mlo_link(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool is_link = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", vdev_id);
		return is_link;
	}

	if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	    wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		is_link = true;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return is_link;
}

bool
wlan_vdev_mlme_get_is_mlo_vdev(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool is_mlo_vdev = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", vdev_id);
		return is_mlo_vdev;
	}

	if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	    !wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		is_mlo_vdev = true;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return is_mlo_vdev;
}
#endif
#ifdef WLAN_FEATURE_SR
void
wlan_mlme_update_sr_data(struct wlan_objmgr_vdev *vdev, int *val,
			 int32_t srg_pd_threshold, int32_t non_srg_pd_threshold,
			 bool is_sr_enable)
{
	uint8_t ap_non_srg_pd_threshold = 0;
	uint8_t ap_srg_min_pd_threshold_offset = 0;
	uint8_t ap_srg_max_pd_threshold_offset = 0;
	uint8_t sr_ctrl;

	sr_ctrl = wlan_vdev_mlme_get_sr_ctrl(vdev);
	if (!(sr_ctrl & NON_SRG_PD_SR_DISALLOWED) &&
	    (sr_ctrl & NON_SRG_OFFSET_PRESENT)) {
		ap_non_srg_pd_threshold =
			wlan_vdev_mlme_get_non_srg_pd_offset(vdev) +
			SR_PD_THRESHOLD_MIN;
		/*
		 * Update non_srg_pd_threshold with provide
		 * non_srg_pd_threshold for non-srg, if pd threshold is
		 * with in the range else keep the same as
		 * advertised by AP.
		 */
		if (!non_srg_pd_threshold ||
		    (non_srg_pd_threshold > ap_non_srg_pd_threshold))
			non_srg_pd_threshold = ap_non_srg_pd_threshold;

		/* 31st BIT - Enable/Disable Non-SRG based spatial reuse. */
		*val |= is_sr_enable << NON_SRG_SPR_ENABLE_POS;
	}

	if (sr_ctrl & SRG_INFO_PRESENT) {
		wlan_vdev_mlme_get_srg_pd_offset(
					vdev, &ap_srg_max_pd_threshold_offset,
					&ap_srg_min_pd_threshold_offset);
		/*
		 * Update srg_pd_threshold with provide
		 * srg_pd_threshold, if pd threshold is with in the
		 * SRG range else keep the max of advertised by AP.
		 */
		if (!srg_pd_threshold ||
		    (srg_pd_threshold > (ap_srg_max_pd_threshold_offset +
					SR_PD_THRESHOLD_MIN) ||
		    srg_pd_threshold < (ap_srg_min_pd_threshold_offset +
					SR_PD_THRESHOLD_MIN)))
			srg_pd_threshold = ap_srg_max_pd_threshold_offset +
					   SR_PD_THRESHOLD_MIN;

		/* 30th BIT - Enable/Disable SRG based spatial reuse. */
		*val |= is_sr_enable << SRG_SPR_ENABLE_POS;
	}
	/* bit    | purpose
	 * -----------------
	 * 0  - 7 | Param Value for non-SRG based Spatial Reuse
	 * 8  - 15| Param value for SRG based Spatial Reuse
	 * 29     | Param value is in dBm units rather than dB units
	 */
	QDF_SET_BITS(*val, NON_SRG_MAX_PD_OFFSET_POS, SR_PADDING_BYTE,
		     (uint8_t)non_srg_pd_threshold);
	QDF_SET_BITS(*val, SRG_THRESHOLD_MAX_PD_POS, SR_PADDING_BYTE,
		     (uint8_t)srg_pd_threshold);
	*val |= SR_PARAM_VAL_DBM_UNIT << SR_PARAM_VAL_DBM_POS;
	wlan_vdev_mlme_set_current_non_srg_pd_threshold(vdev,
							non_srg_pd_threshold);
	wlan_vdev_mlme_set_current_srg_pd_threshold(vdev, srg_pd_threshold);
}
#endif
