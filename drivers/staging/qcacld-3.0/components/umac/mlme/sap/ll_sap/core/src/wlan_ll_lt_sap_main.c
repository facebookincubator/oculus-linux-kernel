/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wlan_ll_lt_sap_main.h"
#include "wlan_reg_services_api.h"
#include "wlan_ll_sap_public_structs.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "wlan_ll_sap_main.h"
#include "wlan_ll_lt_sap_bearer_switch.h"
#include "wlan_scan_api.h"
#include "target_if.h"

bool ll_lt_sap_is_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		ll_sap_err("Invalid WMI handle");
		return false;
	}

	return wmi_service_enabled(wmi_handle, wmi_service_xpan_support);
}

/**
 * ll_lt_sap_get_sorted_user_config_acs_ch_list() - API to get sorted user
 * configured ACS channel list
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev Id
 * @ch_info: Pointer to ch_info
 *
 * Return: None
 */
static
QDF_STATUS ll_lt_sap_get_sorted_user_config_acs_ch_list(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					struct sap_sel_ch_info *ch_info)
{
	struct scan_filter *filter;
	qdf_list_t *list = NULL;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LL_SAP_ID);

	if (!vdev) {
		ll_sap_err("Invalid vdev for vdev_id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		goto rel_ref;

	wlan_sap_get_user_config_acs_ch_list(vdev_id, filter);

	list = wlan_scan_get_result(pdev, filter);

	qdf_mem_free(filter);

	if (!list)
		goto rel_ref;

	status = wlan_ll_sap_sort_channel_list(vdev_id, list, ch_info);

	wlan_scan_purge_results(list);

rel_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LL_SAP_ID);

	return status;
}

