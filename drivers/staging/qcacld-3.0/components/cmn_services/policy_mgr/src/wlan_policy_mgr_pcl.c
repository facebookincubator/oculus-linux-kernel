/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_policy_mgr_pcl.c
 *
 * WLAN Concurrenct Connection Management APIs
 *
 */

/* Include files */

#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "qdf_str.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_utility.h"
#include "wlan_mlme_ucfg_api.h"
#ifdef WLAN_FEATURE_11BE_MLO
#include "wlan_mlo_mgr_cmn.h"
#endif
#include "wlan_policy_mgr_ll_sap.h"
#include "wlan_cm_ucfg_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_scan_api.h"
#include "wlan_nan_api.h"
#include "wlan_mlo_link_force.h"

/*
 * first_connection_pcl_table - table which provides PCL for the
 * very first connection in the system
 */
const enum policy_mgr_pcl_type
first_connection_pcl_table[PM_MAX_NUM_OF_MODE]
			[PM_MAX_CONC_PRIORITY_MODE] = {
	[PM_STA_MODE] = {PM_NONE, PM_NONE, PM_NONE},
	[PM_SAP_MODE] = {PM_5G,   PM_5G,   PM_5G  },
	[PM_P2P_CLIENT_MODE] = {PM_5G,   PM_5G,   PM_5G  },
	[PM_P2P_GO_MODE] = {PM_5G,   PM_5G,   PM_5G  },
	[PM_NAN_DISC_MODE] = {PM_5G, PM_5G, PM_5G},
	[PM_LL_LT_SAP_MODE] = {PM_5G, PM_5G, PM_5G},
};

pm_dbs_pcl_second_connection_table_type
		*second_connection_pcl_dbs_table;

enum policy_mgr_pcl_type const
	(*second_connection_pcl_non_dbs_table)[PM_MAX_ONE_CONNECTION_MODE]
			[PM_MAX_NUM_OF_MODE][PM_MAX_CONC_PRIORITY_MODE];
pm_dbs_pcl_third_connection_table_type
		*third_connection_pcl_dbs_table;
enum policy_mgr_pcl_type const
	(*third_connection_pcl_non_dbs_table)[PM_MAX_TWO_CONNECTION_MODE]
			[PM_MAX_NUM_OF_MODE][PM_MAX_CONC_PRIORITY_MODE];
policy_mgr_next_action_two_connection_table_type
		*next_action_two_connection_table;
policy_mgr_next_action_three_connection_table_type
		*next_action_three_connection_table;
policy_mgr_next_action_two_connection_table_type
		*next_action_two_connection_2x2_2g_1x1_5g_table;
policy_mgr_next_action_three_connection_table_type
		*next_action_three_connection_2x2_2g_1x1_5g_table;

QDF_STATUS policy_mgr_get_pcl_for_existing_conn(
		struct wlan_objmgr_psoc *psoc,
		enum policy_mgr_con_mode mode,
		uint32_t *pcl_ch, uint32_t *len,
		uint8_t *pcl_weight, uint32_t weight_len,
		bool all_matching_cxn_to_del,
		uint8_t vdev_id)
{
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_cxn_del = 0;

	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	policy_mgr_debug("get pcl for existing conn:%d", mode);
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	*len = 0;
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (policy_mgr_mode_specific_connection_count(psoc, mode, NULL) > 0) {
		/* Check, store and temp delete the mode's parameter */
		policy_mgr_store_and_del_conn_info(psoc, mode,
				all_matching_cxn_to_del, info, &num_cxn_del);
		/* Get the PCL */
		status = policy_mgr_get_pcl(psoc, mode, pcl_ch, len,
					    pcl_weight, weight_len, vdev_id);
		policy_mgr_debug("Get PCL to FW for mode:%d", mode);
		/* Restore the connection info */
		policy_mgr_restore_deleted_conn_info(psoc, info, num_cxn_del);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * policy_mgr_get_pcl_concurrent_connetions() - Get concurrent connections
 * those will affect PCL fetching for the given vdev id
 * @psoc: PSOC object information
 * @mode: Connection Mode
 * @vdev_id: vdev id
 * @vdev_ids: vdev id list of the concurrent connections
 * @vdev_ids_size: size of the vdev id list
 *
 * Return: number of the concurrent connections
 */
static uint32_t
policy_mgr_get_pcl_concurrent_connetions(struct wlan_objmgr_psoc *psoc,
					 enum policy_mgr_con_mode mode,
					 uint8_t vdev_id, uint8_t *vdev_ids,
					 uint32_t vdev_ids_size)
{
	struct wlan_objmgr_vdev *vdev;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t num_related = 0;
	bool is_ml_sta, has_same_band = false;
	uint8_t vdev_id_with_diff_band = WLAN_INVALID_VDEV_ID;
	uint8_t num_ml = 0, num_non_ml = 0, ml_vdev_id;
	uint8_t ml_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t non_ml_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t freq = 0, ml_freq;
	int i;

	if (!vdev_ids || !vdev_ids_size) {
		policy_mgr_err("Invalid parameters");
		return num_related;
	}

	if (mode != PM_STA_MODE) {
		vdev_ids[0] = vdev_id;
		return 1;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d is not present", vdev_id);
		goto out;
	}

	if (wlan_vdev_mlme_is_link_sta_vdev(vdev)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		policy_mgr_debug("ignore ML STA link vdev %d", vdev_id);
		goto out;
	}

	is_ml_sta = wlan_vdev_mlme_is_mlo_vdev(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	policy_mgr_get_ml_and_non_ml_sta_count(psoc, &num_ml, ml_idx,
					       &num_non_ml, non_ml_idx,
					       freq_list, vdev_id_list);
	for (i = 0;
	     i < num_non_ml + num_ml && num_related < vdev_ids_size; i++) {
		if (vdev_id_list[i] == vdev_id) {
			vdev_ids[num_related++] = vdev_id;
			freq = freq_list[i];
			break;
		}
	}

	/* No existing connection for the vdev id */
	if (!freq)
		goto out;

	for (i = 0; i < num_ml && num_related < vdev_ids_size; i++) {
		ml_vdev_id = vdev_id_list[ml_idx[i]];
		if (ml_vdev_id == vdev_id)
			continue;

		/* If it's ML STA, return vdev ids for all links */
		if (is_ml_sta) {
			policy_mgr_debug("vdev_ids[%d]: %d",
					 num_related, ml_vdev_id);
			vdev_ids[num_related++] = ml_vdev_id;
			continue;
		}

		ml_freq = freq_list[ml_idx[i]];
		if (wlan_reg_is_24ghz_ch_freq(ml_freq) ==
		    wlan_reg_is_24ghz_ch_freq(freq)) {
			if (policy_mgr_are_sbs_chan(psoc, freq, ml_freq) &&
			    wlan_cm_same_band_sta_allowed(psoc))
				continue;

			/*
			 * If it's Non-ML STA, and its freq is within the same
			 * band with one of the existing ML link, but can NOT
			 * lead to SBS, return the original vdev id and vdev id
			 * of the ML link within same band.
			 */
			policy_mgr_debug("vdev_ids[%d]: %d",
					 num_related, ml_vdev_id);
			vdev_ids[num_related++] = ml_vdev_id;
			has_same_band = true;
			break;
		}

		vdev_id_with_diff_band = ml_vdev_id;
	}

	/*
	 * If it's Non-ML STA, and ML STA is present but the links are
	 * within different band or (within same band but can lead to SBS and
	 * same band STA is allowed), return original vdev id and vdev id of
	 * any ML link within different band.
	 */
	if (!has_same_band && vdev_id_with_diff_band != WLAN_INVALID_VDEV_ID) {
		policy_mgr_debug("vdev_ids[%d]: %d",
				 num_related, vdev_id_with_diff_band);

		if (num_related < vdev_ids_size)
			vdev_ids[num_related++] = vdev_id_with_diff_band;
	}

out:
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return num_related;
}
#else
static inline uint32_t
policy_mgr_get_pcl_concurrent_connetions(struct wlan_objmgr_psoc *psoc,
					 enum policy_mgr_con_mode mode,
					 uint8_t vdev_id, uint8_t *vdev_ids,
					 uint32_t vdev_ids_size)
{
	if (!vdev_ids || !vdev_ids_size) {
		policy_mgr_err("Invalid parameters");
		return 0;
	}

	vdev_ids[0] = vdev_id;
	return 1;
}
#endif

QDF_STATUS policy_mgr_get_pcl_for_vdev_id(struct wlan_objmgr_psoc *psoc,
					  enum policy_mgr_con_mode mode,
					  uint32_t *pcl_ch, uint32_t *len,
					  uint8_t *pcl_weight,
					  uint32_t weight_len,
					  uint8_t vdev_id)
{
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t ids[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t num_del = 0, total_del = 0, id_num = 0;
	int i;

	policy_mgr_debug("get pcl for existing conn:%d vdev id %d",
			 mode, vdev_id);
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	id_num = policy_mgr_get_pcl_concurrent_connetions(psoc, mode,
							  vdev_id, ids,
							  QDF_ARRAY_SIZE(ids));
	if (!id_num || id_num > MAX_NUMBER_OF_CONC_CONNECTIONS) {
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	*len = 0;

	/* Check, store and temp delete the mode's parameter */
	for (i = 0; i < id_num; i++) {
		policy_mgr_store_and_del_conn_info_by_vdev_id(psoc,
							      ids[i],
							      &info[i],
							      &num_del);
		total_del += num_del;
	}

	/* Get the PCL */
	status = policy_mgr_get_pcl(psoc, mode, pcl_ch, len,
				    pcl_weight, weight_len, vdev_id);
	policy_mgr_debug("Get PCL to FW for mode:%d", mode);
	/* Restore the connection info */
	policy_mgr_restore_deleted_conn_info(psoc, info, total_del);

out:
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

QDF_STATUS
policy_mgr_get_pcl_for_scc_in_same_mode(struct wlan_objmgr_psoc *psoc,
					enum policy_mgr_con_mode mode,
					uint32_t *pcl_ch, uint32_t *len,
					uint8_t *pcl_weight,
					uint32_t weight_len,
					uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	qdf_freq_t vdev_freq;
	QDF_STATUS status;
	uint8_t num_del = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	status = policy_mgr_get_chan_by_session_id(psoc, vdev_id, &vdev_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Fail to get channel by vdev id %d", vdev_id);
		return status;
	}

	policy_mgr_debug("get pcl for existing conn:%d vdev id %d",
			 mode, vdev_id);

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_store_and_del_conn_info_by_chan_and_mode(psoc,
							    vdev_freq,
							    mode,
							    info,
							    &num_del);
	status = policy_mgr_get_pcl(psoc, mode, pcl_ch, len,
				    pcl_weight, weight_len, vdev_id);
	if (num_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, info, num_del);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

void
polic_mgr_send_pcl_to_fw(struct wlan_objmgr_psoc *psoc,
			 enum QDF_OPMODE mode)
{
	uint32_t conn_idx = 0;
	mac_handle_t mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	uint8_t vdev_id = WLAN_INVALID_VDEV_ID;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	for (conn_idx = 0; conn_idx < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_idx++) {
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
		if (!(pm_conc_connection_list[conn_idx].mode ==
		      PM_STA_MODE &&
		      pm_conc_connection_list[conn_idx].in_use)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			continue;
		}

		vdev_id = pm_conc_connection_list[conn_idx].vdev_id;
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

		/*
		 * Avoid sending PCL when roaming is in progress. PCL
		 * gets updated to firmware once roaming is done
		 */
		if (mode == QDF_SAP_MODE &&
		    wlan_cm_roaming_in_progress(pm_ctx->pdev,
						vdev_id)) {
			policy_mgr_debug("Roaming is in progress, don't stop RSO for vdev_id: %d",
					 vdev_id);
			continue;
		}

		pm_ctx->sme_cbacks.sme_rso_stop_cb(
				mac_handle, vdev_id,
				REASON_DRIVER_DISABLED,
				RSO_SET_PCL);

		policy_mgr_set_pcl_for_existing_combo(pm_ctx->psoc, PM_STA_MODE,
						      vdev_id);
		pm_ctx->sme_cbacks.sme_rso_start_cb(
				mac_handle, vdev_id,
				REASON_DRIVER_ENABLED,
				RSO_SET_PCL);
	}
}

void policy_mgr_decr_session_set_pcl(struct wlan_objmgr_psoc *psoc,
				     enum QDF_OPMODE mode,
				     uint8_t session_id)
{
	QDF_STATUS qdf_status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_status = policy_mgr_decr_active_session(psoc, mode, session_id);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		policy_mgr_debug("Invalid active session");
		return;
	}

	/*
	 * After the removal of this connection, we need to check if
	 * a STA connection still exists. The reason for this is that
	 * if one or more STA exists, we need to provide the updated
	 * PCL to the FW for cases like LFR.
	 *
	 * Since policy_mgr_get_pcl provides PCL list based on the new
	 * connection that is going to come up, we will find the
	 * existing STA entry, save it and delete it temporarily.
	 * After this we will get PCL as though as new STA connection
	 * is coming up. This will give the exact PCL that needs to be
	 * given to the FW. After setting the PCL, we need to restore
	 * the entry that we have saved before.
	 */

	if ((policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE,
						       NULL) > 0) &&
	    mode != QDF_STA_MODE)
		polic_mgr_send_pcl_to_fw(psoc, mode);

	/* do we need to change the HW mode */
	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return;

	policy_mgr_check_n_start_opportunistic_timer(psoc);
	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE)
		ml_nlink_conn_change_notify(
			psoc, session_id, ml_nlink_ap_stopped_evt, NULL);
}

/**
 * policy_mgr_update_valid_ch_freq_list() - Update policy manager valid ch list
 * @pm_ctx: policy manager context data
 * @reg_ch_list: Regulatory channel list
 * @is_client: true if caller is a client, false if it is a beaconing entity
 *
 * When regulatory component channel list is updated this internal function is
 * called to update policy manager copy of valid channel list.
 *
 * Return: QDF_STATUS_SUCCESS on success other qdf error status code
 */
static void
policy_mgr_update_valid_ch_freq_list(struct policy_mgr_psoc_priv_obj *pm_ctx,
				     struct regulatory_channel *reg_ch_list,
				     bool is_client)
{
	uint32_t i, j = 0, ch_freq;
	enum channel_state state;

	for (i = 0; i < NUM_CHANNELS; i++) {
		ch_freq = reg_ch_list[i].center_freq;
		if (is_client)
			state = wlan_reg_get_channel_state_for_pwrmode(
							pm_ctx->pdev, ch_freq,
							REG_CURRENT_PWR_MODE);
		else
			state =
			wlan_reg_get_channel_state_from_secondary_list_for_freq(
							pm_ctx->pdev, ch_freq);

		if (state != CHANNEL_STATE_DISABLE &&
		    state != CHANNEL_STATE_INVALID) {
			pm_ctx->valid_ch_freq_list[j] =
				reg_ch_list[i].center_freq;
			j++;
		}
	}
	pm_ctx->valid_ch_freq_list_count = j;
}

#ifdef FEATURE_WLAN_CH_AVOID_EXT
void
policy_mgr_set_freq_restriction_mask(struct policy_mgr_psoc_priv_obj *pm_ctx,
				     struct ch_avoid_ind_type *freq_list)
{
	pm_ctx->restriction_mask = freq_list->restriction_mask;
}

uint32_t
policy_mgr_get_freq_restriction_mask(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	return pm_ctx->restriction_mask;
}
#endif

void
policy_mgr_reg_chan_change_callback(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_pdev *pdev,
				    struct regulatory_channel *chan_list,
				    struct avoid_freq_ind_data *avoid_freq_ind,
				    void *arg)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;
	struct ch_avoid_ind_type *freq_list;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	policy_mgr_update_valid_ch_freq_list(pm_ctx, chan_list, false);

	if (!avoid_freq_ind) {
		policy_mgr_debug("avoid_freq_ind NULL");
		return;
	}

	/*
	 * The ch_list buffer can accommodate a maximum of
	 * NUM_CHANNELS and hence the ch_cnt should also not
	 * exceed NUM_CHANNELS.
	 */
	pm_ctx->unsafe_channel_count = avoid_freq_ind->chan_list.chan_cnt >=
			NUM_CHANNELS ?
			NUM_CHANNELS : avoid_freq_ind->chan_list.chan_cnt;

	freq_list = &avoid_freq_ind->freq_list;
	policy_mgr_set_freq_restriction_mask(pm_ctx, freq_list);

	for (i = 0; i < pm_ctx->unsafe_channel_count; i++)
		pm_ctx->unsafe_channel_list[i] =
			avoid_freq_ind->chan_list.chan_freq_list[i];

	policy_mgr_debug("Channel list update, received %d avoided channels",
			 pm_ctx->unsafe_channel_count);
}

QDF_STATUS policy_mgr_init_chan_avoidance(struct wlan_objmgr_psoc *psoc,
					  qdf_freq_t *chan_freq_list,
					  uint16_t chan_cnt)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->unsafe_channel_count = chan_cnt >= NUM_CHANNELS ?
			NUM_CHANNELS : chan_cnt;

	for (i = 0; i < pm_ctx->unsafe_channel_count; i++)
		pm_ctx->unsafe_channel_list[i] = chan_freq_list[i];

	policy_mgr_debug("Channel list init, received %d avoided channels",
			 pm_ctx->unsafe_channel_count);

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_update_with_safe_channel_list(struct wlan_objmgr_psoc *psoc,
					      uint32_t *pcl_channels,
					      uint32_t *len,
					      uint8_t *weight_list,
					      uint32_t weight_len)
{
	uint32_t current_channel_list[NUM_CHANNELS];
	uint8_t org_weight_list[NUM_CHANNELS];
	uint8_t is_unsafe = 1;
	uint8_t i, j;
	uint32_t safe_channel_count = 0, current_channel_count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t scc_on_lte_coex = 0;
	uint32_t nan_2g_freq, nan_5g_freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	if (len) {
		current_channel_count = QDF_MIN(*len, NUM_CHANNELS);
	} else {
		policy_mgr_err("invalid number of channel length");
		return;
	}

	if (!pm_ctx->unsafe_channel_count)
		return;

	qdf_mem_copy(current_channel_list, pcl_channels,
		     current_channel_count * sizeof(*current_channel_list));
	qdf_mem_zero(pcl_channels,
		     current_channel_count * sizeof(*pcl_channels));

	qdf_mem_copy(org_weight_list, weight_list, NUM_CHANNELS);
	qdf_mem_zero(weight_list, weight_len);

	policy_mgr_get_sta_sap_scc_lte_coex_chnl(psoc, &scc_on_lte_coex);
	nan_2g_freq =
		policy_mgr_mode_specific_get_channel(pm_ctx->psoc,
						     PM_NAN_DISC_MODE);
	nan_5g_freq = wlan_nan_get_disc_5g_ch_freq(pm_ctx->psoc);

	for (i = 0; i < current_channel_count; i++) {
		is_unsafe = 0;
		for (j = 0; j < pm_ctx->unsafe_channel_count; j++) {
			if (current_channel_list[i] ==
				pm_ctx->unsafe_channel_list[j]) {
				/* Found unsafe channel, update it */
				is_unsafe = 1;
				policy_mgr_debug("CH %d is not safe",
					current_channel_list[i]);
				break;
			}
		}
		if (is_unsafe && scc_on_lte_coex &&
		    policy_mgr_is_sta_sap_scc(psoc, current_channel_list[i])) {
			policy_mgr_debug("CH %d unsafe ignored when STA present on it",
					 current_channel_list[i]);
			is_unsafe = 0;
		} else if (is_unsafe &&
			   (nan_2g_freq == current_channel_list[i] ||
			    nan_5g_freq == current_channel_list[i]) &&
			    policy_mgr_is_force_scc(pm_ctx->psoc) &&
			 policy_mgr_get_nan_sap_scc_on_lte_coex_chnl(pm_ctx->psoc)) {
			is_unsafe = 0;
		}

		if (!is_unsafe) {
			pcl_channels[safe_channel_count] =
				current_channel_list[i];
			if (safe_channel_count < weight_len)
				weight_list[safe_channel_count] =
					org_weight_list[i];
			safe_channel_count++;
		}
	}
	*len = safe_channel_count;

	return;
}

static QDF_STATUS policy_mgr_modify_pcl_based_on_enabled_channels(
					struct wlan_objmgr_psoc *psoc,
					uint32_t *pcl_list_org,
					uint8_t *weight_list_org,
					uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	bool allow_go_scc_on_dfs_chn = false;
	bool dfs_master_capable = false;
	uint8_t sta_sap_scc_on_dfs_chnl = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return status;
	}

	status = ucfg_mlme_get_dfs_master_capability(psoc, &dfs_master_capable);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs master capable");
		return status;
	}

	status = policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc,
						&sta_sap_scc_on_dfs_chnl);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get sta_sap_scc_on_dfs_chnl");
		return status;
	}

	if (dfs_master_capable && sta_sap_scc_on_dfs_chnl &&
	    pm_ctx->cfg.go_force_scc == GO_FORCE_SCC_STRICT) {
		allow_go_scc_on_dfs_chn = true;
	}

	for (i = 0; i < *pcl_len_org; i++) {
		if ((!wlan_reg_is_passive_or_disable_for_pwrmode(
			pm_ctx->pdev, pcl_list_org[i],
			REG_CURRENT_PWR_MODE)) ||
		    (allow_go_scc_on_dfs_chn &&
		     wlan_reg_is_dfs_for_freq(pm_ctx->pdev, pcl_list_org[i]))) {
			pcl_list_org[pcl_len] = pcl_list_org[i];
			weight_list_org[pcl_len++] = weight_list_org[i];
		}
	}
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_modify_pcl_based_on_dnbs(
						struct wlan_objmgr_psoc *psoc,
						uint32_t *pcl_list_org,
						uint8_t *weight_list_org,
						uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	bool ok;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return status;
	}
	for (i = 0; i < *pcl_len_org; i++) {
		status = policy_mgr_is_chan_ok_for_dnbs(psoc, pcl_list_org[i],
							&ok);

		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Not able to check DNBS eligibility");
			return status;
		}
		if (ok) {
			pcl_list[pcl_len] = pcl_list_org[i];
			weight_list[pcl_len++] = weight_list_org[i];
		}
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_get_channel(struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				uint32_t *vdev_id)
{
	uint32_t idx = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	if (mode >= PM_MAX_NUM_OF_MODE) {
		policy_mgr_err("incorrect mode");
		return 0;
	}

	for (idx = 0; idx < MAX_NUMBER_OF_CONC_CONNECTIONS; idx++) {
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
		if ((pm_conc_connection_list[idx].mode == mode) &&
				(!vdev_id || (*vdev_id ==
					pm_conc_connection_list[idx].vdev_id))
				&& pm_conc_connection_list[idx].in_use) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return pm_conc_connection_list[idx].freq;
		}
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	}

	return 0;
}

QDF_STATUS policy_mgr_skip_dfs_ch(struct wlan_objmgr_psoc *psoc,
				  bool *skip_dfs_channel)
{
	bool sta_sap_scc_on_dfs_chan;
	bool dfs_master_capable;
	QDF_STATUS status;

	status = ucfg_mlme_get_dfs_master_capability(psoc,
						     &dfs_master_capable);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs master capable");
		return status;
	}

	*skip_dfs_channel = false;
	if (!dfs_master_capable) {
		policy_mgr_debug("skip DFS ch for SAP/Go dfs master cap %d",
				 dfs_master_capable);
		*skip_dfs_channel = true;
		return QDF_STATUS_SUCCESS;
	}

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);

	if (policy_mgr_is_hw_dbs_capable(psoc)) {
		if ((policy_mgr_is_special_mode_active_5g(psoc,
							  PM_P2P_CLIENT_MODE) ||
		     policy_mgr_is_special_mode_active_5g(psoc, PM_STA_MODE)) &&
		    !sta_sap_scc_on_dfs_chan) {
			policy_mgr_debug("skip DFS ch from pcl for DBS SAP/Go");
			*skip_dfs_channel = true;
		}
	} else {
		if ((policy_mgr_mode_specific_connection_count(psoc,
							       PM_STA_MODE,
							       NULL) > 0) &&
		    !sta_sap_scc_on_dfs_chan) {
			policy_mgr_debug("skip DFS ch from pcl for non-DBS SAP/Go");
			*skip_dfs_channel = true;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_modify_sap_pcl_based_on_dfs() - filter out DFS channel if needed
 * @psoc: pointer to soc
 * @pcl_list_org: channel list to filter out
 * @weight_list_org: weight of channel list
 * @pcl_len_org: length of channel list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS policy_mgr_modify_sap_pcl_based_on_dfs(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *pcl_list_org,
		uint8_t *weight_list_org,
		uint32_t *pcl_len_org)
{
	size_t i, pcl_len = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool skip_dfs_channel = false;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	status = policy_mgr_skip_dfs_ch(psoc, &skip_dfs_channel);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs channel skip info");
		return status;
	}

	if (!skip_dfs_channel)
		return QDF_STATUS_SUCCESS;

	for (i = 0; i < *pcl_len_org; i++) {
		if (!wlan_reg_is_dfs_in_secondary_list_for_freq(
							pm_ctx->pdev,
							pcl_list_org[i])) {
			pcl_list_org[pcl_len] = pcl_list_org[i];
			weight_list_org[pcl_len++] = weight_list_org[i];
		}
	}

	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_modify_sap_pcl_based_on_nol(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *pcl_list_org,
		uint8_t *weight_list_org,
		uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < *pcl_len_org; i++) {
		if (!wlan_reg_is_disable_in_secondary_list_for_freq(
		    pm_ctx->pdev, pcl_list_org[i])) {
			pcl_list[pcl_len] = pcl_list_org[i];
			weight_list[pcl_len++] = weight_list_org[i];
		}
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
policy_mgr_modify_pcl_based_on_srd(struct wlan_objmgr_psoc *psoc,
				   uint32_t *pcl_list_org,
				   uint8_t *weight_list_org,
				   uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}
	for (i = 0; i < *pcl_len_org; i++) {
		if (wlan_reg_is_etsi_srd_chan_for_freq(
		    pm_ctx->pdev, pcl_list_org[i]))
			continue;
		pcl_list[pcl_len] = pcl_list_org[i];
		weight_list[pcl_len++] = weight_list_org[i];
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_modify_pcl_based_on_indoor() - filter out indoor channel if needed
 * @psoc: pointer to soc
 * @pcl_list_org: channel list to filter out
 * @weight_list_org: weight of channel list
 * @pcl_len_org: length of channel list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
policy_mgr_modify_pcl_based_on_indoor(struct wlan_objmgr_psoc *psoc,
				      uint32_t *pcl_list_org,
				      uint8_t *weight_list_org,
				      uint32_t *pcl_len_org)
{
	uint32_t i, pcl_len = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool include_indoor_channel, sta_sap_scc_on_indoor_channel_allowed;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	status = ucfg_mlme_get_indoor_channel_support(psoc,
						      &include_indoor_channel);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get indoor channel skip info");
		return status;
	}

	/*
	 * If STA SAP scc is allowed on indoor channels, and if STA/P2P
	 * client is present on 5 GHz channel, include indoor channels
	 */
	sta_sap_scc_on_indoor_channel_allowed =
		policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc);
	if (!include_indoor_channel && sta_sap_scc_on_indoor_channel_allowed &&
	    (policy_mgr_is_special_mode_active_5g(psoc, PM_P2P_CLIENT_MODE) ||
	     policy_mgr_is_special_mode_active_5g(psoc, PM_STA_MODE)))
		include_indoor_channel = true;

	if (include_indoor_channel) {
		policy_mgr_debug("Indoor channels allowed. PCL not modified for indoor channels");
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < *pcl_len_org; i++) {
		if (wlan_reg_is_freq_indoor_in_secondary_list(pm_ctx->pdev,
							pcl_list_org[i])) {
			policy_mgr_debug("Remove freq: %d from PCL as it's indoor",
					 pcl_list_org[i]);
			continue;
		}
		pcl_list[pcl_len] = pcl_list_org[i];
		weight_list[pcl_len++] = weight_list_org[i];
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org);
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_modify_sap_pcl_for_6G_channels() - filter out the
 * 6GHz channels where SCC is not supported.
 * @psoc: pointer to soc
 * @pcl_list_org: channel list to filter out
 * @weight_list_org: weight of channel list
 * @pcl_len_org: length of channel list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
policy_mgr_modify_sap_pcl_for_6G_channels(struct wlan_objmgr_psoc *psoc,
					  uint32_t *pcl_list_org,
					  uint8_t *weight_list_org,
					  uint32_t *pcl_len_org)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	uint32_t vdev_id = 0, pcl_len = 0, i;
	struct wlan_objmgr_vdev *vdev;
	qdf_freq_t sta_gc_6ghz_freq = 0;
	uint32_t ap_pwr_type_6g = 0;
	bool indoor_ch_support = false;
	bool keep_6ghz_sta_cli_conn;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((pm_conc_connection_list[i].mode == PM_STA_MODE ||
		    pm_conc_connection_list[i].mode == PM_P2P_CLIENT_MODE) &&
		    pm_conc_connection_list[i].in_use) {
			if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(pm_conc_connection_list[i].freq))
				continue;
			sta_gc_6ghz_freq = pm_conc_connection_list[i].freq;
			vdev_id = pm_conc_connection_list[i].vdev_id;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (!sta_gc_6ghz_freq)
		return QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d is not present", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* If STA is present in 6GHz PSC, STA+SAP SCC is allowed
	 * only for the following combinations:
	 *
	 * VLP STA + SAP - Allowed with VLP Power
	 * LPI STA + SAP - Allowed with VLP power if channel supports VLP.
	 * LPI STA + SAP - Allowed with LPI power if gindoor_channel_support=1
	 */
	ap_pwr_type_6g = wlan_mlme_get_6g_ap_power_type(vdev);
	policy_mgr_debug("STA power type : %d", ap_pwr_type_6g);

	ucfg_mlme_get_indoor_channel_support(psoc, &indoor_ch_support);
	keep_6ghz_sta_cli_conn = wlan_reg_get_keep_6ghz_sta_cli_connection(
								pm_ctx->pdev);
	for (i = 0; i < *pcl_len_org; i++) {
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(pcl_list_org[i])) {
			if (!WLAN_REG_IS_6GHZ_PSC_CHAN_FREQ(pcl_list_org[i]) ||
			    keep_6ghz_sta_cli_conn)
				continue;
			if (ap_pwr_type_6g == REG_VERY_LOW_POWER_AP)
				goto add_freq;
			else if (ap_pwr_type_6g == REG_INDOOR_AP &&
				 (!wlan_reg_is_freq_indoor(pm_ctx->pdev,
							   pcl_list_org[i]) ||
				  indoor_ch_support))
				goto add_freq;
			else
				continue;
		}
add_freq:
		pcl_list[pcl_len] = pcl_list_org[i];
		weight_list[pcl_len++] = weight_list_org[i];
	}

	qdf_mem_zero(pcl_list_org, *pcl_len_org * sizeof(*pcl_list_org));
	qdf_mem_zero(weight_list_org, *pcl_len_org * sizeof(*weight_list_org));
	qdf_mem_copy(pcl_list_org, pcl_list, pcl_len * sizeof(*pcl_list_org));
	qdf_mem_copy(weight_list_org, weight_list, pcl_len);
	*pcl_len_org = pcl_len;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_channel_mcc_with_non_sap() - Helper function to check if channel
 * is MCC with exist non-movable connections.
 * @psoc: pointer to SOC
 * @chan_freq: channel frequency to check
 *
 * Return: true if is MCC with exist non-movable connections, otherwise false.
 */
static bool policy_mgr_channel_mcc_with_non_sap(struct wlan_objmgr_psoc *psoc,
						qdf_freq_t chan_freq)
{
	uint32_t i, connection_of_2ghz = 0;
	qdf_freq_t conc_freq;
	bool is_mcc = false, check_only_dbs = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(pm_conc_connection_list[i].freq))
			connection_of_2ghz++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (connection_of_2ghz >= 2)
		check_only_dbs = true;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use &&
		    (pm_conc_connection_list[i].mode == PM_STA_MODE ||
		     pm_conc_connection_list[i].mode == PM_P2P_CLIENT_MODE ||
		     pm_conc_connection_list[i].mode == PM_P2P_GO_MODE)) {
			conc_freq = pm_conc_connection_list[i].freq;
			if (conc_freq != chan_freq &&
			    ((check_only_dbs &&
			      policy_mgr_2_freq_same_mac_in_dbs(pm_ctx,
								chan_freq,
								conc_freq)) ||
			     policy_mgr_2_freq_always_on_same_mac(psoc,
								  chan_freq,
								  conc_freq))) {
				is_mcc = true;
				break;
			}
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return is_mcc;
}

/**
 * policy_mgr_modify_sap_pcl_filter_mcc() - API to filter out MCC channel with
 * existing non-SAP connection frequency from SAP PCL list.
 * @psoc: pointer to SOC
 * @pcl_list_org: channel list to filter out
 * @weight_list_org: weight of channel list
 * @pcl_len_org: length of channel list
 * @mode: Policy manager connection mode
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
policy_mgr_modify_sap_pcl_filter_mcc(struct wlan_objmgr_psoc *psoc,
				     uint32_t *pcl_list_org,
				     uint8_t *weight_list_org,
				     uint32_t *pcl_len_org,
				     enum policy_mgr_con_mode mode)
{
	uint32_t i, pcl_len = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (mode == PM_LL_LT_SAP_MODE)
		return QDF_STATUS_SUCCESS;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}

	if (!policy_mgr_is_force_scc(psoc)) {
		policy_mgr_debug("force SCC is not prefer, skip!");
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < *pcl_len_org; i++) {
		if (policy_mgr_channel_mcc_with_non_sap(psoc, pcl_list_org[i]))
			continue;

		pcl_list_org[pcl_len] = pcl_list_org[i];
		weight_list_org[pcl_len++] = weight_list_org[i];
	}

	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_modify_sap_go_4th_conc_disallow() - filter out channel that
 * is not allowed for 4th sap/go connection
 * @psoc: pointer to soc
 * @mode: interface mode
 * @pcl_list_org: channel list to filter out
 * @weight_list_org: weight of channel list
 * @pcl_len_org: length of channel list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS policy_mgr_modify_sap_go_4th_conc_disallow(
		struct wlan_objmgr_psoc *psoc,
		enum policy_mgr_con_mode mode,
		uint32_t *pcl_list_org,
		uint8_t *weight_list_org,
		uint32_t *pcl_len_org)
{
	size_t i, pcl_len = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t num_connections;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	if (*pcl_len_org > NUM_CHANNELS) {
		policy_mgr_err("Invalid PCL List Length %d", *pcl_len_org);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	num_connections = policy_mgr_get_connection_count(psoc);
	if (num_connections < 3)
		goto end;

	for (i = 0; i < *pcl_len_org; i++) {
		if (policy_mgr_allow_4th_new_freq(psoc, pcl_list_org[i],
						  mode, 0)) {
			pcl_list_org[pcl_len] = pcl_list_org[i];
			weight_list_org[pcl_len++] = weight_list_org[i];
		}
	}

	*pcl_len_org = pcl_len;
end:
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_pcl_modification_for_sap(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *pcl_channels, uint8_t *pcl_weight,
			uint32_t *len, uint32_t weight_len,
			enum policy_mgr_con_mode mode)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool mandatory_modified_pcl = false;
	bool nol_modified_pcl = false;
	bool dfs_modified_pcl = false;
	bool indoor_modified_pcl = false;
	bool passive_modified_pcl = false;
	bool band_6ghz_modified_pcl = false;
	bool fourth_conc_modified_pcl = false;
	bool modified_final_pcl = false;
	bool srd_chan_enabled;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	/* check the channel avoidance list for beaconing entities */
	policy_mgr_update_with_safe_channel_list(psoc, pcl_channels,
						 len, pcl_weight, weight_len);

	if (policy_mgr_is_sap_mandatory_channel_set(psoc)) {
		status = policy_mgr_modify_sap_pcl_based_on_mandatory_channel(
				psoc, pcl_channels, pcl_weight, len);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err(
				"failed to get mandatory modified pcl for SAP");
			return status;
		}
		mandatory_modified_pcl = true;
	}

	status = policy_mgr_modify_sap_pcl_based_on_nol(
			psoc, pcl_channels, pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get nol modified pcl for SAP");
		return status;
	}
	nol_modified_pcl = true;

	status = policy_mgr_modify_sap_pcl_based_on_dfs(
			psoc, pcl_channels, pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs modified pcl for SAP");
		return status;
	}
	dfs_modified_pcl = true;

	wlan_mlme_get_srd_master_mode_for_vdev(psoc, QDF_SAP_MODE,
					       &srd_chan_enabled);

	if (!srd_chan_enabled) {
		status = policy_mgr_modify_pcl_based_on_srd
				(psoc, pcl_channels, pcl_weight, len);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Failed to modify SRD in pcl for SAP");
			return status;
		}
	}

	status = policy_mgr_modify_pcl_based_on_indoor(psoc, pcl_channels,
						       pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get indoor modified pcl for SAP");
		return status;
	}
	indoor_modified_pcl = true;

	status = policy_mgr_filter_passive_ch(pm_ctx->pdev,
					      pcl_channels, len);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to filter passive channels");
		return INVALID_CHANNEL_ID;
	}
	passive_modified_pcl = true;

	status = policy_mgr_modify_sap_pcl_for_6G_channels(psoc,
							   pcl_channels,
							   pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to modify pcl for 6G channels");
		return status;
	}
	band_6ghz_modified_pcl = true;

	status = policy_mgr_modify_sap_go_4th_conc_disallow(psoc,
							    PM_SAP_MODE,
							    pcl_channels,
							    pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to modify pcl for 4th sap channels");
		return status;
	}
	fourth_conc_modified_pcl = true;

	status = policy_mgr_modify_sap_pcl_filter_mcc(psoc,
						      pcl_channels,
						      pcl_weight, len,
						      mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to modify pcl for filter mcc");
		return status;
	}

	modified_final_pcl = true;
	policy_mgr_debug("%d %d %d %d %d %d %d %d",
			 mandatory_modified_pcl,
			 nol_modified_pcl,
			 dfs_modified_pcl,
			 indoor_modified_pcl,
			 passive_modified_pcl,
			 band_6ghz_modified_pcl,
			 fourth_conc_modified_pcl,
			 modified_final_pcl);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_pcl_modification_for_p2p_go(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *pcl_channels, uint8_t *pcl_weight,
			uint32_t *len, uint32_t weight_len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	bool srd_chan_enabled;

	/* check the channel avoidance list for beaconing entities */
	policy_mgr_update_with_safe_channel_list(psoc, pcl_channels,
						 len, pcl_weight, weight_len);

	status = policy_mgr_modify_pcl_based_on_enabled_channels(
			psoc, pcl_channels, pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl for GO");
		return status;
	}

	status = policy_mgr_modify_sap_pcl_based_on_dfs(
			psoc, pcl_channels, pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get dfs modified pcl for GO");
		return status;
	}

	wlan_mlme_get_srd_master_mode_for_vdev(psoc, QDF_P2P_GO_MODE,
					       &srd_chan_enabled);

	if (!srd_chan_enabled) {
		status = policy_mgr_modify_pcl_based_on_srd
				(psoc, pcl_channels, pcl_weight, len);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Failed to modify SRD in pcl for GO");
			return status;
		}
	}
	status = policy_mgr_modify_sap_go_4th_conc_disallow(psoc,
							    PM_P2P_GO_MODE,
							    pcl_channels,
							    pcl_weight, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to modify pcl for 4th go channels");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_LL_LT_SAP
#ifdef WLAN_FEATURE_LL_LT_SAP_6G_SUPPORT
static bool policy_mgr_is_6G_chan_valid_for_ll_sap(qdf_freq_t freq)
{
	if (!wlan_reg_is_6ghz_chan_freq(freq))
		return true;

	if (wlan_reg_is_6ghz_psc_chan_freq(freq) &&
	    wlan_reg_is_6ghz_unii5_chan_freq(freq))
		return true;

	return false;
}
#else
static inline bool policy_mgr_is_6G_chan_valid_for_ll_sap(qdf_freq_t freq)
{
	if (!wlan_reg_is_6ghz_chan_freq(freq))
		return true;

	return false;
}
#endif

static bool policy_mgr_is_dynamic_sbs_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	return (policy_mgr_is_hw_sbs_capable(psoc) &&
		policy_mgr_can_2ghz_share_low_high_5ghz_sbs(pm_ctx));
}