QDF_STATUS ll_lt_sap_init(struct wlan_objmgr_vdev *vdev)
{
	struct ll_sap_vdev_priv_obj *ll_sap_obj;
	QDF_STATUS status;
	uint8_t i, j;
	struct bearer_switch_info *bs_ctx;

	ll_sap_obj = ll_sap_get_vdev_priv_obj(vdev);

	if (!ll_sap_obj) {
		ll_sap_err("vdev %d ll_sap obj null",
			   wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	bs_ctx = qdf_mem_malloc(sizeof(struct bearer_switch_info));
	if (!bs_ctx)
		return QDF_STATUS_E_NOMEM;

	ll_sap_obj->bearer_switch_ctx = bs_ctx;

	qdf_atomic_init(&bs_ctx->request_id);

	for (i = 0; i < WLAN_UMAC_PSOC_MAX_VDEVS; i++)
		for (j = 0; j < BEARER_SWITCH_REQ_MAX; j++)
			qdf_atomic_init(&bs_ctx->ref_count[i][j]);

	qdf_atomic_init(&bs_ctx->fw_ref_count);
	qdf_atomic_init(&bs_ctx->total_ref_count);

	bs_ctx->vdev = vdev;

	status = bs_sm_create(bs_ctx);

	if (QDF_IS_STATUS_ERROR(status))
		goto bs_sm_failed;

	ll_sap_debug("vdev %d", wlan_vdev_get_id(vdev));

	return QDF_STATUS_SUCCESS;

bs_sm_failed:
	qdf_mem_free(ll_sap_obj->bearer_switch_ctx);
	ll_sap_obj->bearer_switch_ctx = NULL;
	return status;
}

QDF_STATUS ll_lt_sap_deinit(struct wlan_objmgr_vdev *vdev)
{
	struct ll_sap_vdev_priv_obj *ll_sap_obj;

	ll_sap_obj = ll_sap_get_vdev_priv_obj(vdev);

	if (!ll_sap_obj) {
		ll_sap_err("vdev %d ll_sap obj null",
			   wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	if (!ll_sap_obj->bearer_switch_ctx) {
		ll_sap_debug("vdev %d Bearer switch context is NULL",
			     wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	bs_sm_destroy(ll_sap_obj->bearer_switch_ctx);
	qdf_mem_free(ll_sap_obj->bearer_switch_ctx);
	ll_sap_obj->bearer_switch_ctx = NULL;

	ll_sap_debug("vdev %d", wlan_vdev_get_id(vdev));

	return QDF_STATUS_SUCCESS;
}

static
void ll_lt_sap_update_mac_freq(struct wlan_ll_lt_sap_mac_freq *freq_list,
			       qdf_freq_t sbs_cut_off_freq,
			       qdf_freq_t given_freq, uint32_t given_weight)
{
	if (WLAN_REG_IS_5GHZ_CH_FREQ(given_freq) &&
	    ((sbs_cut_off_freq && given_freq < sbs_cut_off_freq) ||
	    !sbs_cut_off_freq) && !freq_list->freq_5GHz_low) {
		freq_list->freq_5GHz_low = given_freq;
		freq_list->weight_5GHz_low = given_weight;

		/*
		 * Update same freq in 5GHz high freq if sbs_cut_off_freq
		 * is not present
		 */
		if (!sbs_cut_off_freq) {
			freq_list->freq_5GHz_high = given_freq;
			freq_list->weight_5GHz_high = given_weight;
		}
	} else if (WLAN_REG_IS_5GHZ_CH_FREQ(given_freq) &&
		   ((sbs_cut_off_freq && given_freq > sbs_cut_off_freq) ||
		   !sbs_cut_off_freq) && !freq_list->freq_5GHz_high) {
			freq_list->freq_5GHz_high = given_freq;
			freq_list->weight_5GHz_high = given_weight;

		/*
		 * Update same freq for 5GHz low freq if sbs_cut_off_freq
		 * is not present
		 */
		if (!sbs_cut_off_freq) {
			freq_list->freq_5GHz_low = given_freq;
			freq_list->weight_5GHz_low = given_weight;
		}
	} else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(given_freq) &&
		   !freq_list->freq_6GHz) {
		freq_list->freq_6GHz = given_freq;
		freq_list->weight_6GHz = given_weight;
	}
}

/* Threshold value of channel weight */
#define CHANNEL_WEIGHT_THRESHOLD_VALUE 20000
static
void ll_lt_sap_update_freq_list(struct wlan_objmgr_psoc *psoc,
				struct sap_sel_ch_info *ch_param,
				struct wlan_ll_lt_sap_freq_list *freq_list,
				struct policy_mgr_pcl_list *pcl,
				struct connection_info *info,
				uint8_t connection_count,
				uint8_t vdev_id)
{
	qdf_freq_t freq, sbs_cut_off_freq;
	qdf_freq_t same_mac_freq, standalone_mac_freq;
	uint8_t i = 0, count;
	uint32_t weight;
	enum policy_mgr_con_mode con_mode = PM_MAX_NUM_OF_MODE;

	sbs_cut_off_freq = policy_mgr_get_sbs_cut_off_freq(psoc);

	for (i = 0; i < ch_param->num_ch; i++) {
		if (!ch_param->ch_info[i].valid)
			continue;

		freq = ch_param->ch_info[i].chan_freq;
		weight = ch_param->ch_info[i].weight;

		/*
		 * Do not select same channel where LL_LT_SAP was
		 * present earlier
		 */
		if (freq_list->prev_freq == freq)
			continue;

		/* Check if channel is present in PCL or not */
		if (!wlan_ll_sap_freq_present_in_pcl(pcl, freq))
			continue;

		/*
		 * Store first valid best channel from ACS final list
		 * This will be used if there is no valid freq present
		 * within threshold value or no concurrency present
		 */
		if (!freq_list->best_freq) {
			freq_list->best_freq = freq;
			freq_list->weight_best_freq = weight;
		}

		if (!connection_count)
			break;

		/*
		 * Instead of selecting random channel, select those
		 * channel whose weight is less than
		 * CHANNEL_WEIGHT_THRESHOLD_VALUE value. This will help
		 * to get the best channel compartively and avoid multiple
		 * times of channel switch.
		 */
		if (weight > CHANNEL_WEIGHT_THRESHOLD_VALUE)
			continue;

		same_mac_freq = 0;
		standalone_mac_freq = 0;

		/*
		 * Loop through all existing connections before updating
		 * channels for LL_LT_SAP.
		 */
		for (count = 0; count < connection_count; count++) {
			/* Do not select SCC channel to update freq_list */
			if (freq == info[count].ch_freq) {
				same_mac_freq = 0;
				standalone_mac_freq = 0;
				break;
			}
			/*
			 * Check whether ch_param frequency is in same mac
			 * or not.
			 */
			if (policy_mgr_2_freq_always_on_same_mac(
							psoc, freq,
							info[count].ch_freq)) {
				con_mode = policy_mgr_get_mode_by_vdev_id(
						psoc, info[count].vdev_id);
				/*
				 * Check whether SAP is present in same mac or
				 * not. If yes then do not select that frequency
				 * because two beacon entity can't be in same
				 * mac.
				 * Also, do not fill same_mac_freq if it's
				 * already filled i.e two existing connection
				 * are present on same mac.
				 */
				if (con_mode == PM_SAP_MODE || same_mac_freq) {
					same_mac_freq = 0;
					standalone_mac_freq = 0;
					break;
				}
				same_mac_freq = freq;
			} else if (!standalone_mac_freq) {
				/*
				 * Fill standalone_mac_freq only if it's not
				 * filled
				 */
				standalone_mac_freq = freq;
			}
		}

		/*
		 * Scenario: Let say two concurrent connection(other than
		 * LL_LT_SAP) are present in both mac and one channel from
		 * ch_param structure needs to select to fill in freq_list for
		 * LL_LT_SAP.
		 * Since, there is an existing connection present in both mac
		 * then there is chance that channel from ch_param structure
		 * may get select for both same_mac_freq and standalone_mac_freq
		 *
		 * But ideally standalone_mac_freq is not free, some existing
		 * connection is already present in it.
		 * So, instead of giving priority to fill standalone_mac_freq
		 * first, fill same_mac_freq first.
		 *
		 * Example: Let say 2 concurrent interface present in channel
		 * 36 and 149. Now channel 48 from ch_param structure needs to
		 * be validate and fill based on existing interface.
		 * With respect to channel 36, it will fit to same_mac_freq
		 * and with respect to channel 149, it will fit to
		 * standalone_mac_freq. But in standalone_mac_freq, there is
		 * already existing interface present. So, give priority to
		 * same_mac_freq to fill the freq list.
		 */
		if (same_mac_freq)
			ll_lt_sap_update_mac_freq(&freq_list->shared_mac,
						  sbs_cut_off_freq,
						  same_mac_freq,
						  weight);
		else if (standalone_mac_freq)
			ll_lt_sap_update_mac_freq(&freq_list->standalone_mac,
						  sbs_cut_off_freq,
						  standalone_mac_freq,
						  weight);

		if (freq_list->shared_mac.freq_5GHz_low &&
		    freq_list->shared_mac.freq_5GHz_high &&
		    freq_list->shared_mac.freq_6GHz &&
		    freq_list->standalone_mac.freq_5GHz_low &&
		    freq_list->standalone_mac.freq_5GHz_high &&
		    freq_list->standalone_mac.freq_6GHz)
			break;
	}

	ll_sap_debug("vdev %d, best %d[%d] Shared: low %d[%d] high %d[%d] 6Ghz %d[%d], Standalone: low %d[%d] high %d[%d] 6Ghz %d[%d], prev %d, existing connection cnt %d",
		     vdev_id, freq_list->best_freq,
		     freq_list->weight_best_freq,
		     freq_list->shared_mac.freq_5GHz_low,
		     freq_list->shared_mac.weight_5GHz_low,
		     freq_list->shared_mac.freq_5GHz_high,
		     freq_list->shared_mac.weight_5GHz_high,
		     freq_list->shared_mac.freq_6GHz,
		     freq_list->shared_mac.weight_6GHz,
		     freq_list->standalone_mac.freq_5GHz_low,
		     freq_list->standalone_mac.weight_5GHz_low,
		     freq_list->standalone_mac.freq_5GHz_high,
		     freq_list->standalone_mac.weight_5GHz_high,
		     freq_list->standalone_mac.freq_6GHz,
		     freq_list->standalone_mac.weight_6GHz,
		     freq_list->prev_freq, connection_count);
}

static
QDF_STATUS ll_lt_sap_get_allowed_freq_list(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				struct sap_sel_ch_info *ch_param,
				struct wlan_ll_lt_sap_freq_list *freq_list)
{
	struct connection_info conn_info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t count;
	struct policy_mgr_pcl_list *pcl;
	QDF_STATUS status;

	pcl = qdf_mem_malloc(sizeof(*pcl));
	if (!pcl)
		return QDF_STATUS_E_FAILURE;

	status = policy_mgr_get_pcl_ch_list_for_ll_sap(psoc, pcl, vdev_id,
						       conn_info, &count);
	if (QDF_IS_STATUS_ERROR(status))
		goto end;

	ll_lt_sap_update_freq_list(psoc, ch_param, freq_list, pcl,
				   conn_info, count, vdev_id);

	status = QDF_STATUS_SUCCESS;
end:
	qdf_mem_free(pcl);
	return status;
}

QDF_STATUS ll_lt_sap_get_freq_list(struct wlan_objmgr_psoc *psoc,
				   struct wlan_ll_lt_sap_freq_list *freq_list,
				   uint8_t vdev_id)
{
	struct sap_sel_ch_info ch_param = { NULL, 0 };
	QDF_STATUS status;

	/*
	 * This memory will be allocated in sap_chan_sel_init() as part
	 * of sap_sort_channel_list(). But the caller has to free up the
	 * allocated memory
	 */

	status = ll_lt_sap_get_sorted_user_config_acs_ch_list(psoc, vdev_id,
							      &ch_param);

	if (!ch_param.num_ch || QDF_IS_STATUS_ERROR(status))
		goto release_mem;

	status = ll_lt_sap_get_allowed_freq_list(psoc, vdev_id, &ch_param,
						 freq_list);

release_mem:
	wlan_ll_sap_free_chan_info(&ch_param);
	return status;
}

qdf_freq_t ll_lt_sap_get_valid_freq(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id)
{
	struct wlan_ll_lt_sap_freq_list freq_list;

	qdf_mem_zero(&freq_list, sizeof(freq_list));

	ll_lt_sap_get_freq_list(psoc, &freq_list, vdev_id);

	if (freq_list.standalone_mac.freq_5GHz_low)
		return freq_list.standalone_mac.freq_5GHz_low;
	if (freq_list.shared_mac.freq_5GHz_low)
		return freq_list.shared_mac.freq_5GHz_low;
	if (freq_list.standalone_mac.freq_6GHz)
		return freq_list.standalone_mac.freq_6GHz;
	if (freq_list.standalone_mac.freq_5GHz_high)
		return freq_list.standalone_mac.freq_5GHz_high;
	if (freq_list.shared_mac.freq_6GHz)
		return freq_list.shared_mac.freq_6GHz;
	if (freq_list.shared_mac.freq_5GHz_high)
		return freq_list.shared_mac.freq_5GHz_high;

	return freq_list.best_freq;
}