/**
 * policy_mgr_is_sbs_mac0_freq() - Check if the given frequency is
 * sbs frequency on mac0 for static sbs case.
 * @psoc: psoc pointer
 * @freq: Frequency which needs to be checked.
 *
 * Return: true/false.
 */
static bool policy_mgr_is_sbs_mac0_freq(struct wlan_objmgr_psoc *psoc,
					qdf_freq_t freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_freq_range *freq_range;

	if (policy_mgr_is_dynamic_sbs_enabled(psoc))
		return false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	freq_range = pm_ctx->hw_mode.freq_range_caps[MODE_SBS];

	if (policy_mgr_is_freq_on_mac_id(freq_range, freq, 0))
		return true;

	return false;
}

static QDF_STATUS policy_mgr_pcl_modification_for_ll_lt_sap(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *pcl_channels, uint8_t *pcl_weight,
			uint32_t *len, uint32_t weight_len)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t pcl_list[NUM_CHANNELS], orig_len = *len;
	uint8_t weight_list[NUM_CHANNELS];
	uint32_t i, pcl_len = 0;
	bool sbs_mac0_modified_pcl = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_pcl_modification_for_sap(
		psoc, pcl_channels, pcl_weight, len, weight_len,
		PM_LL_LT_SAP_MODE);

	for (i = 0; i < *len; i++) {
		/* Remove passive/dfs/6G invalid channel for LL_LT_SAP */
		if (wlan_reg_is_24ghz_ch_freq(pcl_channels[i]) ||
		    wlan_reg_is_passive_for_freq(
					pm_ctx->pdev,
					pcl_channels[i]) ||
		    wlan_reg_is_dfs_for_freq(
					pm_ctx->pdev,
					pcl_channels[i]) ||
		    !policy_mgr_is_6G_chan_valid_for_ll_sap(pcl_channels[i]))
			continue;

		/* Remove mac0 frequencies for static SBS case */
		if (policy_mgr_is_sbs_mac0_freq(psoc, pcl_channels[i])) {
			sbs_mac0_modified_pcl = true;
			continue;
		}

		pcl_list[pcl_len] = pcl_channels[i];
		weight_list[pcl_len++] = pcl_weight[i];
	}

	if (orig_len == pcl_len)
		return QDF_STATUS_SUCCESS;

	qdf_mem_zero(pcl_channels, *len * sizeof(*pcl_channels));
	qdf_mem_zero(pcl_weight, *len);
	qdf_mem_copy(pcl_channels, pcl_list, pcl_len * sizeof(*pcl_channels));
	qdf_mem_copy(pcl_weight, weight_list, pcl_len);
	*len = pcl_len;

	policy_mgr_debug("sbs_mac0_modified_pcl %d, PCL after ll sap modification",
			 sbs_mac0_modified_pcl);
	policy_mgr_dump_channel_list(*len, pcl_channels, pcl_weight);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
policy_mgr_pcl_modification_for_ll_lt_sap(struct wlan_objmgr_psoc *psoc,
					  uint32_t *pcl_channels,
					  uint8_t *pcl_weight,
					  uint32_t *len, uint32_t weight_len)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS policy_mgr_mode_specific_modification_on_pcl(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *pcl_channels, uint8_t *pcl_weight,
			uint32_t *len, uint32_t weight_len,
			enum policy_mgr_con_mode mode)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	switch (mode) {
	case PM_SAP_MODE:
		status = policy_mgr_pcl_modification_for_sap(
			psoc, pcl_channels, pcl_weight, len, weight_len, mode);
		break;
	case PM_P2P_GO_MODE:
		status = policy_mgr_pcl_modification_for_p2p_go(
			psoc, pcl_channels, pcl_weight, len, weight_len);
		break;
	case PM_STA_MODE:
	case PM_P2P_CLIENT_MODE:
	case PM_NAN_DISC_MODE:
		status = QDF_STATUS_SUCCESS;
		break;
	case PM_LL_LT_SAP_MODE:
		status = policy_mgr_pcl_modification_for_ll_lt_sap(
			psoc, pcl_channels, pcl_weight, len, weight_len);
		break;
	default:
		policy_mgr_err("unexpected mode %d", mode);
		break;
	}

	return status;
}

#ifdef FEATURE_FOURTH_CONNECTION
static enum policy_mgr_pcl_type policy_mgr_get_pcl_4_port(
				struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				enum policy_mgr_conc_priority_mode pref)
{
	enum policy_mgr_three_connection_mode fourth_index = 0;
	enum policy_mgr_pcl_type pcl;

	/* Will be enhanced for other types of 4 port conc (NaN etc.)
	 * in future.
	 */
	if (!policy_mgr_is_hw_dbs_capable(psoc)) {
		policy_mgr_err("Can't find index for 4th port pcl table for non dbs capable");
		return PM_MAX_PCL_TYPE;
	}

	/* SAP and P2P Go have same result in 4th port pcl table */
	if (mode == PM_SAP_MODE || mode == PM_P2P_GO_MODE)
		mode = PM_SAP_MODE;
	else if (mode == PM_P2P_CLIENT_MODE)
		mode = PM_STA_MODE;

	if (mode != PM_STA_MODE && mode != PM_SAP_MODE &&
	    mode != PM_NDI_MODE) {
		policy_mgr_err("Can't start 4th port if not STA, SAP, NDI");
		return PM_MAX_PCL_TYPE;
	}

	fourth_index =
		policy_mgr_get_fourth_connection_pcl_table_index(psoc);
	if (PM_MAX_THREE_CONNECTION_MODE == fourth_index) {
		policy_mgr_err("Can't find index for 4th port pcl table");
		return PM_MAX_PCL_TYPE;
	}
	policy_mgr_debug("Index for 4th port pcl table: %d", fourth_index);

	pcl = fourth_connection_pcl_dbs_sbs_table[fourth_index][mode][pref];

	return pcl;
}
#else
static inline enum policy_mgr_pcl_type policy_mgr_get_pcl_4_port(
				struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				enum policy_mgr_conc_priority_mode pref)
{return PM_MAX_PCL_TYPE; }
#endif

QDF_STATUS policy_mgr_get_pcl(struct wlan_objmgr_psoc *psoc,
			      enum policy_mgr_con_mode mode,
			      uint32_t *pcl_channels, uint32_t *len,
			      uint8_t *pcl_weight, uint32_t weight_len,
			      uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t num_connections = 0;
	enum policy_mgr_conc_priority_mode first_index = 0;
	enum policy_mgr_one_connection_mode second_index = 0;
	enum policy_mgr_two_connection_mode third_index = 0;
	enum policy_mgr_pcl_type pcl = PM_NONE;
	enum policy_mgr_conc_priority_mode conc_system_pref = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum QDF_OPMODE qdf_mode;
	uint32_t orig_pcl_len;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return status;
	}

	if ((mode < 0) || (mode >= PM_MAX_NUM_OF_MODE)) {
		policy_mgr_err("Invalid connection mode %d received", mode);
		return status;
	}

	/* find the current connection state from pm_conc_connection_list*/
	num_connections = policy_mgr_get_connection_count(psoc);
	policy_mgr_debug("connections:%d pref:%d requested mode:%d vdev_id:%d",
			 num_connections, pm_ctx->cur_conc_system_pref, mode,
			 vdev_id);

	switch (pm_ctx->cur_conc_system_pref) {
	case 0:
		conc_system_pref = PM_THROUGHPUT;
		break;
	case 1:
		conc_system_pref = PM_POWERSAVE;
		break;
	case 2:
		conc_system_pref = PM_LATENCY;
		break;
	default:
		policy_mgr_err("unknown cur_conc_system_pref value %d",
			pm_ctx->cur_conc_system_pref);
		break;
	}

	switch (num_connections) {
	case 0:
		first_index =
			policy_mgr_get_first_connection_pcl_table_index(psoc);
		pcl = first_connection_pcl_table[mode][first_index];
		break;
	case 1:
		second_index =
			policy_mgr_get_second_connection_pcl_table_index(psoc);
		if (PM_MAX_ONE_CONNECTION_MODE == second_index) {
			policy_mgr_err("couldn't find index for 2nd connection pcl table");
			return status;
		}
		qdf_mode = policy_mgr_get_qdf_mode_from_pm(mode);
		if (qdf_mode == QDF_MAX_NO_OF_MODE)
			return status;

		if (policy_mgr_is_hw_dbs_capable(psoc) == true &&
		    policy_mgr_is_dbs_allowed_for_concurrency(
							psoc, qdf_mode)) {
			pcl = (*second_connection_pcl_dbs_table)
				[second_index][mode][conc_system_pref];
		} else {
			pcl = (*second_connection_pcl_non_dbs_table)
				[second_index][mode][conc_system_pref];
		}

		break;
	case 2:
		third_index =
			policy_mgr_get_third_connection_pcl_table_index(psoc);
		if (PM_MAX_TWO_CONNECTION_MODE == third_index) {
			policy_mgr_err(
				"couldn't find index for 3rd connection pcl table");
			return status;
		}
		if (policy_mgr_is_hw_dbs_capable(psoc) == true) {
			pcl = (*third_connection_pcl_dbs_table)
				[third_index][mode][conc_system_pref];
		} else {
			pcl = (*third_connection_pcl_non_dbs_table)
				[third_index][mode][conc_system_pref];
		}
		break;
	case 3:
		pcl = policy_mgr_get_pcl_4_port(psoc, mode, conc_system_pref);
		break;
	default:
		policy_mgr_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	/* once the PCL enum is obtained find out the exact channel list with
	 * help from sme_get_cfg_valid_channels
	 */
	status = policy_mgr_get_channel_list(psoc, pcl, mode, pcl_channels,
					     pcl_weight, weight_len, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get channel list:%d", status);
		return status;
	}

	if (!*len) {
		policymgr_nofl_debug("Total PCL Chan %d", *len);
		return QDF_STATUS_SUCCESS;
	}
	orig_pcl_len = *len;
	policy_mgr_dump_channel_list(*len, pcl_channels, pcl_weight);
	policy_mgr_mode_specific_modification_on_pcl(
		psoc, pcl_channels, pcl_weight, len, weight_len, mode);

	status = policy_mgr_modify_pcl_based_on_dnbs(psoc, pcl_channels,
						pcl_weight, len);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl based on DNBS");
		return status;
	}

	if (orig_pcl_len != *len) {
		policy_mgr_debug("PCL after modification");
		policy_mgr_dump_channel_list(*len, pcl_channels, pcl_weight);
	}

	return QDF_STATUS_SUCCESS;
}

enum policy_mgr_conc_priority_mode
		policy_mgr_get_first_connection_pcl_table_index(
		struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return PM_THROUGHPUT;
	}

	if (pm_ctx->cur_conc_system_pref >= PM_MAX_CONC_PRIORITY_MODE)
		return PM_THROUGHPUT;

	return pm_ctx->cur_conc_system_pref;
}

enum policy_mgr_one_connection_mode
		policy_mgr_get_second_connection_pcl_table_index(
		struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_one_connection_mode index = PM_MAX_ONE_CONNECTION_MODE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return index;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (PM_STA_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_24_1x1;
			else
				index = PM_STA_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_STA_5_1x1;
			else
				index = PM_STA_5_2x2;
		}
	} else if (PM_SAP_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_24_1x1;
			else
				index = PM_SAP_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_SAP_5_1x1;
			else
				index = PM_SAP_5_2x2;
		}
	} else if (PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_24_1x1;
			else
				index = PM_P2P_CLI_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_5_1x1;
			else
				index = PM_P2P_CLI_5_2x2;
		}
	} else if (PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_24_1x1;
			else
				index = PM_P2P_GO_24_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_5_1x1;
			else
				index = PM_P2P_GO_5_2x2;
		}
	} else if (PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_24_1x1;
		else
			index = PM_NAN_DISC_24_2x2;
	} else if (PM_LL_LT_SAP_MODE == pm_conc_connection_list[0].mode) {
		index = PM_LL_LT_SAP_5_2x2;
	}

	policy_mgr_debug("mode:%d freq:%d chain:%d index:%d",
			 pm_conc_connection_list[0].mode,
			 pm_conc_connection_list[0].freq,
			 pm_conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return index;
}

/*
 * policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc() -
 * This function checks connection mode is in scc or not and returns
 * index value based on mode and prvided index inputs.
 *
 * @scc_2g_1x1: index of scc_2g_1x1 for provided concurrency
 * @scc_2g_2x2: index of scc_2g_2x2 for provided concurrency
 * @scc_5g_1x1: index of scc_5g_1x1 for provided concurrency
 * @scc_5g_2x2: index of scc_5g_2x2 for provided concurrency
 *
 * Return: policy_mgr_two_connection_mode index
 */
static enum policy_mgr_two_connection_mode
policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
			enum policy_mgr_two_connection_mode scc_2g_1x1,
			enum policy_mgr_two_connection_mode scc_2g_2x2,
			enum policy_mgr_two_connection_mode scc_5g_1x1,
			enum policy_mgr_two_connection_mode scc_5g_2x2)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	if (pm_conc_connection_list[0].freq ==
	    pm_conc_connection_list[1].freq) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) {
			if (POLICY_MGR_ONE_ONE ==
					pm_conc_connection_list[0].chain_mask)
				index = scc_2g_1x1;
			else
				index = scc_2g_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = scc_5g_1x1;
			else
				index = scc_5g_2x2;
		}
	}
	return index;
}

/*
 * policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc() -
 * This function checks connection mode is in mcc or not and returns
 * index value based on mode and prvided index inputs.
 *
 * @mcc_2g_1x1: index of mcc_2g_1x1 for provided concurrency
 * @mcc_2g_2x2: index of mcc_2g_2x2 for provided concurrency
 * @mcc_5g_1x1: index of mcc_5g_1x1 for provided concurrency
 * @mcc_5g_2x2: index of mcc_5g_2x2 for provided concurrency
 * @mcc_24_5_1x1: index of mcc_24_5_1x1 for provided concurrency
 * @mcc_24_5_2x2: index of mcc_24_5_2x2 for provided concurrency
 *
 * Return: policy_mgr_two_connection_mode index
 */
static enum policy_mgr_two_connection_mode
policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(
			struct wlan_objmgr_psoc *psoc,
			enum policy_mgr_two_connection_mode mcc_2g_1x1,
			enum policy_mgr_two_connection_mode mcc_2g_2x2,
			enum policy_mgr_two_connection_mode mcc_5g_1x1,
			enum policy_mgr_two_connection_mode mcc_5g_2x2,
			enum policy_mgr_two_connection_mode mcc_24_5_1x1,
			enum policy_mgr_two_connection_mode mcc_24_5_2x2)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	if (policy_mgr_are_2_freq_on_same_mac(psoc,
					      pm_conc_connection_list[0].freq,
					      pm_conc_connection_list[1].freq)
					     ) {
		if ((WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    (WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
					pm_conc_connection_list[0].chain_mask)
				index = mcc_2g_1x1;
			else
				index = mcc_2g_2x2;
		} else if (!(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq)) &&
			   !(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
					pm_conc_connection_list[0].chain_mask)
				index = mcc_5g_1x1;
			else
				index = mcc_5g_2x2;
		} else {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = mcc_24_5_1x1;
			else
				index = mcc_24_5_2x2;
		}
	}
	return index;
}

/*
 * policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs() -
 * This function checks connection mode is in dbs or sbs and returns index
 * value based on mode and prvided index inputs.
 *
 * @sbs_5g_1x1: index of sbs_5g_1x1 for provided concurrency
 * @sbs_5g_2x2: index of sbs_5g_2x2 for provided concurrency
 * @dbs_1x1: index of dbs_1x1 for provided concurrency
 * @dbs_2x2: index of dbs_2x2 for provided concurrency
 *
 * Return: policy_mgr_two_connection_mode index
 */
static enum policy_mgr_two_connection_mode
policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(
			struct wlan_objmgr_psoc *psoc,
			enum policy_mgr_two_connection_mode sbs_5g_1x1,
			enum policy_mgr_two_connection_mode sbs_5g_2x2,
			enum policy_mgr_two_connection_mode dbs_1x1,
			enum policy_mgr_two_connection_mode dbs_2x2)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	if (!policy_mgr_are_2_freq_on_same_mac(psoc,
					       pm_conc_connection_list[0].freq,
					       pm_conc_connection_list[1].freq)
					      ) {
		/* SBS */
		if (!(WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    !(WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = sbs_5g_1x1;
			else
				index = sbs_5g_2x2;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = dbs_1x1;
			else
				index = dbs_2x2;
		}
	}

	return index;
}

/*
 * policy_mgr_get_3rd_pcl_table_index_for_dbs_with_ml_sta() -
 * Get third connection pcl table index when ML STA is present
 * @sbs_5g_1x1: index of sbs_5g_1x1 for provided concurrency
 * @sbs_5g_2x2: index of sbs_5g_2x2 for provided concurrency
 * @dbs_1x1: index of dbs_1x1 for provided concurrency
 * @dbs_2x2: index of dbs_2x2 for provided concurrency
 *
 * This function checks connection mode is in dbs or sbs when two ML STA links
 * are active and returns index value based on mode and provided index inputs.
 *
 * Return: policy_mgr_two_connection_mode index
 */
static enum policy_mgr_two_connection_mode
policy_mgr_get_3rd_pcl_table_index_for_dbs_with_ml_sta(
			struct wlan_objmgr_psoc *psoc,
			enum policy_mgr_two_connection_mode sbs_5g_1x1,
			enum policy_mgr_two_connection_mode sbs_5g_2x2,
			enum policy_mgr_two_connection_mode dbs_1x1,
			enum policy_mgr_two_connection_mode dbs_2x2)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	if (!policy_mgr_2_freq_always_on_same_mac(
				psoc,
				pm_conc_connection_list[0].freq,
				pm_conc_connection_list[1].freq)) {
		/* SBS */
		if (!(WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[0].freq)) &&
		    !(WLAN_REG_IS_24GHZ_CH_FREQ(
		    pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = sbs_5g_1x1;
			else
				index = sbs_5g_2x2;
		} else {
		/* DBS */
			if (POLICY_MGR_ONE_ONE ==
				pm_conc_connection_list[0].chain_mask)
				index = dbs_1x1;
			else
				index = dbs_2x2;
		}
	}

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_cli_sap(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_P2P_CLI_SAP_SCC_24_1x1,
					PM_P2P_CLI_SAP_SCC_24_2x2,
					PM_P2P_CLI_SAP_SCC_5_1x1,
					PM_P2P_CLI_SAP_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_P2P_CLI_SAP_MCC_24_1x1,
					PM_P2P_CLI_SAP_MCC_24_2x2,
					PM_P2P_CLI_SAP_MCC_5_1x1,
					PM_P2P_CLI_SAP_MCC_5_2x2,
					PM_P2P_CLI_SAP_MCC_24_5_1x1,
					PM_P2P_CLI_SAP_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_P2P_CLI_SAP_SBS_5_1x1,
					PM_P2P_CLI_SAP_SBS_5_2x2,
					PM_P2P_CLI_SAP_DBS_1x1,
					PM_P2P_CLI_SAP_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sta_sap(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_STA_SAP_SCC_24_1x1,
					PM_STA_SAP_SCC_24_2x2,
					PM_STA_SAP_SCC_5_1x1,
					PM_STA_SAP_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_STA_SAP_MCC_24_1x1,
					PM_STA_SAP_MCC_24_2x2,
					PM_STA_SAP_MCC_5_1x1,
					PM_STA_SAP_MCC_5_2x2,
					PM_STA_SAP_MCC_24_5_1x1,
					PM_STA_SAP_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_STA_SAP_SBS_5_1x1,
					PM_STA_SAP_SBS_5_2x2,
					PM_STA_SAP_DBS_1x1,
					PM_STA_SAP_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sap_sap(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_SAP_SAP_SCC_24_1x1,
					PM_SAP_SAP_SCC_24_2x2,
					PM_SAP_SAP_SCC_5_1x1,
					PM_SAP_SAP_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_SAP_SAP_MCC_24_1x1,
					PM_SAP_SAP_MCC_24_2x2,
					PM_SAP_SAP_MCC_5_1x1,
					PM_SAP_SAP_MCC_5_2x2,
					PM_SAP_SAP_MCC_24_5_1x1,
					PM_SAP_SAP_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_SAP_SAP_SBS_5_1x1,
					PM_SAP_SAP_SBS_5_2x2,
					PM_SAP_SAP_DBS_1x1,
					PM_SAP_SAP_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sta_go(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_STA_P2P_GO_SCC_24_1x1,
					PM_STA_P2P_GO_SCC_24_2x2,
					PM_STA_P2P_GO_SCC_5_1x1,
					PM_STA_P2P_GO_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_STA_P2P_GO_MCC_24_1x1,
					PM_STA_P2P_GO_MCC_24_2x2,
					PM_STA_P2P_GO_MCC_5_1x1,
					PM_STA_P2P_GO_MCC_5_2x2,
					PM_STA_P2P_GO_MCC_24_5_1x1,
					PM_STA_P2P_GO_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_STA_P2P_GO_SBS_5_1x1,
					PM_STA_P2P_GO_SBS_5_2x2,
					PM_STA_P2P_GO_DBS_1x1,
					PM_STA_P2P_GO_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sta_cli(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_STA_P2P_CLI_SCC_24_1x1,
					PM_STA_P2P_CLI_SCC_24_2x2,
					PM_STA_P2P_CLI_SCC_5_1x1,
					PM_STA_P2P_CLI_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_STA_P2P_CLI_MCC_24_1x1,
					PM_STA_P2P_CLI_MCC_24_2x2,
					PM_STA_P2P_CLI_MCC_5_1x1,
					PM_STA_P2P_CLI_MCC_5_2x2,
					PM_STA_P2P_CLI_MCC_24_5_1x1,
					PM_STA_P2P_CLI_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_STA_P2P_CLI_SBS_5_1x1,
					PM_STA_P2P_CLI_SBS_5_2x2,
					PM_STA_P2P_CLI_DBS_1x1,
					PM_STA_P2P_CLI_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_go_cli(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_P2P_GO_P2P_CLI_SCC_24_1x1,
					PM_P2P_GO_P2P_CLI_SCC_24_2x2,
					PM_P2P_GO_P2P_CLI_SCC_5_1x1,
					PM_P2P_GO_P2P_CLI_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_P2P_GO_P2P_CLI_MCC_24_1x1,
					PM_P2P_GO_P2P_CLI_MCC_24_2x2,
					PM_P2P_GO_P2P_CLI_MCC_5_1x1,
					PM_P2P_GO_P2P_CLI_MCC_5_2x2,
					PM_P2P_GO_P2P_CLI_MCC_24_5_1x1,
					PM_P2P_GO_P2P_CLI_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_P2P_GO_P2P_CLI_SBS_5_1x1,
					PM_P2P_GO_P2P_CLI_SBS_5_2x2,
					PM_P2P_GO_P2P_CLI_DBS_1x1,
					PM_P2P_GO_P2P_CLI_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_go_sap(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_P2P_GO_SAP_SCC_24_1x1,
					PM_P2P_GO_SAP_SCC_24_2x2,
					PM_P2P_GO_SAP_SCC_5_1x1,
					PM_P2P_GO_SAP_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_P2P_GO_SAP_MCC_24_1x1,
					PM_P2P_GO_SAP_MCC_24_2x2,
					PM_P2P_GO_SAP_MCC_5_1x1,
					PM_P2P_GO_SAP_MCC_5_2x2,
					PM_P2P_GO_SAP_MCC_24_5_1x1,
					PM_P2P_GO_SAP_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_P2P_GO_SAP_SBS_5_1x1,
					PM_P2P_GO_SAP_SBS_5_2x2,
					PM_P2P_GO_SAP_DBS_1x1,
					PM_P2P_GO_SAP_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sta_sta(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;

	if (policy_mgr_is_ml_vdev_id(psoc,
				     pm_conc_connection_list[0].vdev_id) &&
	    policy_mgr_is_ml_vdev_id(psoc,
				     pm_conc_connection_list[1].vdev_id)) {
		index =
		policy_mgr_get_3rd_pcl_table_index_for_dbs_with_ml_sta(
					psoc,
					PM_STA_STA_SBS_5_1x1,
					PM_STA_STA_SBS_5_2x2,
					PM_STA_STA_DBS_1x1,
					PM_STA_STA_DBS_2x2);
		if (index != PM_MAX_TWO_CONNECTION_MODE)
			return index;
	}
	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_STA_STA_SCC_24_1x1,
					PM_STA_STA_SCC_24_2x2,
					PM_STA_STA_SCC_5_1x1,
					PM_STA_STA_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_STA_STA_MCC_24_1x1,
					PM_STA_STA_MCC_24_2x2,
					PM_STA_STA_MCC_5_1x1,
					PM_STA_STA_MCC_5_2x2,
					PM_STA_STA_MCC_24_5_1x1,
					PM_STA_STA_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_STA_STA_SBS_5_1x1,
					PM_STA_STA_SBS_5_2x2,
					PM_STA_STA_DBS_1x1,
					PM_STA_STA_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_cli_cli(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_P2P_CLI_P2P_CLI_SCC_24_1x1,
					PM_P2P_CLI_P2P_CLI_SCC_24_2x2,
					PM_P2P_CLI_P2P_CLI_SCC_5_1x1,
					PM_P2P_CLI_P2P_CLI_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_P2P_CLI_P2P_CLI_MCC_24_1x1,
					PM_P2P_CLI_P2P_CLI_MCC_24_2x2,
					PM_P2P_CLI_P2P_CLI_MCC_5_1x1,
					PM_P2P_CLI_P2P_CLI_MCC_5_2x2,
					PM_P2P_CLI_P2P_CLI_MCC_24_5_1x1,
					PM_P2P_CLI_P2P_CLI_MCC_24_5_2x2);

	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_P2P_CLI_P2P_CLI_SBS_5_1x1,
					PM_P2P_CLI_P2P_CLI_SBS_5_2x2,
					PM_P2P_CLI_P2P_CLI_DBS_1x1,
					PM_P2P_CLI_P2P_CLI_DBS_2x2);

	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_go_go(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_scc(
					PM_P2P_GO_P2P_GO_SCC_24_1x1,
					PM_P2P_GO_P2P_GO_SCC_24_2x2,
					PM_P2P_GO_P2P_GO_SCC_5_1x1,
					PM_P2P_GO_P2P_GO_SCC_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_mcc(psoc,
					PM_P2P_GO_P2P_GO_MCC_24_1x1,
					PM_P2P_GO_P2P_GO_MCC_24_2x2,
					PM_P2P_GO_P2P_GO_MCC_5_1x1,
					PM_P2P_GO_P2P_GO_MCC_5_2x2,
					PM_P2P_GO_P2P_GO_MCC_24_5_1x1,
					PM_P2P_GO_P2P_GO_MCC_24_5_2x2);
	if (index != PM_MAX_TWO_CONNECTION_MODE)
		return index;

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(psoc,
					PM_P2P_GO_P2P_GO_SBS_5_1x1,
					PM_P2P_GO_P2P_GO_SBS_5_2x2,
					PM_P2P_GO_P2P_GO_DBS_1x1,
					PM_P2P_GO_P2P_GO_DBS_2x2);
	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_nan_ndi(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_NDI_SCC_24_1x1;
		else
			index = PM_NAN_DISC_NDI_SCC_24_2x2;
	/* MCC */
	} else if (policy_mgr_are_2_freq_on_same_mac(psoc,
			pm_conc_connection_list[0].freq,
			pm_conc_connection_list[1].freq)) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_NDI_MCC_24_1x1;
		else
			index = PM_NAN_DISC_NDI_MCC_24_2x2;
	/* DBS */
	} else {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_NAN_DISC_NDI_DBS_1x1;
		else
			index = PM_NAN_DISC_NDI_DBS_2x2;
	}
	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sta_nan(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_STA_NAN_DISC_SCC_24_1x1;
		else
			index = PM_STA_NAN_DISC_SCC_24_2x2;
	/* MCC */
	} else if (policy_mgr_are_2_freq_on_same_mac(psoc,
			pm_conc_connection_list[0].freq,
			pm_conc_connection_list[1].freq)) {
		/* Policy mgr only considers NAN Disc ch in 2.4 GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_STA_NAN_DISC_MCC_24_1x1;
		else
			index = PM_STA_NAN_DISC_MCC_24_2x2;
	/* DBS */
	} else {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_STA_NAN_DISC_DBS_1x1;
		else
			index = PM_STA_NAN_DISC_DBS_2x2;
	}
	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sap_nan(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	/* SCC */
	if (pm_conc_connection_list[0].freq ==
		pm_conc_connection_list[1].freq) {
		/* Policy mgr only considers NAN Disc ch in 2.4 GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_SAP_NAN_DISC_SCC_24_1x1;
		else
			index = PM_SAP_NAN_DISC_SCC_24_2x2;
	/* MCC */
	} else if (policy_mgr_are_2_freq_on_same_mac(psoc,
			pm_conc_connection_list[0].freq,
			pm_conc_connection_list[1].freq)) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_SAP_NAN_DISC_MCC_24_1x1;
		else
			index = PM_SAP_NAN_DISC_MCC_24_2x2;
	/* DBS */
	} else {
		if (POLICY_MGR_ONE_ONE == pm_conc_connection_list[0].chain_mask)
			index = PM_SAP_NAN_DISC_DBS_1x1;
		else
			index = PM_SAP_NAN_DISC_DBS_2x2;
	}
	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sta_ll_lt_sap(
						struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;
	enum policy_mgr_two_connection_mode sbs_5g_1x1;
	enum policy_mgr_two_connection_mode sbs_5g_2x2;
	qdf_freq_t sta_freq, sbs_cut_off_freq;

	/*
	 * LL_LT_SAP can not be in SCC so there will not be any scc index.
	 * With LL_LT_SAP, MCC is possible only on 5 GHz
	 */
	if (policy_mgr_are_2_freq_on_same_mac(
					psoc,
					pm_conc_connection_list[0].freq,
					pm_conc_connection_list[1].freq)) {
		if (!(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq)) &&
			   !(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
					pm_conc_connection_list[0].chain_mask)
				index = PM_STA_5_LL_LT_SAP_MCC_1x1;
			else
				index = PM_STA_5_LL_LT_SAP_MCC_2x2;

			return index;
		}
	}

	if (pm_conc_connection_list[0].mode == PM_STA_MODE)
		sta_freq = pm_conc_connection_list[0].freq;
	else
		sta_freq = pm_conc_connection_list[1].freq;

	sbs_cut_off_freq =  policy_mgr_get_sbs_cut_off_freq(psoc);

	if (sta_freq < sbs_cut_off_freq) {
		sbs_5g_1x1 = PM_STA_5_LOW_LL_LT_SAP_5_HIGH_SBS_1x1;
		sbs_5g_2x2 = PM_STA_5_LOW_LL_LT_SAP_5_HIGH_SBS_2x2;
	} else {
		sbs_5g_1x1 = PM_STA_5_HIGH_LL_LT_SAP_5_LOW_SBS_1x1;
		sbs_5g_2x2 = PM_STA_5_HIGH_LL_LT_SAP_5_LOW_SBS_2x2;
	}

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(
						psoc, sbs_5g_1x1, sbs_5g_2x2,
						PM_STA_24_LL_LT_SAP_DBS_1x1,
						PM_STA_24_LL_LT_SAP_DBS_2x2);
	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_sap_ll_lt_sap(
						struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;
	enum policy_mgr_two_connection_mode sbs_5g_1x1;
	enum policy_mgr_two_connection_mode sbs_5g_2x2;
	qdf_freq_t sap_freq, sbs_cut_off_freq;

	if (pm_conc_connection_list[0].mode == PM_SAP_MODE)
		sap_freq = pm_conc_connection_list[0].freq;
	else
		sap_freq = pm_conc_connection_list[1].freq;

	sbs_cut_off_freq =  policy_mgr_get_sbs_cut_off_freq(psoc);

	if (sap_freq < sbs_cut_off_freq) {
		sbs_5g_1x1 = PM_SAP_5_LOW_LL_LT_SAP_5_HIGH_SBS_1x1;
		sbs_5g_2x2 = PM_SAP_5_LOW_LL_LT_SAP_5_HIGH_SBS_2x2;
	} else {
		sbs_5g_1x1 = PM_SAP_5_HIGH_LL_LT_SAP_5_LOW_SBS_1x1;
		sbs_5g_2x2 = PM_SAP_5_HIGH_LL_LT_SAP_5_LOW_SBS_2x2;
	}

	/*
	 * LL_LT_SAP can not be in SCC so there will not be any scc index.
	 * For LL_LT_SAP + SAP, MCC is not possible, so there will be only
	 * sbs or dbs index
	 */

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(
						psoc, sbs_5g_1x1, sbs_5g_2x2,
						PM_SAP_24_LL_LT_SAP_DBS_1x1,
						PM_SAP_24_LL_LT_SAP_DBS_2x2);
	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_go_ll_lt_sap(
						struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;
	enum policy_mgr_two_connection_mode sbs_5g_1x1;
	enum policy_mgr_two_connection_mode sbs_5g_2x2;
	qdf_freq_t go_freq, sbs_cut_off_freq;

	/*
	 * LL_LT_SAP can not be in SCC so there will not be any scc index.
	 * With LL_LT_SAP, MCC is possible only on 5 GHz
	 */
	if (policy_mgr_are_2_freq_on_same_mac(
					psoc,
					pm_conc_connection_list[0].freq,
					pm_conc_connection_list[1].freq)) {
		if (!(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq)) &&
			   !(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
					pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_GO_5_LL_LT_SAP_MCC_1x1;
			else
				index = PM_P2P_GO_5_LL_LT_SAP_MCC_2x2;

			return index;
		}
	}

	if (pm_conc_connection_list[0].mode == PM_P2P_GO_MODE)
		go_freq = pm_conc_connection_list[0].freq;
	else
		go_freq = pm_conc_connection_list[1].freq;

	sbs_cut_off_freq =  policy_mgr_get_sbs_cut_off_freq(psoc);

	if (go_freq < sbs_cut_off_freq) {
		sbs_5g_1x1 = PM_P2P_GO_5_LOW_LL_LT_SAP_5_HIGH_SBS_1x1;
		sbs_5g_2x2 = PM_P2P_GO_5_LOW_LL_LT_SAP_5_HIGH_SBS_2x2;
	} else {
		sbs_5g_1x1 = PM_P2P_GO_5_HIGH_LL_LT_SAP_5_LOW_SBS_1x1;
		sbs_5g_2x2 = PM_P2P_GO_5_HIGH_LL_LT_SAP_5_LOW_SBS_2x2;
	}

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(
						psoc, sbs_5g_1x1, sbs_5g_2x2,
						PM_P2P_GO_24_LL_LT_SAP_DBS_1x1,
						PM_P2P_GO_24_LL_LT_SAP_DBS_2x2);
	return index;
}

static enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index_cli_ll_lt_sap(
						struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index;
	enum policy_mgr_two_connection_mode sbs_5g_1x1;
	enum policy_mgr_two_connection_mode sbs_5g_2x2;
	qdf_freq_t cli_freq, sbs_cut_off_freq;

	/*
	 * LL_LT_SAP can not be in SCC so there will not be any scc index.
	 * With LL_LT_SAP, MCC is possible only on 5 GHz
	 */
	if (policy_mgr_are_2_freq_on_same_mac(
					psoc,
					pm_conc_connection_list[0].freq,
					pm_conc_connection_list[1].freq)) {
		if (!(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[0].freq)) &&
			   !(WLAN_REG_IS_24GHZ_CH_FREQ(
			   pm_conc_connection_list[1].freq))) {
			if (POLICY_MGR_ONE_ONE ==
					pm_conc_connection_list[0].chain_mask)
				index = PM_P2P_CLI_5_LL_LT_SAP_MCC_1x1;
			else
				index = PM_P2P_CLI_5_LL_LT_SAP_MCC_2x2;

			return index;
		}
	}

	if (pm_conc_connection_list[0].mode == PM_P2P_GO_MODE)
		cli_freq = pm_conc_connection_list[0].freq;
	else
		cli_freq = pm_conc_connection_list[1].freq;

	sbs_cut_off_freq =  policy_mgr_get_sbs_cut_off_freq(psoc);

	if (cli_freq < sbs_cut_off_freq) {
		sbs_5g_1x1 = PM_P2P_GO_5_LOW_LL_LT_SAP_5_HIGH_SBS_1x1;
		sbs_5g_2x2 = PM_P2P_GO_5_LOW_LL_LT_SAP_5_HIGH_SBS_2x2;
	} else {
		sbs_5g_1x1 = PM_P2P_GO_5_HIGH_LL_LT_SAP_5_LOW_SBS_1x1;
		sbs_5g_2x2 = PM_P2P_GO_5_HIGH_LL_LT_SAP_5_LOW_SBS_2x2;
	}

	index =
	policy_mgr_check_and_get_third_connection_pcl_table_index_for_dbs(
					psoc, sbs_5g_1x1, sbs_5g_2x2,
					PM_P2P_CLI_24_LL_LT_SAP_DBS_1x1,
					PM_P2P_CLI_24_LL_LT_SAP_DBS_2x2);
	return index;
}

enum policy_mgr_two_connection_mode
policy_mgr_get_third_connection_pcl_table_index(
					struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_two_connection_mode index = PM_MAX_TWO_CONNECTION_MODE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return index;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (((PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_cli_sap(psoc);
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_sap(psoc);
	else if ((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sap_sap(psoc);
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_go(psoc);
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_cli(psoc);
	else if (((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_go_cli(psoc);
	else if (((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		(PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) &&
		(PM_SAP_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_go_sap(psoc);
	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_sta(psoc);
	else if (((PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) &&
		(PM_STA_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		(PM_NAN_DISC_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sta_nan(psoc);
	else if (((PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) &&
		(PM_NDI_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_NDI_MODE == pm_conc_connection_list[0].mode) &&
		(PM_NAN_DISC_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_nan_ndi(psoc);
	else if (((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		  (PM_NAN_DISC_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_NAN_DISC_MODE == pm_conc_connection_list[0].mode) &&
		 (PM_SAP_MODE == pm_conc_connection_list[1].mode)))
		index =
		policy_mgr_get_third_connection_pcl_table_index_sap_nan(psoc);
	else if ((pm_conc_connection_list[0].mode == PM_P2P_GO_MODE) &&
		 (pm_conc_connection_list[1].mode == PM_P2P_GO_MODE))
		index =
		policy_mgr_get_third_connection_pcl_table_index_go_go(psoc);

	else if ((pm_conc_connection_list[0].mode == PM_P2P_CLIENT_MODE) &&
		 (pm_conc_connection_list[1].mode == PM_P2P_CLIENT_MODE))
		index =
		policy_mgr_get_third_connection_pcl_table_index_cli_cli(psoc);

	else if (((PM_STA_MODE == pm_conc_connection_list[0].mode) &&
		  (PM_LL_LT_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_LL_LT_SAP_MODE == pm_conc_connection_list[0].mode) &&
		 (PM_STA_MODE == pm_conc_connection_list[1].mode)))
		index = policy_mgr_get_third_connection_pcl_table_index_sta_ll_lt_sap(psoc);

	else if (((PM_SAP_MODE == pm_conc_connection_list[0].mode) &&
		  (PM_LL_LT_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_LL_LT_SAP_MODE == pm_conc_connection_list[0].mode) &&
		 (PM_SAP_MODE == pm_conc_connection_list[1].mode)))
		index = policy_mgr_get_third_connection_pcl_table_index_sap_ll_lt_sap(psoc);

	else if (((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode) &&
		  (PM_LL_LT_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_LL_LT_SAP_MODE == pm_conc_connection_list[0].mode) &&
		 (PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)))
		index = policy_mgr_get_third_connection_pcl_table_index_go_ll_lt_sap(psoc);

	else if (((PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode) &&
		  (PM_LL_LT_SAP_MODE == pm_conc_connection_list[1].mode)) ||
		((PM_LL_LT_SAP_MODE == pm_conc_connection_list[0].mode) &&
		 (PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)))
		index = policy_mgr_get_third_connection_pcl_table_index_cli_ll_lt_sap(psoc);

	policy_mgr_debug("mode0:%d mode1:%d freq0:%d freq1:%d chain:%d index:%d",
			 pm_conc_connection_list[0].mode,
			 pm_conc_connection_list[1].mode,
			 pm_conc_connection_list[0].freq,
			 pm_conc_connection_list[1].freq,
			 pm_conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return index;
}

#ifdef FEATURE_FOURTH_CONNECTION
/**
 * policy_mgr_get_index_for_3_given_freq_dbs() - Find the index for next
 * connection for given 3 freq in DBS mode
 * @pm_ctx: policy manager context
 * @index: Index to return for next connection
 * @freq1: freq of interface 1
 * @freq2: freq of interface 2
 * @freq3: freq of interface 3
 *
 * This function finds the index for next connection for 3 freq in DBS mode.
 *
 * Return: none
 */
static void
policy_mgr_get_index_for_3_given_freq_dbs(
	struct policy_mgr_psoc_priv_obj *pm_ctx,
	enum policy_mgr_three_connection_mode *index,
	qdf_freq_t freq1, qdf_freq_t freq2, qdf_freq_t freq3)
{
	/* If all freq are on same band */
	if ((WLAN_REG_IS_24GHZ_CH_FREQ(freq1) ==
	     WLAN_REG_IS_24GHZ_CH_FREQ(freq2) &&
	    (WLAN_REG_IS_24GHZ_CH_FREQ(freq2) ==
	     WLAN_REG_IS_24GHZ_CH_FREQ(freq3)))) {
		policy_mgr_err("Invalid mode for all freq %d, %d and %d on same band",
			       freq1, freq2, freq3);
		return;
	}

	/*
	 * If freq1 and freq2 are on same band and freq3 is on differet band and
	 * is not sharing mac with any SAP. STA on same band is handled above,
	 * so both SAP on same band mean STA cannot be on same band. This can
	 * happen if SBS is not enabled.
	 */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(freq1) ==
	    WLAN_REG_IS_24GHZ_CH_FREQ(freq2)) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(freq3))
			/*
			 * As all 3 cannot be on same band, so if freq3 is
			 * 2.4 GHZ mean both freq1 and freq2 are on 5 / 6 GHZ
			 */
			*index = PM_5_SCC_MCC_PLUS_24_DBS;
		else
			/*
			 * As all 3 cannot be on same band, so if freq3 is
			 * 5 / 6 GHZ, mean both freq1 and freq2 are on 2.4 GHZ.
			 */
			*index = PM_24_SCC_MCC_PLUS_5_DBS;
		return;
	}

	/*
	 * if freq1 and freq 2 are on different band (2 GHZ + 5 GHZ/6 GHZ DBS),
	 * check with which freq the freq3 will share mac, and return index as
	 * per it.
	 */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(freq3))
		*index = PM_24_SCC_MCC_PLUS_5_DBS;
	else
		*index = PM_5_SCC_MCC_PLUS_24_DBS;
}

/**
 * policy_mgr_get_index_for_3_given_freq_sbs() - Find the index for next
 * connection for 3 given freq, in case current HW mode is SBS
 * @pm_ctx: policy manager context
 * @index: Index to return for next connection
 * @freq1: freq of interface 1
 * @freq2: freq of interface 2
 * @freq3: freq of interface 3
 *
 * This function finds the index for next
 * connection for 3 given freq, in case current HW mode is SBS
 *
 * Return: none
 */
static void policy_mgr_get_index_for_3_given_freq_sbs(
	struct policy_mgr_psoc_priv_obj *pm_ctx,
	enum policy_mgr_three_connection_mode *index,
	qdf_freq_t freq1, qdf_freq_t freq2, qdf_freq_t freq3)
{
	qdf_freq_t sbs_cut_off_freq;
	qdf_freq_t shared_5_ghz_freq = 0;

	/*
	 * Sanity check: At least 2 of the given freq needs to be creating SBS
	 * separation for HW mode to be in SBS, if not it shouldn't have
	 * entered this API.
	 */
	if (!policy_mgr_are_sbs_chan(pm_ctx->psoc, freq1, freq2) &&
	    !policy_mgr_are_sbs_chan(pm_ctx->psoc, freq2, freq3) &&
	    !policy_mgr_are_sbs_chan(pm_ctx->psoc, freq3, freq1)) {
		policy_mgr_err("freq1 %d, freq2 %d and freq3 %d, none of the 2 connections/3 vdevs are leading to SBS",
			       freq1, freq2, freq3);
		return;
	}

	sbs_cut_off_freq =  policy_mgr_get_sbs_cut_off_freq(pm_ctx->psoc);
	if (!sbs_cut_off_freq) {
		policy_mgr_err("Invalid cutoff freq");
		return;
	}

	/*
	 * If dynamic SBS is enabled (2.4 GHZ can share mac with HIGH
	 * 5GHZ as well as LOW 5 GHZ, but one at a time) and one of the
	 * freq is 2.4 GHZ, this mean that the new interface can come up on
	 * 5 GHZ LOW or HIGH and HW mode will move the 2.4 GHZ link to
	 * the other mac dynamically.
	 */
	if (policy_mgr_can_2ghz_share_low_high_5ghz_sbs(pm_ctx) &&
	    (WLAN_REG_IS_24GHZ_CH_FREQ(freq1) ||
	     WLAN_REG_IS_24GHZ_CH_FREQ(freq2) ||
	     WLAN_REG_IS_24GHZ_CH_FREQ(freq3))) {
		*index = PM_24_5_PLUS_5_LOW_N_HIGH_SHARE_SBS;
		return;
	}
	/*
	 * if freq1 on freq2 same mac, get the 5 / 6 GHZ freq from it check
	 * and determine shared mac.
	 */
	if (policy_mgr_2_freq_same_mac_in_sbs(pm_ctx, freq1, freq2)) {
		/*
		 * If freq1 is 2.4 GHZ that mean freq2 is 5 / 6 GHZ.
		 * so take decision using freq2.
		 */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(freq1))
			shared_5_ghz_freq = freq2;
		else
			/* freq1 5 / 6 GHZ, use freq1 */
			shared_5_ghz_freq = freq1;
	} else if (policy_mgr_2_freq_same_mac_in_sbs(pm_ctx, freq2, freq3)) {
		/*
		 * If freq2 is 2.4 GHZ that mean freq3 is 5 / 6 GHZ.
		 * so take decision using freq3.
		 */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(freq2))
			shared_5_ghz_freq = freq3;
		else
			/* freq2 5 / 6 GHZ, use freq1 */
			shared_5_ghz_freq = freq2;
	} else if (policy_mgr_2_freq_same_mac_in_sbs(pm_ctx, freq3, freq1)) {
		/*
		 * If freq1 is 2.4 GHZ that mean freq3 is 5 / 6 GHZ.
		 * so take decision using freq3.
		 */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(freq1))
			shared_5_ghz_freq = freq3;
		else
			/* freq1 5 / 6 GHZ, use freq1 */
			shared_5_ghz_freq = freq1;
	}

	if (!shared_5_ghz_freq ||
	    WLAN_REG_IS_24GHZ_CH_FREQ(shared_5_ghz_freq)) {
		policy_mgr_err("shared_5_ghz_freq %d is not 5 / 6 GHZ",
			       shared_5_ghz_freq);
		return;
	}

	/* If shared 5 / 6 GHZ freq is low 5 GHZ, then return high 5 GHZ freq */
	if (shared_5_ghz_freq < sbs_cut_off_freq)
		*index = PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS;
	else
		*index = PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * policy_mgr_get_index_for_ml_sta_sap_dbs() - Find the index for next
 * connection for ML STA + SAP, in case current HW mode is DBS and ML STA is
 * 2.4 GHZ + 5 GHZ/6 GHZ OR if SBS is not supported.
 * @pm_ctx: policy manager context
 * @index: Index to return for next connection
 * @sap_freq: SAP freq
 * @sta_freq_list: STA freq list
 * @ml_sta_idx: ML STA index in freq_list
 *
 * This function finds the index for next
 * connection for ML STA + SAP, in case current HW mode is DBS and ML STA is
 * 2.4 GHZ + 5 GHZ/6 GHZ OR if SBS is not supported.
 *
 * Return: none
 */
static void
policy_mgr_get_index_for_ml_sta_sap_dbs(
	struct policy_mgr_psoc_priv_obj *pm_ctx,
	enum policy_mgr_three_connection_mode *index, qdf_freq_t sap_freq,
	qdf_freq_t *sta_freq_list, uint8_t *ml_sta_idx)
{
	/* If ML STA and SAP all are on same band */
	if ((WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[0]]) ==
	     WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[1]])) &&
	    (WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[0]]) ==
	     WLAN_REG_IS_24GHZ_CH_FREQ(sap_freq))) {
		policy_mgr_err("Invalid mode for ML STA %d and %d are on same band as SAP %d",
			       sta_freq_list[ml_sta_idx[0]],
			       sta_freq_list[ml_sta_idx[1]], sap_freq);
		return;
	}

	/*
	 * If ML STA is MCC and SAP is on differet band and is not sharing mac
	 * with any link. SAP on same band is handled above, so ML STA on same
	 * band mean SAP cannot be on same band. This can happen if SBS is not
	 * enabled.
	 */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[0]]) ==
	    WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[1]])) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(sap_freq))
			/*
			 * As all 3 cannot be on same band, so if SAP is 2.4 GHZ
			 * mean both ML STA are on 5 / 6 GHZ
			 */
			*index = PM_STA_STA_5_SAP_24_DBS;
		else
			/*
			 * As all 3 cannot be on same band, so if SAP is
			 *  5 / 6 GHZ, mean both ML STA are on 2.4 GHZ.
			 */
			policy_mgr_err("Invalid mode for ML STA %d and %d are on 2.4 GHZ, sap freq %d",
				       sta_freq_list[ml_sta_idx[0]],
				       sta_freq_list[ml_sta_idx[1]], sap_freq);

		return;
	}

	/*
	 * if ML STA is 2 GHZ + 5 GHZ/6 GHZ DBS, check with which freq the SAP
	 * will share mac, and return index as per it.
	 */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(sap_freq))
		*index = PM_STA_SAP_24_STA_5_DBS;
	else
		*index = PM_STA_SAP_5_STA_24_DBS;
}

/**
 * policy_mgr_get_index_for_ml_sta_sap_hwmode_sbs() - Find the index for next
 * connection for ML STA + SAP, in case current HW mode is SBS but ML STA is
 * with 2 GHz + 5/6 GHz.
 * @pm_ctx: policy manager context
 * @index: Index to return for next connection
 * @sap_freq: SAP freq
 * @sta_freq_list: STA freq list
 * @ml_sta_idx: ML STA index in freq_list
 *
 * This function finds the index for next connection for ML STA + SAP,
 * in case current HW mode is SBS but ML STA is with 2 GHz + 5/6 GHz.
 *
 * Return: none
 */
static void policy_mgr_get_index_for_ml_sta_sap_hwmode_sbs(
	struct policy_mgr_psoc_priv_obj *pm_ctx,
	enum policy_mgr_three_connection_mode *index, qdf_freq_t sap_freq,
	qdf_freq_t *sta_freq_list, uint8_t *ml_sta_idx)
{
	bool sbs_24_shared_high_support =
		policy_mgr_sbs_24_shared_with_high_5(pm_ctx);
	bool sbs_24_shared_low_support =
		policy_mgr_sbs_24_shared_with_low_5(pm_ctx);
	qdf_freq_t sbs_cut_off_freq, ml_sta_5g_freq;
	bool ml_sta_5g_low;

	/* HW supports sbs but ml sta 2 home channels are not in sbs frequency
	 * separation by check policy_mgr_are_sbs_chan.
	 * It means one ml sta is 2.4 GHz, the other is 5/6 GHz.
	 * The combinations handled by this API:
	 * 2.4 GHz band    |  5 GHz low band  | 5/6 GHz high band |   PCL list
	 * ----------------------------------------------------------------------
	 * ML STA          |  ML STA+SAP      |               |   5 GHz High + 2.4 GHz
	 * ML STA          |  ML STA+SAP      |               |   2.4 GHz(nhss)
	 * ML STA          |  ML STA          | SAP           |   5 GHz Low + 2.4 GHz
	 * ML STA          |  ML STA          | SAP           |   2.4 GHz (nhss)
	 * ML STA          |  SAP             | ML STA        |   5 GHz High+ 2.4 GHz
	 * ML STA          |  SAP             | ML STA        |   2.4 GHz (nlss)
	 * ML STA          |                  | ML STA+SAP    |   5 GHz Low + 2.4 GHz
	 * ML STA          |                  | ML STA+SAP    |   2.4 GHz (nlss)
	 * ML STA+SAP      |  ML STA          |               |   5 GHz Low+5 GHz High
	 * ML STA+SAP      |                  | ML STA        |   5 GHz Low+5 GHz High
	 *
	 * nhss: no high share supported
	 * nlss: no low share supported
	 */

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[0]]) &&
	    !WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[1]])) {
		policy_mgr_err("unexpected ml sta home freq to handle (%d %d)",
			       sta_freq_list[ml_sta_idx[0]],
			       sta_freq_list[ml_sta_idx[1]]);
		return;
	}
	if (!sbs_24_shared_low_support && !sbs_24_shared_high_support) {
		policy_mgr_err("unexpected sbs mode: low share %d high share %d",
			       sbs_24_shared_low_support,
			       sbs_24_shared_high_support);
		return;
	}
	sbs_cut_off_freq =  policy_mgr_get_sbs_cut_off_freq(pm_ctx->psoc);
	if (!sbs_cut_off_freq) {
		policy_mgr_err("Invalid cutoff freq");
		return;
	}

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[0]]))
		ml_sta_5g_freq = sta_freq_list[ml_sta_idx[0]];
	else
		ml_sta_5g_freq = sta_freq_list[ml_sta_idx[1]];
	if (ml_sta_5g_freq < sbs_cut_off_freq)
		ml_sta_5g_low = true;
	else
		ml_sta_5g_low = false;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(sap_freq)) {
		/* pcl 5 GHz Low+5 GHz High - PM_SCC_ON_5_CH_5G */
		*index = PM_24_5_PLUS_5_LOW_N_HIGH_SHARE_SBS;
	} else if (sap_freq < sbs_cut_off_freq) {
		if ((ml_sta_5g_low && sbs_24_shared_high_support) ||
		    (!ml_sta_5g_low && sbs_24_shared_low_support))
			/* pcl 5 GHz High + 2.4 GHz -
			 * PM_SCC_ON_5G_HIGH_5G_HIGH_PLUS_SHARED_2G
			 */
			*index = PM_STA_24_STA_5_MCC_SAP_5_LOW_SBS;
		else
			*index = PM_24_5_PLUS_5_LOW_OR_HIGH_SHARE_SBS;
	} else {
		if ((ml_sta_5g_low && sbs_24_shared_high_support) ||
		    (!ml_sta_5g_low && sbs_24_shared_low_support))
			/* pcl 5 GHz Low + 2.4 GHz -
			 * PM_SCC_ON_5G_LOW_5G_LOW_PLUS_SHARED_2G
			 */
			*index = PM_STA_24_STA_5_MCC_SAP_5_HIGH_SBS;
		else
			*index = PM_24_5_PLUS_5_LOW_OR_HIGH_SHARE_SBS;
	}
	policy_mgr_debug("4th index %d sap freq %d ml sta 5g %d sbs_cut_off_freq %d support high share %d low share %d",
			 *index, sap_freq, ml_sta_5g_freq, sbs_cut_off_freq,
			 sbs_24_shared_high_support,
			 sbs_24_shared_low_support);
}

/**
 * policy_mgr_get_index_for_ml_sta_sap_sbs() - Find the index for next
 * connection for ML STA + SAP, in case current HW mode is SBS or ML STA is
 * with 5 GHZ + 5 GHZ/6 GHZ SBS separation.
 * @pm_ctx: policy manager context
 * @index: Index to return for next connection
 * @sap_freq: SAP freq
 * @sta_freq_list: STA freq list
 * @ml_sta_idx: ML STA index in freq_list
 *
 * This function finds the index for next
 * connection for ML STA + SAP, in case current HW mode is SBS or ML STA is
 * with 5 GHZ + 5 GHZ/6 GHZ SBS separation.
 *
 * Return: none
 */
static void policy_mgr_get_index_for_ml_sta_sap_sbs(
	struct policy_mgr_psoc_priv_obj *pm_ctx,
	enum policy_mgr_three_connection_mode *index, qdf_freq_t sap_freq,
	qdf_freq_t *sta_freq_list, uint8_t *ml_sta_idx)
{
	qdf_freq_t sbs_cut_off_freq;
	bool can_2ghz_share_low_high_5ghz =
			policy_mgr_can_2ghz_share_low_high_5ghz_sbs(pm_ctx);

	/*
	 * Sanity check: At least one of the 3 combo (ML STA OR SAP + one of
	 * ML STA link) needs to be creating SBS separation for
	 * HW mode to be in SBS, if not it shouldn't have entered this API.
	 */
	if (!policy_mgr_are_sbs_chan(pm_ctx->psoc, sta_freq_list[ml_sta_idx[0]],
				     sta_freq_list[ml_sta_idx[1]]) &&
	    !policy_mgr_are_sbs_chan(pm_ctx->psoc, sap_freq,
				     sta_freq_list[ml_sta_idx[0]]) &&
	    !policy_mgr_are_sbs_chan(pm_ctx->psoc, sap_freq,
				     sta_freq_list[ml_sta_idx[1]])) {
		if (policy_mgr_is_current_hwmode_sbs(pm_ctx->psoc)) {
			policy_mgr_get_index_for_ml_sta_sap_hwmode_sbs(
				pm_ctx, index, sap_freq, sta_freq_list,
				ml_sta_idx);
			return;
		}
		policy_mgr_err("SAP freq (%d) and ML STA freq %d and %d, none of the 2 connections/3 vdevs are leading to SBS",
			       sap_freq,
			       sta_freq_list[ml_sta_idx[0]],
			       sta_freq_list[ml_sta_idx[1]]);
		return;
	}

	sbs_cut_off_freq =  policy_mgr_get_sbs_cut_off_freq(pm_ctx->psoc);
	if (!sbs_cut_off_freq) {
		policy_mgr_err("Invalid cutoff freq");
		return;
	}

	if (WLAN_REG_IS_24GHZ_CH_FREQ(sap_freq)) {
		/*
		 * If dynamic SBS is enabled (2.4 GHZ can share mac with HIGH
		 * 5GHZ as well as LOW 5 GHZ, but one at a time) and SAP is
		 * 2.4 GHZ, this mean that the new SAP can come up on 5 GHZ LOW
		 * or HIGH and HW mode will move the 2.4 GHZ SAP to the other
		 * mac dynamically.
		 */
		if (can_2ghz_share_low_high_5ghz) {
			*index = PM_SAP_24_STA_5_STA_5_LOW_N_HIGH_SHARE_SBS;
			return;
		}
		/*
		 * if SAP is 2.4 GHZ that means both ML STA needs to
		 * be with 5 GHZ + 5 GHZ/6 GHZ SBS separation. If not, it would
		 * have failed the sanity check. So Get STA link with
		 * which SAP freq is sharing mac and select index accordingly
		 */
		if (policy_mgr_2_freq_same_mac_in_sbs(
						pm_ctx, sap_freq,
						sta_freq_list[ml_sta_idx[0]])) {
			/*
			 * SAP is sharig mac with link ml_sta_idx[0], so check
			 * if ml_sta_idx[0] is lower 5 GHZ or high 5 GHZ and
			 * select index
			 */
			if (sta_freq_list[ml_sta_idx[0]] < sbs_cut_off_freq)
				*index = PM_STA_5_LOW_SAP_24_MCC_STA_5_HIGH_SBS;
			else
				*index = PM_STA_5_HIGH_SAP_24_MCC_STA_5_LOW_SBS;
		} else {
			/*
			 * SAP is sharig mac with link ml_sta_idx[1], so check
			 * if ml_sta_idx[1] is lower 5 GHZ or high 5 GHZ and
			 * select index
			 */
			if (sta_freq_list[ml_sta_idx[1]] < sbs_cut_off_freq)
				*index = PM_STA_5_LOW_SAP_24_MCC_STA_5_HIGH_SBS;
			else
				*index = PM_STA_5_HIGH_SAP_24_MCC_STA_5_LOW_SBS;
		}

		return;
	}

	/* SAP freq is 5 GHZ or 6 GHZ and one ML sta is on 2.4 GHZ */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[0]]) ||
	    WLAN_REG_IS_24GHZ_CH_FREQ(sta_freq_list[ml_sta_idx[1]])) {
		/*
		 * If dynamic SBS is enabled (2.4 GHZ can share mac with HIGH
		 * 5GHZ as well as LOW 5 GHZ, but one at a time) and one STA
		 * link is 2.4 GHZ, this mean that the new SAP can come up on
		 * 5 GHZ LOW or HIGH and HW mode will move the 2.4 GHZ link to
		 * the other mac dynamically.
		 */
		if (can_2ghz_share_low_high_5ghz) {
			*index = PM_STA_24_SAP_5_STA_5_LOW_N_HIGH_SHARE_SBS;
			return;
		}
		/*
		 * If (2 GHZ + 5 GHZ/6 GHZ) ML is MCC i.e Both sta links are on
		 * same mac and SAP is on separate mac. This can happen if SBS
		 * is not dynamic and is low 5 GHZ shared, with ML STA on 5 GHZ
		 * low along with SAP 5Ghz high and vice versa. As both links
		 * of ML STA and sap are on different mac, so set index based
		 * on sap frequency whether it is in 5 GHZ low band or 5 GHZ
		 * high Band.
		 */
		if (policy_mgr_2_freq_same_mac_in_sbs(
					pm_ctx, sta_freq_list[ml_sta_idx[0]],
					sta_freq_list[ml_sta_idx[1]])) {
			if (sap_freq < sbs_cut_off_freq)
				*index = PM_STA_24_STA_5_HIGH_MCC_SAP_5_LOW_SBS;
			else
				*index = PM_STA_24_STA_5_LOW_MCC_SAP_5_HIGH_SBS;
		} else {
			/*
			 * STA ML is (2 GHZ + 5 GHZ/6 GHZ) select as per the SAP
			 * freq, as for current mode to be in SBS, SAP will
			 * share mac with STA 2.4 GHZ Link or will not share
			 * mac at all (2 GHZ + 5 GHZ/6 GHZ MCC). Other case i.e
			 * 2 links on 5 GHZ and one STA link on 2 GHZ is DBS and
			 * sanity check already taken care this at start of the
			 * func.
			 */
			if (sap_freq < sbs_cut_off_freq)
				*index = PM_STA_24_SAP_5_LOW_MCC_STA_5_HIGH_SBS;
			else
				*index = PM_STA_24_SAP_5_HIGH_MCC_STA_5_LOW_SBS;
		}
		return;
	}
	/*
	 * If (5 GHZ + 5 GHZ/6 GHZ) ML is MCC i.e SAP is not sharing mac
	 * with any link. This can happen if SBS is not dynamic and is
	 * low 5 GHZ shared, with ML STA on 5 GHZ low along with SAP 5 GHZ
	 * high and vice versa. As both ML sta link are on same mac and
	 * sap is on different mac, so decide whether ML links are sharing
	 * low 5 GHZ frequency or high 5 GHZ frequency based on sap frequency
	 */
	if (policy_mgr_2_freq_same_mac_in_sbs(
				pm_ctx, sta_freq_list[ml_sta_idx[0]],
				sta_freq_list[ml_sta_idx[1]])) {
		if (sap_freq < sbs_cut_off_freq)
			*index = PM_STA_STA_5_HIGH_MCC_SAP_5_LOW_SBS;
		else
			*index = PM_STA_STA_5_LOW_MCC_SAP_5_HIGH_SBS;
	} else {
		/*
		 * STA ML is (5 GHZ + 5 GHZ/6 GHZ SBS/MCC), select as per the
		 * SAP freq, as for current mode to be in SBS, SAP will share
		 * mac with the corresponding low/high 5 GHZ
		 * (5 GHZ + 5 GHZ/6 GHZ SBS) or will not share mac at all
		 * (5 GHZ +5 GHZ/6 GHZ MCC). Other case sanity already
		 * taken care at start of the func.
		 */
		if (sap_freq < sbs_cut_off_freq)
			*index = PM_STA_SAP_5_LOW_STA_5_HIGH_SBS;
		else
			*index = PM_STA_SAP_5_HIGH_STA_5_LOW_SBS;
	}
}

static void
policy_mgr_get_index_for_ml_sta_sap(
			struct policy_mgr_psoc_priv_obj *pm_ctx,
			enum policy_mgr_three_connection_mode *index,
			qdf_freq_t sap_freq, qdf_freq_t *sta_freq_list,
			uint8_t *ml_sta_idx)
{
	/*
	 * P2P GO/P2P CLI are treated as SAP to optimize as pcl
	 * table is same for all three.
	 */
	policy_mgr_debug("channel: sap0: %d, ML STA link0: %d, ML STA link1: %d",
			 sap_freq, sta_freq_list[ml_sta_idx[0]],
			 sta_freq_list[ml_sta_idx[1]]);
	if (policy_mgr_is_current_hwmode_sbs(pm_ctx->psoc) ||
	    policy_mgr_are_sbs_chan(pm_ctx->psoc, sta_freq_list[ml_sta_idx[0]],
				    sta_freq_list[ml_sta_idx[1]])) {
		/* if current mode is SBS or the ML STA is 5+5/6Ghz SBS */
		policy_mgr_get_index_for_ml_sta_sap_sbs(pm_ctx, index,
							sap_freq,
							sta_freq_list,
							ml_sta_idx);
		return;
	}
	/*
	 * current HW mode is DBS and ML STA is 2.4 GHZ + 5 GHZ or SBS
	 * is not enabled
	 */
	policy_mgr_get_index_for_ml_sta_sap_dbs(pm_ctx, index, sap_freq,
						sta_freq_list, ml_sta_idx);
}

static void
policy_mgr_get_index_for_ml_sta_sap_sap(
			struct policy_mgr_psoc_priv_obj *pm_ctx,
			enum policy_mgr_three_connection_mode *index,
			qdf_freq_t ml_sta_freq, qdf_freq_t sap_freq_1,
			qdf_freq_t sap_freq_2)
{
	/*
	 * P2P GO/P2P CLI are treated as SAP to optimize as pcl
	 * table is same for all three.
	 */
	policy_mgr_debug("channel: ML sta0: %d, SAP0: %d, SAP1: %d",
			 ml_sta_freq, sap_freq_1, sap_freq_2);
	if (policy_mgr_is_current_hwmode_sbs(pm_ctx->psoc))
		/* if current mode is SBS */
		return policy_mgr_get_index_for_3_given_freq_sbs(pm_ctx, index,
							ml_sta_freq, sap_freq_1,
							sap_freq_2);

	/* current HW mode is DBS */
	policy_mgr_get_index_for_3_given_freq_dbs(pm_ctx, index, ml_sta_freq,
						  sap_freq_1, sap_freq_2);
}

#else /* WLAN_FEATURE_11BE_MLO */

static inline void
policy_mgr_get_index_for_ml_sta_sap(
			struct policy_mgr_psoc_priv_obj *pm_ctx,
			enum policy_mgr_three_connection_mode *index,
			qdf_freq_t sap_freq, qdf_freq_t *sta_freq_list,
			uint8_t *ml_sta_idx) {}

static inline void
policy_mgr_get_index_for_ml_sta_sap_sap(
			struct policy_mgr_psoc_priv_obj *pm_ctx,
			enum policy_mgr_three_connection_mode *index,
			qdf_freq_t ml_sta_freq, qdf_freq_t sap_freq_1,
			qdf_freq_t sap_freq_2) {}
#endif /* WLAN_FEATURE_11BE_MLO */

enum policy_mgr_three_connection_mode
		policy_mgr_get_fourth_connection_pcl_table_index(
		struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_three_connection_mode index =
			PM_MAX_THREE_CONNECTION_MODE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t count_sap = 0;
	uint32_t count_sta = 0;
	uint32_t count_ndi = 0;
	uint32_t count_nan_disc = 0;
	uint8_t num_ml_sta = 0, num_non_ml_sta = 0;
	uint32_t list_sap[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t list_sta[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t list_ndi[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t list_nan_disc[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t ml_sta_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t non_ml_sta_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t sap_freq = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return index;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	/* For 4 port concurrency case,
	 * 1st step: (SAP+STA)(2.4G MAC SCC) + (SAP+STA)(5G MAC SCC)
	 * 2nd step: (AGO+STA)(2.4G MAC SCC) + (AGO+STA)(5G MAC SCC)
	 */
	count_sap += policy_mgr_mode_specific_connection_count(
				psoc, PM_SAP_MODE, &list_sap[count_sap]);
	count_sap += policy_mgr_mode_specific_connection_count(
				psoc, PM_P2P_GO_MODE, &list_sap[count_sap]);
	count_sap += policy_mgr_mode_specific_connection_count(
				psoc, PM_P2P_CLIENT_MODE, &list_sap[count_sap]);
	count_sta = policy_mgr_mode_specific_connection_count(
				psoc, PM_STA_MODE, list_sta);
	policy_mgr_get_ml_and_non_ml_sta_count(psoc, &num_ml_sta, ml_sta_idx,
					       &num_non_ml_sta, non_ml_sta_idx,
					       freq_list, vdev_id_list);

	count_ndi = policy_mgr_mode_specific_connection_count(
				psoc, PM_NDI_MODE, list_ndi);
	count_nan_disc = policy_mgr_mode_specific_connection_count(
				psoc, PM_NAN_DISC_MODE, list_nan_disc);
	policy_mgr_debug("sap/go/cli:%d sta:%d ndi:%d nan disc:%d ml_sta:%d",
			 count_sap, count_sta, count_ndi, count_nan_disc,
			 num_ml_sta);

	if (count_sap == 2 && num_ml_sta == 1) {
		policy_mgr_get_index_for_ml_sta_sap_sap(
				pm_ctx, &index,
				freq_list[ml_sta_idx[0]],
				pm_conc_connection_list[list_sap[0]].freq,
				pm_conc_connection_list[list_sap[1]].freq);
	} else if (count_sap == 2 && count_sta == 1 && !num_ml_sta) {
		policy_mgr_debug(
			"channel: sap0: %d, sap1: %d, sta0: %d",
			pm_conc_connection_list[list_sap[0]].freq,
			pm_conc_connection_list[list_sap[1]].freq,
			pm_conc_connection_list[list_sta[0]].freq);
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq)) {
			index = PM_STA_SAP_SCC_24_SAP_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq)) {
			index = PM_STA_SAP_SCC_24_SAP_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq)) {
			index = PM_STA_SAP_SCC_5_SAP_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[1]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq)) {
			index = PM_STA_SAP_SCC_5_SAP_24_DBS;
		} else {
			index =  PM_MAX_THREE_CONNECTION_MODE;
		}
	} else if (num_ml_sta == 2 && count_sap == 1) {
		sap_freq = pm_conc_connection_list[list_sap[0]].freq;
		policy_mgr_get_index_for_ml_sta_sap(pm_ctx, &index, sap_freq,
						    freq_list, ml_sta_idx);
	} else if (count_sap == 1 && count_sta == 2 && !num_ml_sta) {
		policy_mgr_debug(
			"channel: sap0: %d, sta0: %d, sta1: %d",
			pm_conc_connection_list[list_sap[0]].freq,
			pm_conc_connection_list[list_sta[0]].freq,
			pm_conc_connection_list[list_sta[1]].freq);
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq)) {
			index = PM_STA_SAP_24_STA_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq) &&
		     WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq)) {
			index = PM_STA_SAP_24_STA_5_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq)) {
			index = PM_STA_SAP_5_STA_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[1]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq)) {
			index = PM_STA_SAP_5_STA_24_DBS;
		} else {
			index =  PM_MAX_THREE_CONNECTION_MODE;
		}
	} else if (count_nan_disc == 1 && count_ndi == 1 && count_sap == 1) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_SAP_SCC_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
			   WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_NDI_SCC_24_SAP_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sap[0]].freq) &&
			  WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_SAP_NDI_SCC_5_NAN_DISC_24_DBS;
		} else {
			index = PM_MAX_THREE_CONNECTION_MODE;
		}
	} else if (count_nan_disc == 1 && count_ndi == 1 && count_sta == 1) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_STA_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
			   WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_NDI_24_STA_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
			  WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_STA_NDI_5_NAN_DISC_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_sta[0]].freq) &&
			  WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_STA_NDI_NAN_DISC_24_SMM;
		}
	} else if (count_nan_disc == 1 && count_ndi == 2) {
		/* Policy mgr only considers NAN Disc ch in 2.4GHz */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[1]].freq)) {
			index = PM_NAN_DISC_NDI_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
			   WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NAN_DISC_NDI_24_NDI_5_DBS;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
			  WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NDI_NDI_5_NAN_DISC_24_DBS;
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq) &&
			  WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[list_ndi[0]].freq)) {
			index = PM_NDI_NDI_NAN_DISC_24_SMM;
		}
	} else if (count_sap == 3) {
		if (policy_mgr_is_current_hwmode_sbs(psoc))
			policy_mgr_get_index_for_3_given_freq_sbs(pm_ctx,
								  &index,
								  pm_conc_connection_list[list_sap[0]].freq,
								  pm_conc_connection_list[list_sap[1]].freq,
								  pm_conc_connection_list[list_sap[2]].freq);
		else if (policy_mgr_is_current_hwmode_dbs(psoc))
			policy_mgr_get_index_for_3_given_freq_dbs(pm_ctx,
								  &index,
								  pm_conc_connection_list[list_sap[0]].freq,
								  pm_conc_connection_list[list_sap[1]].freq,
								  pm_conc_connection_list[list_sap[2]].freq);
	}

	policy_mgr_debug(
		"mode0:%d mode1:%d mode2:%d chan0:%d chan1:%d chan2:%d chain:%d index:%d",
		pm_conc_connection_list[0].mode,
		pm_conc_connection_list[1].mode,
		pm_conc_connection_list[2].mode,
		pm_conc_connection_list[0].freq,
		pm_conc_connection_list[1].freq,
		pm_conc_connection_list[2].freq,
		pm_conc_connection_list[0].chain_mask, index);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return index;
}
#endif

uint32_t
policy_mgr_get_nondfs_preferred_channel(struct wlan_objmgr_psoc *psoc,
					enum policy_mgr_con_mode mode,
					bool for_existing_conn,
					uint8_t vdev_id)
{
	uint32_t pcl_channels[NUM_CHANNELS];
	uint8_t pcl_weight[NUM_CHANNELS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	/*
	 * in worst case if we can't find any channel at all
	 * then return 2.4G channel, so atleast we won't fall
	 * under 5G MCC scenario
	 */
	uint32_t i, pcl_len = 0, non_dfs_freq, freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return PM_24_GHZ_CH_FREQ_6;
	}

	freq = PM_24_GHZ_CH_FREQ_6;
	if (true == for_existing_conn) {
		/*
		 * First try to see if there is any non-dfs channel already
		 * present in current connection table. If yes then return
		 * that channel
		 */
		if (true == policy_mgr_is_any_nondfs_chnl_present(
			psoc, &non_dfs_freq))
			return non_dfs_freq;

		if (QDF_STATUS_SUCCESS !=
				policy_mgr_get_pcl_for_existing_conn(
					psoc, mode,
					pcl_channels, &pcl_len,
					pcl_weight, QDF_ARRAY_SIZE(pcl_weight),
					false, vdev_id))
			return freq;
	} else {
		if (QDF_STATUS_SUCCESS != policy_mgr_get_pcl(
		    psoc, mode, pcl_channels, &pcl_len, pcl_weight,
		    QDF_ARRAY_SIZE(pcl_weight), vdev_id))
			return freq;
	}

	for (i = 0; i < pcl_len; i++) {
		if (wlan_reg_is_dfs_for_freq(pm_ctx->pdev, pcl_channels[i]) ||
		    !policy_mgr_is_safe_channel(psoc, pcl_channels[i])) {
			continue;
		} else {
			freq = pcl_channels[i];
			break;
		}
	}

	return freq;
}

static void policy_mgr_remove_dsrc_channels(uint32_t *ch_freq_list,
					    uint32_t *num_channels,
					    struct wlan_objmgr_pdev *pdev)
{
	uint32_t num_chan_temp = 0;
	int i;

	for (i = 0; i < *num_channels; i++) {
		if (!wlan_reg_is_dsrc_freq(ch_freq_list[i])) {
			ch_freq_list[num_chan_temp] = ch_freq_list[i];
			num_chan_temp++;
		}
	}

	*num_channels = num_chan_temp;
}

QDF_STATUS policy_mgr_get_valid_chans_from_range(
		struct wlan_objmgr_psoc *psoc, uint32_t *ch_freq_list,
		uint32_t *ch_cnt, enum policy_mgr_con_mode mode)
{
	uint8_t ch_weight_list[NUM_CHANNELS] = {0};
	uint32_t ch_weight_len;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	size_t chan_index = 0;

	if (!ch_freq_list || !ch_cnt) {
		policy_mgr_err("NULL parameters");
		return QDF_STATUS_E_FAILURE;
	}

	for (chan_index = 0; chan_index < *ch_cnt; chan_index++)
		ch_weight_list[chan_index] = WEIGHT_OF_GROUP1_PCL_CHANNELS;

	ch_weight_len = *ch_cnt;

	/* check the channel avoidance list for beaconing entities */
	if (policy_mgr_is_beaconing_mode(mode))
		policy_mgr_update_with_safe_channel_list(
			psoc, ch_freq_list, ch_cnt, ch_weight_list,
			ch_weight_len);

	status = policy_mgr_mode_specific_modification_on_pcl(
			psoc, ch_freq_list, ch_weight_list, ch_cnt,
			ch_weight_len, mode);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl for mode %d", mode);
		return status;
	}

	status = policy_mgr_modify_pcl_based_on_dnbs(psoc, ch_freq_list,
						     ch_weight_list, ch_cnt);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get modified pcl based on DNBS");
		return status;
	}
	policy_mgr_dump_channel_list(*ch_cnt, ch_freq_list, ch_weight_list);

	return status;
}

QDF_STATUS policy_mgr_get_valid_chans(struct wlan_objmgr_psoc *psoc,
				      uint32_t *ch_freq_list,
				      uint32_t *list_len)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	*list_len = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}
	if (!pm_ctx->valid_ch_freq_list_count) {
		policy_mgr_err("Invalid PM valid channel list");
		return QDF_STATUS_E_INVAL;
	}

	*list_len = pm_ctx->valid_ch_freq_list_count;
	qdf_mem_copy(ch_freq_list, pm_ctx->valid_ch_freq_list,
		     pm_ctx->valid_ch_freq_list_count *
		     sizeof(pm_ctx->valid_ch_freq_list[0]));

	policy_mgr_remove_dsrc_channels(ch_freq_list, list_len, pm_ctx->pdev);

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_list_has_24GHz_channel(uint32_t *ch_freq_list,
				       uint32_t list_len)
{
	uint32_t i;

	for (i = 0; i < list_len; i++) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq_list[i]))
			return true;
	}

	return false;
}

QDF_STATUS
policy_mgr_set_sap_mandatory_channels(struct wlan_objmgr_psoc *psoc,
				      uint32_t *ch_freq_list, uint32_t len)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!len) {
		policy_mgr_err("No mandatory freq/chan configured");
		return QDF_STATUS_E_FAILURE;
	}

	if (!policy_mgr_list_has_24GHz_channel(ch_freq_list, len)) {
		policy_mgr_err("2.4GHz channels missing, this is not expected");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("mandatory chan length:%d",
			pm_ctx->sap_mandatory_channels_len);

	for (i = 0; i < len; i++) {
		pm_ctx->sap_mandatory_channels[i] = ch_freq_list[i];
		policy_mgr_debug("chan:%d", pm_ctx->sap_mandatory_channels[i]);
	}

	pm_ctx->sap_mandatory_channels_len = len;

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_sap_mandatory_channel_set(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (pm_ctx->sap_mandatory_channels_len)
		return true;
	else
		return false;
}

static inline
uint32_t policy_mgr_is_sta_on_indoor_channel(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	uint32_t freq = INVALID_CHANNEL_ID;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return freq;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mode == PM_STA_MODE &&
		    wlan_reg_is_freq_indoor(pm_ctx->pdev,
				pm_conc_connection_list[conn_index].freq) &&
				pm_conc_connection_list[conn_index].in_use) {
			freq = pm_conc_connection_list[conn_index].freq;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return freq;
}

QDF_STATUS policy_mgr_modify_sap_pcl_based_on_mandatory_channel(
		struct wlan_objmgr_psoc *psoc, uint32_t *pcl_list_org,
		uint8_t *weight_list_org, uint32_t *pcl_len_org)
{
	uint32_t i, j, pcl_len = 0;
	bool found;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	qdf_freq_t dfs_sta_freq = INVALID_CHANNEL_ID;
	qdf_freq_t indoor_sta_freq = INVALID_CHANNEL_ID;
	qdf_freq_t sta_5GHz_freq = INVALID_CHANNEL_ID;
	enum hw_mode_bandwidth sta_ch_width;
	uint8_t sta_vdev_id = 0, scc_on_dfs_channel = 0;
	bool sta_sap_scc_on_5ghz_channel;
	bool scc_on_indoor =
		 policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc);
	uint8_t go_count;
	uint32_t go_op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t go_vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t go_op_ch_freq_5g = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!pm_ctx->sap_mandatory_channels_len)
		return QDF_STATUS_SUCCESS;

	if (!policy_mgr_list_has_24GHz_channel(pm_ctx->sap_mandatory_channels,
			pm_ctx->sap_mandatory_channels_len)) {
		policy_mgr_err("fav channel list is missing 2.4GHz channels");
		return QDF_STATUS_E_FAILURE;
	}


	for (i = 0; i < pm_ctx->sap_mandatory_channels_len; i++)
		policy_mgr_debug("fav chan:%d",
				 pm_ctx->sap_mandatory_channels[i]);

	if (scc_on_indoor)
		indoor_sta_freq = policy_mgr_is_sta_on_indoor_channel(psoc);

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc, &scc_on_dfs_channel);
	if (scc_on_dfs_channel)
		policy_mgr_is_sta_present_on_dfs_channel(psoc,
							 &sta_vdev_id,
							 &dfs_sta_freq,
							 &sta_ch_width);
	sta_sap_scc_on_5ghz_channel =
		policy_mgr_is_connected_sta_5g(psoc, &sta_5GHz_freq);

	go_count = policy_mgr_get_mode_specific_conn_info(
				psoc, go_op_ch_freq_list,
				go_vdev_id_list, PM_P2P_GO_MODE);
	if (go_count && !WLAN_REG_IS_24GHZ_CH_FREQ(go_op_ch_freq_list[0])) {
		go_op_ch_freq_5g = go_op_ch_freq_list[0];
		policy_mgr_debug("go 5/6G present, SAP exclude 5/6G channels");
	}

	for (i = 0; i < *pcl_len_org; i++) {
		found = false;
		if (i >= NUM_CHANNELS) {
			policy_mgr_debug("index is exceeding NUM_CHANNELS");
			break;
		}

		if (go_op_ch_freq_5g &&
		    !WLAN_REG_IS_24GHZ_CH_FREQ(pcl_list_org[i]))
			continue;

		if (scc_on_indoor && policy_mgr_is_force_scc(psoc) &&
		    pcl_list_org[i] == indoor_sta_freq) {
			policy_mgr_debug("indoor chan:%d", pcl_list_org[i]);
			found = true;
			goto update_pcl;
		}

		if (scc_on_dfs_channel && policy_mgr_is_force_scc(psoc) &&
		    pcl_list_org[i] == dfs_sta_freq) {
			policy_mgr_debug("dfs chan:%d", pcl_list_org[i]);
			found = true;
			goto update_pcl;
		}

		if (sta_sap_scc_on_5ghz_channel &&
		    policy_mgr_is_force_scc(psoc) &&
		    pcl_list_org[i] == sta_5GHz_freq) {
			policy_mgr_debug("scc chan:%d", pcl_list_org[i]);
			found = true;
			goto update_pcl;
		}

		for (j = 0; j < pm_ctx->sap_mandatory_channels_len; j++) {
			if (pcl_list_org[i] ==
			    pm_ctx->sap_mandatory_channels[j]) {
				found = true;
				break;
			}
		}

update_pcl:
		if (found && (pcl_len < NUM_CHANNELS)) {
			pcl_list_org[pcl_len] = pcl_list_org[i];
			weight_list_org[pcl_len++] = weight_list_org[i];
		}
	}
	*pcl_len_org = pcl_len;

	return QDF_STATUS_SUCCESS;
}

void
policy_mgr_sap_on_non_psc_channel(struct wlan_objmgr_psoc *psoc,
				  qdf_freq_t *intf_ch_freq, uint8_t vdev_id)
{
	struct policy_mgr_pcl_list pcl;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;
	uint32_t i;
	uint32_t ap_pwr_type_6g = 0;

	if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(*intf_ch_freq))
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);

	if (!vdev) {
		policy_mgr_err("vdev %d is not present", vdev_id);
		return;
	}

	ap_pwr_type_6g = wlan_mlme_get_6g_ap_power_type(vdev);
	qdf_mem_zero(&pcl, sizeof(pcl));

	/* PCL list is filtered with Non-PSC channels during
	 * policy_mgr_pcl_modification_for_sap, Reuse same list to check
	 * if STA is in PSC channel for STA + SAP concurrency during SAP restart
	 */
	status = policy_mgr_get_pcl_for_existing_conn(
			psoc, PM_SAP_MODE, pcl.pcl_list, &pcl.pcl_len,
			pcl.weight_list, QDF_ARRAY_SIZE(pcl.weight_list),
			false, vdev_id);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Unable to get PCL for SAP");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		return;
	}

	for (i = 0; i < pcl.pcl_len; i++) {
		if ((WLAN_REG_IS_6GHZ_CHAN_FREQ(pcl.pcl_list[i])) &&
		    pcl.pcl_list[i] == *intf_ch_freq &&
		    ap_pwr_type_6g == REG_VERY_LOW_POWER_AP) {
			policy_mgr_debug("STA is in PSC channel %d in VLP mode, Hence SAP + STA allowed in PSC",
					 *intf_ch_freq);
			*intf_ch_freq = 0;
			wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
			return;
		}
	}

	/* if STA is in Non-PSC Channel + VLP or in non-VLP mode then move
	 * SAP to 2 GHz from PCL list channels
	 */
	*intf_ch_freq = pcl.pcl_list[0];
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
}

QDF_STATUS
policy_mgr_get_sap_mandatory_channel(struct wlan_objmgr_psoc *psoc,
				     uint32_t sap_ch_freq,
				     uint32_t *intf_ch_freq,
				     uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status;
	struct policy_mgr_pcl_list pcl;
	uint32_t i;
	uint32_t sap_new_freq;
	uint8_t mcc_to_scc_switch;
	uint8_t sta_count;
	qdf_freq_t user_config_freq = 0;
	bool sta_sap_scc_on_indoor_channel =
		 policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc);

	mcc_to_scc_switch =
		policy_mgr_get_mcc_to_scc_switch_mode(psoc);

	sta_count = policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE,
							      NULL);

	if (!sta_count || mcc_to_scc_switch !=
			QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL)
		return QDF_STATUS_E_FAILURE;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(&pcl, sizeof(pcl));

	status = policy_mgr_get_pcl_for_existing_conn(
			psoc, PM_SAP_MODE, pcl.pcl_list, &pcl.pcl_len,
			pcl.weight_list, QDF_ARRAY_SIZE(pcl.weight_list),
			false, vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Unable to get PCL for SAP");
		return status;
	}

	/*
	 * Get inside below loop if no existing SAP connection and hence a new
	 * SAP connection might be coming up. pcl.pcl_len can be 0 if no common
	 * channel between PCL & mandatory channel list as well
	 */
	if (!pcl.pcl_len && !policy_mgr_mode_specific_connection_count(psoc,
	    PM_SAP_MODE, NULL)) {
		policy_mgr_debug("policy_mgr_get_pcl_for_existing_conn returned no pcl");
		status = policy_mgr_get_pcl(
				psoc, PM_SAP_MODE,
				pcl.pcl_list, &pcl.pcl_len,
				pcl.weight_list,
				QDF_ARRAY_SIZE(pcl.weight_list), vdev_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Unable to get PCL for SAP: policy_mgr_get_pcl");
			return status;
		}
	}

	status = policy_mgr_modify_sap_pcl_based_on_mandatory_channel(
							psoc, pcl.pcl_list,
							pcl.weight_list,
							&pcl.pcl_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Unable to modify SAP PCL");
		return status;
	}

	if (!pcl.pcl_len) {
		policy_mgr_err("No common channel between mandatory list & PCL");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * If both freq leads to SBS, and sap_ch_freq is a mandatory freq,
	 * allow it as they are not interfering.
	 */
	if (policy_mgr_are_sbs_chan(psoc, sap_ch_freq, *intf_ch_freq)) {
		for (i = 0; i < pcl.pcl_len; i++) {
			if (pcl.pcl_list[i] == sap_ch_freq) {
				policy_mgr_debug("As both freq, %d and %d are SBS, allow sap on mandatory freq %d",
						 sap_ch_freq, *intf_ch_freq,
						 sap_ch_freq);
				*intf_ch_freq = 0;
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	/*
	 * If intf_ch_freq is non-2.4Ghz, First try to get a mandatory freq
	 * which can cause SBS with intf_ch_freq. i.e if STA is in lower 5Ghz,
	 * allow higher 5Ghz mandatory freq.
	 */
	if (!WLAN_REG_IS_24GHZ_CH_FREQ(*intf_ch_freq)) {
		for (i = 0; i < pcl.pcl_len; i++) {
			if (policy_mgr_are_sbs_chan(psoc, pcl.pcl_list[i],
						    *intf_ch_freq)) {
				sap_new_freq = pcl.pcl_list[i];
				goto update_freq;
			}
		}
	}

	sap_new_freq = pcl.pcl_list[0];
	/*
	 * pcl_list carries multiple channel in ML-STA case depending on
	 * the no.of links connected. Check if intf_ch_freq is carrying
	 * any frequency from the list and pick it. If intf_ch_freq is not
	 * present in the list, the frequency present at pcl_list[0] can
	 * be picked as caller doesn't have any preferred/chosen channel
	 * as such.
	 */
	for (i = 0; i < pcl.pcl_len; i++) {
		if (pcl.pcl_list[i] == *intf_ch_freq) {
			sap_new_freq = pcl.pcl_list[i];
			break;
		}
	}

	user_config_freq = policy_mgr_get_user_config_sap_freq(psoc, vdev_id);

	for (i = 0; i < pcl.pcl_len; i++) {
		/* When sta_sap_scc_on_indoor_channel is enabled,
		 * and if pcl contains SCC channel, then STA must
		 * exist on the concurrent session. Therefore, choose
		 * Indoor channel to restart SAP in SCC.
		 */
		if (wlan_reg_is_freq_indoor(pm_ctx->pdev, pcl.pcl_list[i]) &&
		    !WLAN_REG_IS_6GHZ_CHAN_FREQ(pcl.pcl_list[i]) &&
		    sta_sap_scc_on_indoor_channel) {
			sap_new_freq = pcl.pcl_list[i];
			policy_mgr_debug("Choose Indoor channel from PCL list %d sap_new_freq %d",
					 *intf_ch_freq, sap_new_freq);
			goto update_freq;
		}

		if (user_config_freq && (pcl.pcl_list[i] == user_config_freq)) {
			sap_new_freq = pcl.pcl_list[i];
			policy_mgr_debug("Prefer starting SAP on user configured channel:%d",
					 sap_new_freq);
			goto update_freq;
		}
	}

	/* If no SBS Try get SCC freq */
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ch_freq) ||
	    (WLAN_REG_IS_5GHZ_CH_FREQ(sap_ch_freq) &&
	     WLAN_REG_IS_5GHZ_CH_FREQ(*intf_ch_freq))) {
		for (i = 0; i < pcl.pcl_len; i++) {
			if (pcl.pcl_list[i] == *intf_ch_freq) {
				sap_new_freq = pcl.pcl_list[i];
				break;
			}
		}
	}

update_freq:
	*intf_ch_freq = sap_new_freq;
	policy_mgr_debug("Mandatory channel:%d org sap ch %d", *intf_ch_freq,
			 sap_ch_freq);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_valid_chan_weights(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_pcl_chan_weights *weight,
		enum policy_mgr_con_mode mode, struct wlan_objmgr_vdev *vdev)
{
	uint32_t i, j;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_del = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool strict_follow_pcl = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_set(weight->weighed_valid_list, NUM_CHANNELS,
		    WEIGHT_OF_DISALLOWED_CHANNELS);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if ((mode == PM_P2P_GO_MODE || mode == PM_P2P_CLIENT_MODE) ||
	    (mode == PM_STA_MODE &&
	     policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE,
						       NULL))) {
		/*
		 * Store the STA mode's parameter and temporarily delete it
		 * from the concurrency table. This way the allow concurrency
		 * check can be used as though a new connection is coming up,
		 * allowing to detect the disallowed channels.
		 */
		if (mode == PM_STA_MODE) {
			if (policy_mgr_concurrent_sta_on_different_mac(psoc) &&
			    !wlan_cm_same_band_sta_allowed(psoc) &&
			    weight->pcl_len) {
				policy_mgr_debug("sta follow pcl strictly");
				strict_follow_pcl = true;
			}
			if (vdev)
				policy_mgr_store_and_del_conn_info_by_vdev_id(
					psoc, wlan_vdev_get_id(vdev),
					info, &num_del);
			else
				policy_mgr_store_and_del_conn_info(psoc,
								   mode, true,
								   info,
								   &num_del);
		}
		/*
		 * For two port/three port connection strictly follow
		 * PCL weight if coming intf is p2p GO/GC and existing
		 * vdev is SAP/P2PGO/P2PGC.
		 */
		if ((mode == PM_P2P_GO_MODE || mode == PM_P2P_CLIENT_MODE) &&
		    ((policy_mgr_get_beaconing_mode_count(psoc, NULL)) ||
		     policy_mgr_mode_specific_connection_count(
					psoc, PM_P2P_CLIENT_MODE, NULL)))
			strict_follow_pcl = true;

		/*
		 * This is a temporary check and will be removed once ll_lt_sap
		 * CSA support is added.
		 */
		if (wlan_policy_mgr_get_ll_lt_sap_vdev_id(psoc) !=
							WLAN_INVALID_VDEV_ID) {
			policy_mgr_debug("LL_LT_SAP present, strict follow PCL");
			strict_follow_pcl = true;
		}

		/*
		 * There is a small window between releasing the above lock
		 * and acquiring the same in policy_mgr_allow_concurrency,
		 * below!
		 */

		for (i = 0; i < weight->saved_num_chan; i++) {
			/*
			 * If channel is not allowed for concurrency, keep pcl
			 * weight as 0 (WEIGHT_OF_DISALLOWED_CHANNELS)
			 */
			if (!policy_mgr_is_concurrency_allowed
			    (psoc, mode, weight->saved_chan_list[i],
			     HW_MODE_20_MHZ,
			     policy_mgr_get_conc_ext_flags(vdev, false), NULL))
				continue;
			/*
			 * Keep weight 0 (WEIGHT_OF_DISALLOWED_CHANNELS) not
			 * changed for scenarios which require to follow PCL.
			 */
			if (strict_follow_pcl)
				continue;
			weight->weighed_valid_list[i] =
				WEIGHT_OF_NON_PCL_CHANNELS;
		}
		/* Restore the connection info */
		if (mode == PM_STA_MODE)
			policy_mgr_restore_deleted_conn_info(psoc, info,
							     num_del);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	for (i = 0; i < weight->saved_num_chan; i++) {
		for (j = 0; j < weight->pcl_len; j++) {
			if (weight->saved_chan_list[i] == weight->pcl_list[j]) {
				weight->weighed_valid_list[i] =
					weight->weight_list[j];
				break;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_mode_specific_get_channel(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode)
{
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t freq = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}
	/* provides the channel for the first matching mode type */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
		    pm_conc_connection_list[conn_index].in_use) {
			freq = pm_conc_connection_list[conn_index].freq;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return freq;
}

uint32_t policy_mgr_get_connection_count_with_ch_freq(uint32_t ch_freq)
{
	uint32_t i;
	uint32_t count = 0;

	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use &&
		    ch_freq == pm_conc_connection_list[i].freq)
			count++;
	}

	return count;
}

uint32_t policy_mgr_get_alternate_channel_for_sap(
	struct wlan_objmgr_psoc *psoc, uint8_t sap_vdev_id,
	uint32_t sap_ch_freq,
	enum reg_wifi_band pref_band)
{
	uint32_t pcl_channels[NUM_CHANNELS];
	uint8_t pcl_weight[NUM_CHANNELS];
	uint32_t ch_freq = 0;
	uint32_t pcl_len = 0;
	uint32_t first_valid_dfs_5g_freq = 0;
	uint32_t first_valid_non_dfs_5g_freq = 0;
	uint32_t first_valid_6g_freq = 0;
	struct policy_mgr_conc_connection_info info;
	uint8_t num_cxn_del = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t i;
	enum policy_mgr_con_mode con_mode;
	bool is_6ghz_cap;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}
	con_mode = policy_mgr_con_mode_by_vdev_id(psoc, sap_vdev_id);
	is_6ghz_cap = policy_mgr_get_ap_6ghz_capable(psoc, sap_vdev_id, NULL);
	/*
	 * Store the connection's parameter and temporarily delete it
	 * from the concurrency table. This way the get pcl can be used as a
	 * new connection is coming up, after check, restore the connection to
	 * concurrency table.
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_store_and_del_conn_info_by_vdev_id(psoc, sap_vdev_id,
						      &info, &num_cxn_del);
	if (QDF_STATUS_SUCCESS == policy_mgr_get_pcl(
	    psoc, con_mode, pcl_channels, &pcl_len,
	    pcl_weight, QDF_ARRAY_SIZE(pcl_weight), sap_vdev_id)) {
		for (i = 0; i < pcl_len; i++) {
			/*
			 * The API is expected to select the channel on the
			 * other band which is not same as sap's home and
			 * concurrent interference channel (if present),
			 * so skip the sap home channel in PCL.
			 */
			if (pcl_channels[i] == sap_ch_freq)
				continue;
			if (!is_6ghz_cap &&
			    WLAN_REG_IS_6GHZ_CHAN_FREQ(pcl_channels[i]))
				continue;
			if (policy_mgr_get_connection_count(psoc) &&
			    policy_mgr_are_2_freq_on_same_mac(psoc,
							      sap_ch_freq,
							      pcl_channels[i]))
				continue;
			if (policy_mgr_get_connection_count_with_ch_freq(
							pcl_channels[i])) {
				ch_freq = pcl_channels[i];
				break;
			} else if (!ch_freq) {
				ch_freq = pcl_channels[i];
			}
			if (!first_valid_non_dfs_5g_freq &&
			    wlan_reg_is_5ghz_ch_freq(pcl_channels[i])) {
				if (!wlan_reg_is_dfs_in_secondary_list_for_freq(
					pm_ctx->pdev,
					pcl_channels[i])) {
					first_valid_non_dfs_5g_freq = pcl_channels[i];
					if (pref_band == REG_BAND_5G)
						break;
					} else if (!first_valid_dfs_5g_freq) {
						first_valid_dfs_5g_freq = pcl_channels[i];
					}
			}
			if (!first_valid_6g_freq &&
			    wlan_reg_is_6ghz_chan_freq(pcl_channels[i])) {
				first_valid_6g_freq = pcl_channels[i];
				if (pref_band == REG_BAND_6G)
					break;
			}
		}
	}

	/* Restore the connection entry */
	if (num_cxn_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, &info, num_cxn_del);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	if (pref_band == REG_BAND_6G) {
		if (first_valid_6g_freq)
			ch_freq = first_valid_6g_freq;
		else if (first_valid_non_dfs_5g_freq)
			ch_freq = first_valid_non_dfs_5g_freq;
		else if (first_valid_dfs_5g_freq)
			ch_freq = first_valid_dfs_5g_freq;
	} else if (pref_band == REG_BAND_5G) {
		if (first_valid_non_dfs_5g_freq)
			ch_freq = first_valid_non_dfs_5g_freq;
		else if (first_valid_dfs_5g_freq)
			ch_freq = first_valid_dfs_5g_freq;
	}

	return ch_freq;
}

/*
 * Buffer len size to consider the 4 char freq, 3 char weight, 2 char
 * for open close brackets and space and a space, Total 10
 */
#define CHAN_WEIGHT_CHAR_LEN 10
#define MAX_CHAN_TO_PRINT 39

bool policy_mgr_dump_channel_list(uint32_t len, uint32_t *pcl_channels,
				  uint8_t *pcl_weight)
{
	uint32_t idx, buff_len, num = 0, count = 0;
	char *chan_buff = NULL;

	buff_len = (QDF_MIN(len, MAX_CHAN_TO_PRINT) * CHAN_WEIGHT_CHAR_LEN) + 1;
	chan_buff = qdf_mem_malloc(buff_len);
	if (!chan_buff)
		return false;

	policymgr_nofl_debug("Total PCL Chan Freq %d", len);
	for (idx = 0; (idx < len) && (idx < NUM_CHANNELS); idx++) {
		num += qdf_scnprintf(chan_buff + num, buff_len - num,
				     " %d[%d]", pcl_channels[idx],
				     pcl_weight[idx]);
		count++;
		if (count >= MAX_CHAN_TO_PRINT) {
			/* Print the MAX_CHAN_TO_PRINT channels */
			policymgr_nofl_debug("Freq[weight]:%s",
					     chan_buff);
			count = 0;
			num = 0;
		}
	}
	/* Print any pending channels */
	if (num)
		policymgr_nofl_debug("Freq[weight]:%s", chan_buff);

	qdf_mem_free(chan_buff);

	return true;
}

QDF_STATUS policy_mgr_filter_passive_ch(struct wlan_objmgr_pdev *pdev,
					uint32_t *ch_freq_list,
					uint32_t *ch_cnt)
{
	size_t ch_index;
	size_t target_ch_cnt = 0;

	if (!pdev || !ch_freq_list || !ch_cnt) {
		policy_mgr_err("NULL parameters");
		return QDF_STATUS_E_FAULT;
	}

	for (ch_index = 0; ch_index < *ch_cnt; ch_index++) {
		if (wlan_reg_is_passive_for_freq(pdev,
						 ch_freq_list[ch_index])) {
			policy_mgr_debug("Remove freq: %d from list as it's passive",
					 ch_freq_list[ch_index]);
			continue;
		}
		ch_freq_list[target_ch_cnt++] = ch_freq_list[ch_index];
	}

	*ch_cnt = target_ch_cnt;

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_3rd_conn_on_same_band_allowed(struct wlan_objmgr_psoc *psoc,
						 enum policy_mgr_con_mode mode,
						 qdf_freq_t ch_freq)
{
	enum policy_mgr_pcl_type pcl = PM_NONE;
	enum policy_mgr_conc_priority_mode conc_system_pref = 0;
	enum policy_mgr_two_connection_mode third_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
			return false;
	}

	if (pm_conc_connection_list[0].freq != ch_freq ||
	    pm_conc_connection_list[0].freq !=
				pm_conc_connection_list[1].freq) {
		policy_mgr_debug("No MCC support in 3vif in same mac: %d %d %d",
				 pm_conc_connection_list[0].freq,
				 pm_conc_connection_list[1].freq,
				 ch_freq);
		return false;
	}

	policy_mgr_debug("pref:%d requested mode:%d",
			 pm_ctx->cur_conc_system_pref, mode);

	switch (pm_ctx->cur_conc_system_pref) {
	case 0:
		conc_system_pref = PM_THROUGHPUT;
		break;
	case 1:
		conc_system_pref = PM_POWERSAVE;
		break;
	case 2:
		conc_system_pref = PM_LATENCY;
		break;
	default:
		policy_mgr_err("unknown cur_conc_system_pref value %d",
			       pm_ctx->cur_conc_system_pref);
		break;
	}

	third_index = policy_mgr_get_third_connection_pcl_table_index(psoc);
	if (PM_MAX_TWO_CONNECTION_MODE == third_index) {
		policy_mgr_err(
			"couldn't find index for 3rd connection pcl table");
			return false;
	}
	if (policy_mgr_is_hw_dbs_capable(psoc) == true) {
		pcl = (*third_connection_pcl_dbs_table)
			[third_index][mode][conc_system_pref];
	} else {
		pcl = (*third_connection_pcl_non_dbs_table)
			[third_index][mode][conc_system_pref];
	}

	policy_mgr_debug("pcl for third connection mode %s is %d %s",
			 device_mode_to_string(mode), pcl,
			 pcl_type_to_string(pcl));
	switch (pcl) {
	case PM_SCC_CH:
	case PM_SCC_CH_24G:
	case PM_SCC_CH_5G:
	case PM_24G_SCC_CH:
	case PM_5G_SCC_CH:
	case PM_SCC_ON_5_CH_5G:
	case PM_SCC_ON_5_SCC_ON_24_24G:
	case PM_SCC_ON_5_SCC_ON_24_5G:
	case PM_SCC_ON_5_5G_24G:
	case PM_SCC_ON_5_5G_SCC_ON_24G:
	case PM_SCC_ON_24_SCC_ON_5_24G:
	case PM_SCC_ON_24_SCC_ON_5_5G:
	case PM_SCC_ON_24_CH_24G:
	case PM_SCC_ON_5_SCC_ON_24:
	case PM_SCC_ON_24_SCC_ON_5:
	case PM_24G_SCC_CH_SBS_CH:
	case PM_24G_SCC_CH_SBS_CH_5G:
	case PM_SBS_CH_24G_SCC_CH:
	case PM_SBS_CH_SCC_CH_24G:
	case PM_SCC_CH_SBS_CH_24G:
	case PM_SBS_CH_SCC_CH_5G_24G:
	case PM_SCC_CH_MCC_CH_SBS_CH_24G:
	case PM_MCC_CH:
	case PM_MCC_CH_24G:
	case PM_MCC_CH_5G:
	case PM_24G_MCC_CH:
	case PM_5G_MCC_CH:
	case PM_24G_SBS_CH_MCC_CH:
		ret = true;
		break;
	default:
		policy_mgr_debug("Not in SCC case");
		ret = false;
		break;
	}
	return ret;
}

bool policy_mgr_is_sta_chan_valid_for_connect_and_roam(
				struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq)
{
	struct wlan_objmgr_psoc *psoc;
	uint32_t sap_count;
	bool skip_6g_and_indoor_freq;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return true;

	skip_6g_and_indoor_freq =
		wlan_scan_cfg_skip_6g_and_indoor_freq(psoc);
	sap_count =
		policy_mgr_get_sap_mode_count(psoc, NULL);
	/*
	 * Do not allow STA to connect/roam on 6Ghz or indoor channel for
	 * non-dbs hardware if SAP is present and skip_6g_and_indoor_freq_scan
	 * ini is enabled
	 */
	if (skip_6g_and_indoor_freq && sap_count &&
	    !policy_mgr_is_hw_dbs_capable(psoc) &&
	    (WLAN_REG_IS_6GHZ_CHAN_FREQ(freq) ||
	     wlan_reg_is_freq_indoor(pdev, freq)))
		return false;

	return true;
}

/**
 * _policy_mgr_is_vdev_ll_sap() - Check whether any LL SAP is present or not
 * for provided ap policy
 * @psoc: psoc object
 * @vdev_id: vdev id
 * @ap_type: LL SAP type
 *
 * Return: true if it's present otherwise false
 */
static bool
_policy_mgr_is_vdev_ll_sap(struct wlan_objmgr_psoc *psoc,
			   uint32_t vdev_id, enum ll_ap_type ap_type)
{
	struct wlan_objmgr_vdev *vdev;
	bool is_ll_sap = false;
	enum QDF_OPMODE mode;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum host_concurrent_ap_policy profile =
					HOST_CONCURRENT_AP_POLICY_UNSPECIFIED;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm_ctx");
		return is_ll_sap;
	}

	mode = wlan_get_opmode_from_vdev_id(pm_ctx->pdev, vdev_id);

	if (mode != QDF_SAP_MODE)
		return is_ll_sap;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d: invalid vdev", vdev_id);
		return is_ll_sap;
	}

	profile = wlan_mlme_get_ap_policy(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	switch (ap_type) {
	case LL_AP_TYPE_HT:
		if (profile == HOST_CONCURRENT_AP_POLICY_XR)
			is_ll_sap = true;
	break;
	case LL_AP_TYPE_LT:
		if (profile == HOST_CONCURRENT_AP_POLICY_GAMING_AUDIO ||
		    profile ==
		    HOST_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING)
			is_ll_sap = true;
	break;
	case LL_AP_TYPE_ANY:
		if (profile == HOST_CONCURRENT_AP_POLICY_GAMING_AUDIO ||
		    profile ==
		    HOST_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING ||
		    profile == HOST_CONCURRENT_AP_POLICY_XR)
			is_ll_sap = true;
	break;
	default:
		policy_mgr_err("invalid ap type %d", ap_type);
	}
	return is_ll_sap;
}

bool
policy_mgr_is_vdev_ll_sap(struct wlan_objmgr_psoc *psoc,
			  uint32_t vdev_id)
{
	return _policy_mgr_is_vdev_ll_sap(psoc, vdev_id, LL_AP_TYPE_ANY);
}

bool
policy_mgr_is_vdev_ll_ht_sap(struct wlan_objmgr_psoc *psoc,
			     uint32_t vdev_id)
{
	return _policy_mgr_is_vdev_ll_sap(psoc, vdev_id, LL_AP_TYPE_HT);
}

bool
policy_mgr_is_vdev_ll_lt_sap(struct wlan_objmgr_psoc *psoc,
			     uint32_t vdev_id)
{
	return _policy_mgr_is_vdev_ll_sap(psoc, vdev_id, LL_AP_TYPE_LT);
}

#ifndef WLAN_FEATURE_LL_LT_SAP
QDF_STATUS
policy_mgr_get_pcl_chlist_for_ll_sap(struct wlan_objmgr_psoc *psoc,
				     uint32_t *len, uint32_t *pcl_channels,
				     uint8_t *pcl_weight)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t pcl_len = 0, i, conn_idx = 0;
	uint32_t pcl_list[NUM_CHANNELS], total_connection = 0;
	uint8_t weight_list[NUM_CHANNELS];
	qdf_freq_t freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is null");
		return QDF_STATUS_E_FAILURE;
	}

	total_connection = policy_mgr_get_connection_count(psoc);
	if (!total_connection) {
		for (i = 0; i < *len; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(pcl_channels[i]))
				continue;

			pcl_list[pcl_len] = pcl_channels[i];
			weight_list[pcl_len++] = pcl_weight[i];
		}
		qdf_mem_zero(pcl_channels, *len * sizeof(*pcl_channels));
		qdf_mem_copy(pcl_channels, pcl_list,
			     pcl_len * sizeof(*pcl_channels));
	} else {
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
		for (conn_idx = 0; conn_idx < MAX_NUMBER_OF_CONC_CONNECTIONS;
		     conn_idx++) {
			if (!pm_conc_connection_list[conn_idx].in_use)
				continue;

			freq = pm_conc_connection_list[conn_idx].freq;

			for (i = 0; i < *len; i++) {
				if (policy_mgr_2_freq_always_on_same_mac(
								psoc,
								pcl_channels[i],
								freq) ||
				    WLAN_REG_IS_24GHZ_CH_FREQ(pcl_channels[i]))
					continue;
				pcl_list[pcl_len] = pcl_channels[i];
				weight_list[pcl_len++] = pcl_weight[i];
			}
			*len = pcl_len;
			qdf_mem_zero(pcl_channels,
				     *len * sizeof(*pcl_channels));
			qdf_mem_copy(pcl_channels, pcl_list,
				     pcl_len * sizeof(*pcl_channels));
		}
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	}

	qdf_mem_zero(pcl_weight, *len * sizeof(*pcl_weight));
	qdf_mem_copy(pcl_weight, weight_list, pcl_len);
	*len = pcl_len;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_pcl_ch_for_sap_go_with_ll_sap_present(
					struct wlan_objmgr_psoc *psoc,
					uint32_t *len, uint32_t *pcl_channels,
					uint8_t *pcl_weight)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t pcl_len = 0, i, conn_idx = 0;
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	qdf_freq_t freq;
	uint32_t vdev_id;
	bool is_ll_sap = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is null");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_idx = 0; conn_idx < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_idx++) {
		if (!pm_conc_connection_list[conn_idx].in_use)
			continue;

		freq = pm_conc_connection_list[conn_idx].freq;
		vdev_id = pm_conc_connection_list[conn_idx].vdev_id;
		if (!policy_mgr_is_vdev_ll_sap(psoc, vdev_id))
			continue;

		is_ll_sap = 1;
		for (i = 0; i < *len; i++) {
			if (policy_mgr_2_freq_always_on_same_mac(
								psoc,
								pcl_channels[i],
								freq))
				continue;
			pcl_list[pcl_len] = pcl_channels[i];
			weight_list[pcl_len++] = pcl_weight[i];
		}
		*len = pcl_len;
		qdf_mem_zero(pcl_channels,
			     *len * sizeof(*pcl_channels));
		qdf_mem_copy(pcl_channels, pcl_list,
			     pcl_len * sizeof(*pcl_channels));
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (!is_ll_sap)
		return QDF_STATUS_SUCCESS;

	qdf_mem_zero(pcl_weight, *len * sizeof(*pcl_weight));
	qdf_mem_copy(pcl_weight, weight_list, pcl_len);
	*len = pcl_len;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_pcl_channel_for_ll_sap_concurrency(
					struct wlan_objmgr_psoc *psoc,
					uint32_t vdev_id,
					uint32_t *pcl_channels,
					uint8_t *pcl_weight, uint32_t *len)
{
	uint32_t orig_len = *len;

	if (policy_mgr_is_vdev_ll_sap(psoc, vdev_id)) {
		/* Scenario: If there is some existing interface present and
		 * LL SAP is coming up.
		 * Filter pcl channel for LL SAP
		 */
		policy_mgr_get_pcl_chlist_for_ll_sap(psoc, len, pcl_channels,
						     pcl_weight);
	} else {
		/* Scenario: If there is LL SAP and GO/SAP is coming up.
		 * Filter pcl channel for GO/SAP
		 */
		policy_mgr_get_pcl_ch_for_sap_go_with_ll_sap_present(
								psoc,
								len,
								pcl_channels,
								pcl_weight);
	}

	if (orig_len != *len) {
		policy_mgr_debug("PCL after ll sap modification");
		policy_mgr_dump_channel_list(*len, pcl_channels, pcl_weight);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_LL_LT_SAP
QDF_STATUS policy_mgr_get_pcl_ch_list_for_ll_sap(
					struct wlan_objmgr_psoc *psoc,
					struct policy_mgr_pcl_list *pcl,
					uint8_t vdev_id,
					struct connection_info *info,
					uint8_t *connection_count)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t num_cxn_del = 0;
	struct policy_mgr_conc_connection_info pm_info = {0};
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return QDF_STATUS_E_FAILURE;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	/*
	 * Scenario: Standalone XPAN is present and CSA happens on
	 * LL_LT_SAP interface.
	 * During CSA, it will check the PCL list to get the new freq.
	 * Since there is already LL_LT_SAP interface entry in PCL index.
	 * It will lead to LL_LT_SAP + LL_LT_SAP concurrencies. To avoid
	 * that, delete the existing connection entry from PCL index,
	 * get the PCL list and restore it back.
	 */
	policy_mgr_store_and_del_conn_info_by_vdev_id(psoc, vdev_id,
						      &pm_info, &num_cxn_del);

	status = policy_mgr_get_pcl(psoc, PM_LL_LT_SAP_MODE, pcl->pcl_list,
				    &pcl->pcl_len, pcl->weight_list,
				    QDF_ARRAY_SIZE(pcl->weight_list),
				    vdev_id);

	/*
	 * Get existing connection info before updating LL_LT_SAP freq list
	 * This will help to avoid updation of SCC channel in LL_LT_SAP
	 * freq list.
	 */
	*connection_count = policy_mgr_get_connection_info(psoc, info);

	/* Restore the connection entry */
	if (num_cxn_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, &pm_info,
						     num_cxn_del);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}
#endif
