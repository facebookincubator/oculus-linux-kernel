/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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

#include <wlan_cfr_ucfg_api.h>
#include "cfr_defs_i.h"
#include <wlan_cfr_utils_api.h>
#include <wlan_cfr_tgt_api.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <qdf_module.h>
#ifdef WLAN_ENH_CFR_ENABLE
#include "cdp_txrx_ctrl.h"
#endif

int ucfg_cfr_start_capture(struct wlan_objmgr_pdev *pdev,
			   struct wlan_objmgr_peer *peer,
			   struct cfr_capture_params *params)
{
	int status;
	struct pdev_cfr *pa;
	struct peer_cfr *pe;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_CFR);
	if (NULL == pa) {
		cfr_err("PDEV cfr object is NULL!\n");
		return -EINVAL;
	}

	if (!(pa->is_cfr_capable)) {
		qdf_info("cfr is not supported on this chip\n");
		return -EINVAL;
	}

	/* Get peer private object */
	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_CFR);
	if (NULL == pe) {
		cfr_err("PEER cfr object is NULL!\n");
		return -EINVAL;
	}

	if ((params->period < 0) || (params->period > MAX_CFR_PRD) ||
		(params->period % 10)) {
		cfr_err("Invalid period value: %d\n", params->period);
		return -EINVAL;
	}

	if (!(params->period) && (pa->cfr_timer_enable)) {
		cfr_err("Single shot capture is not allowed during periodic capture\n");
		return -EINVAL;
	}

	if ((params->period) && !(pa->cfr_timer_enable)) {
		cfr_err("Global periodic timer is not enabled, configure global cfr timer\n");
	}

	if (params->period) {
		if (pa->cfr_current_sta_count == pa->cfr_max_sta_count) {
			qdf_info("max periodic cfr clients reached\n");
			return -EINVAL;
		}
		if (!(pe->request))
			pa->cfr_current_sta_count++;
	}

	status = tgt_cfr_start_capture(pdev, peer, params);

	if (status == 0) {
		pe->bandwidth = params->bandwidth;
		pe->period = params->period;
		pe->capture_method = params->method;
		pe->request = PEER_CFR_CAPTURE_ENABLE;
	} else
		pa->cfr_current_sta_count--;

	return status;
}

int ucfg_cfr_start_capture_probe_req(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr *unassoc_mac,
				     struct cfr_capture_params *params)
{
	int idx, idx_to_insert = -1;
	struct pdev_cfr *pa;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_CFR);
	if (!pa) {
		cfr_err("Pdev cfr object is null!");
		return -EINVAL;
	}

	if (!(pa->is_cfr_capable)) {
		cfr_err("CFR is not supported on this chip");
		return -EINVAL;
	}

	if (pa->cfr_current_sta_count == pa->cfr_max_sta_count) {
		cfr_err("max cfr cleint reached");
		return -EINVAL;
	}

	for (idx = 0; idx < MAX_CFR_ENABLED_CLIENTS; idx++) {
		/* Store first invalid entry's index, to add mac entry if not
		 * already present.
		 */
		if (idx_to_insert < 0) {
			if (pa->unassoc_pool[idx].is_valid != true)
				idx_to_insert = idx;
		}

		/* Add new mac entry only if it is not present. If already
		 * present, update the capture parameters
		 */
		if (qdf_mem_cmp(&pa->unassoc_pool[idx].mac, unassoc_mac,
				sizeof(struct qdf_mac_addr)) == 0) {
			cfr_info("Node already present. Updating params");
			qdf_mem_copy(&pa->unassoc_pool[idx].cfr_params,
				     params,
				     sizeof(struct cfr_capture_params));
			pa->unassoc_pool[idx].is_valid = true;
			return 0;
		}
	}

	if (idx_to_insert < 0) {
		/* All the entries in the table are valid. So we have reached
		 * max client capacity. To add a new client, capture on one of
		 * the clients in table has to be stopped.
		 */
		cfr_err("Maximum client capacity reached");
		return -EINVAL;
	}

	/* If control reaches here, we did not find mac in the table
	 * and we have atleast one free entry in table.
	 * Add the entry at index = idx_to_insert
	 */
	qdf_mem_copy(&pa->unassoc_pool[idx_to_insert].mac,
		     unassoc_mac, sizeof(struct qdf_mac_addr));
	qdf_mem_copy(&pa->unassoc_pool[idx_to_insert].cfr_params,
		     params, sizeof(struct cfr_capture_params));
	pa->unassoc_pool[idx_to_insert].is_valid = true;
	pa->cfr_current_sta_count++;

	return 0;
}

int ucfg_cfr_stop_capture_probe_req(struct wlan_objmgr_pdev *pdev,
				    struct qdf_mac_addr *unassoc_mac)
{
	struct pdev_cfr *pa;
	int idx;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_CFR);
	if (!pa) {
		cfr_err("Pdev cfr object is NULL!\n");
		return -EINVAL;
	}

	if (!(pa->is_cfr_capable)) {
		cfr_err("CFR is not supported on this chip\n");
		return -EINVAL;
	}

	for (idx = 0; idx < MAX_CFR_ENABLED_CLIENTS; idx++) {
		/* Remove mac only if it is present */
		if (qdf_mem_cmp(&pa->unassoc_pool[idx].mac, unassoc_mac,
				sizeof(struct qdf_mac_addr)) == 0) {
			qdf_mem_zero(&pa->unassoc_pool[idx],
				     sizeof(struct unassoc_pool_entry));
			pa->cfr_current_sta_count--;
			return 0;
		}
	}

	/* If mac was present in pool it would have been deleted in the
	 * above loop and returned from there.
	 * If control reached here, mac was not found. So, ignore the request.
	 */
	cfr_err("Trying to delete mac not present in pool. Ignoring request.");
	return 0;
}

int ucfg_cfr_set_timer(struct wlan_objmgr_pdev *pdev, uint32_t value)
{
	struct pdev_cfr *pa;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_CFR);
	if (pa == NULL) {
		cfr_err("PDEV cfr object is NULL!\n");
		return -EINVAL;
	}

	if (!(pa->is_cfr_capable)) {
		qdf_info("cfr is not supported on this chip\n");
		return -EINVAL;
	}

	return tgt_cfr_enable_cfr_timer(pdev, value);
}
qdf_export_symbol(ucfg_cfr_set_timer);

int ucfg_cfr_get_timer(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_cfr *pa;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_CFR);
	if (pa == NULL) {
		cfr_err("PDEV cfr object is NULL!\n");
		return -EINVAL;
	}

	if (!(pa->is_cfr_capable)) {
		qdf_info("cfr is not supported on this chip\n");
		return -EINVAL;
	}

	return pa->cfr_timer_enable;
}
qdf_export_symbol(ucfg_cfr_get_timer);

int ucfg_cfr_stop_capture(struct wlan_objmgr_pdev *pdev,
			  struct wlan_objmgr_peer *peer)
{
	int status;
	struct peer_cfr *pe;
	struct pdev_cfr *pa;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_CFR);
	if (pa == NULL) {
		cfr_err("PDEV cfr object is NULL!\n");
		return -EINVAL;
	}

	if (!(pa->is_cfr_capable)) {
		qdf_info("cfr is not supported on this chip\n");
		return -EINVAL;
	}

	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_CFR);
	if (pe == NULL) {
		cfr_err("PEER cfr object is NULL!\n");
		return -EINVAL;
	}

	if ((pe->period) && (pe->request))
		status = tgt_cfr_stop_capture(pdev, peer);
	else {
		qdf_info("periodic cfr not started for the client\n");
		return -EINVAL;
	}

	if (status == 0) {
		pe->request = PEER_CFR_CAPTURE_DISABLE;
		pa->cfr_current_sta_count--;
	}

	return status;
}

int ucfg_cfr_list_peers(struct wlan_objmgr_pdev *pdev)
{
	return 0;
}

QDF_STATUS ucfg_cfr_stop_indication(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev) {
		cfr_err("null vdev");
		return QDF_STATUS_E_INVAL;
	}

	return cfr_stop_indication(vdev);
}

#ifdef WLAN_ENH_CFR_ENABLE

static inline
QDF_STATUS dev_sanity_check(struct wlan_objmgr_vdev *vdev,
			    struct wlan_objmgr_pdev **ppdev,
			    struct pdev_cfr **ppcfr)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev) {
		cfr_err("vdev is NULL\n");
		return QDF_STATUS_E_NULL_VALUE;
	}

	*ppdev = wlan_vdev_get_pdev(vdev);

	if (!*ppdev) {
		cfr_err("pdev is NULL\n");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = wlan_objmgr_pdev_try_get_ref(*ppdev, WLAN_CFR_ID);
	if (status != QDF_STATUS_SUCCESS) {
		cfr_err("Failed to get pdev reference\n");
		return status;
	}

	*ppcfr = wlan_objmgr_pdev_get_comp_private_obj(*ppdev,
						     WLAN_UMAC_COMP_CFR);

	if (!(*ppcfr)) {
		cfr_err("pdev object for CFR is null");
		wlan_objmgr_pdev_release_ref(*ppdev, WLAN_CFR_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!(*ppcfr)->is_cfr_rcc_capable) {
		cfr_err("cfr is not supported on this chip\n");
		wlan_objmgr_pdev_release_ref(*ppdev, WLAN_CFR_ID);
		return QDF_STATUS_E_NOSUPPORT;
	}

	return status;
}

/*
 * This is needed only in case of m_ta_ra_filter mode.
 * If user wants to reset the group configurations to default values,
 * then this handler will come into action.
 *
 * If user wants to reset the configurations of 0th, 1st and 3rd group,
 * then the input should be :
 *
 *               wlanconfig ath0 cfr reset_cfg 0xb
 *
 */

QDF_STATUS ucfg_cfr_set_reset_bitmap(struct wlan_objmgr_vdev *vdev,
				     struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	pcfr->rcc_param.modified_in_curr_session |= params->reset_cfg;
	tgt_cfr_default_ta_ra_cfg(pdev, &pcfr->rcc_param,
				  true, params->reset_cfg);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * This is needed only in case of m_ta_ra_filter mode.
 * After providing all the group configurations, user should provide
 * the information about which groups need to be enabled.
 * Based on that FW will enable the configurations for CFR groups.
 * If user has to enable only 0th group, then input should be :
 *
 *               wlanconfig ath0 cfr en_cfg 0x1
 *
 * Enable the bitmap from user provided configuration into cfr_rcc_param.
 */

QDF_STATUS ucfg_cfr_set_en_bitmap(struct wlan_objmgr_vdev *vdev,
				  struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	pcfr->rcc_param.filter_group_bitmap = params->en_cfg;

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * Copy user provided input for ul_mu_user_mask into cfr_rcc_param.
 */

QDF_STATUS
ucfg_cfr_set_ul_mu_user_mask(struct wlan_objmgr_vdev *vdev,
			     struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	pcfr->rcc_param.ul_mu_user_mask_lower = params->ul_mu_user_mask_lower;
	pcfr->rcc_param.ul_mu_user_mask_upper = params->ul_mu_user_mask_upper;

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * FREEZE_TLV_DELAY_CNT_* registers are used for FREEZE TLV timeout mechanism
 * in MAC side. In case MAC send FREEZE TLV to PHY too late due to
 * long AST delay, PHY ucode may not handle it well or it will impact
 * next frame’s normal processing, then MAC needs to drop FREEZE TLV
 * sending process after reaching the threshold.
 *
 * This handler will copy user provided input for freeze_tlv_delay_cnt
 * into cfr_rcc_param.
 */

QDF_STATUS
ucfg_cfr_set_freeze_tlv_delay_cnt(struct wlan_objmgr_vdev *vdev,
				  struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	pcfr->rcc_param.freeze_tlv_delay_cnt_en =
		params->freeze_tlv_delay_cnt_en;

	pcfr->rcc_param.freeze_tlv_delay_cnt_thr =
		params->freeze_tlv_delay_cnt_thr;

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * Set capture interval from the provided configuration into cfr_rcc_param.
 * All fixed parameters are needed to be stored into cfr_rcc_param.
 */

QDF_STATUS
ucfg_cfr_set_capture_interval(struct wlan_objmgr_vdev *vdev,
			      struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	pcfr->rcc_param.capture_interval = params->cap_intvl;

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * Set capture duration from the provided configuration into cfr_rcc_param.
 * All fixed parameters are needed to be stored into cfr_rcc_param.
 */

QDF_STATUS
ucfg_cfr_set_capture_duration(struct wlan_objmgr_vdev *vdev,
			      struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	pcfr->rcc_param.capture_duration = params->cap_dur;

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * Copy user provided group parameters( type/ subtype of mgmt, ctrl, data )
 * into curr_cfg instance of ta_ra_cfr_cfg.
 * Set valid mask for the provided configuration.
 * Set modified_in_this_session for the particular group.
 */

QDF_STATUS
ucfg_cfr_set_frame_type_subtype(struct wlan_objmgr_vdev *vdev,
				struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct ta_ra_cfr_cfg *curr_cfg = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	/* Populating current config based on user's input */
	curr_cfg = &pcfr->rcc_param.curr[params->grp_id];
	curr_cfg->mgmt_subtype_filter = params->expected_mgmt_subtype;
	curr_cfg->ctrl_subtype_filter = params->expected_ctrl_subtype;
	curr_cfg->data_subtype_filter = params->expected_data_subtype;

	curr_cfg->valid_mgmt_subtype = 1;
	curr_cfg->valid_ctrl_subtype = 1;
	curr_cfg->valid_data_subtype = 1;

	qdf_set_bit(params->grp_id,
		    (unsigned long *)
		    &pcfr->rcc_param.modified_in_curr_session);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * Copy user provided group parameters( BW and NSS )
 * into curr_cfg instance of ta_ra_cfr_cfg.
 * Set valid mask for the provided configuration.
 * Set modified_in_this_session for the particular group.
 */

QDF_STATUS ucfg_cfr_set_bw_nss(struct wlan_objmgr_vdev *vdev,
			       struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct ta_ra_cfr_cfg *curr_cfg = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	/* Populating current config based on user's input */
	curr_cfg = &pcfr->rcc_param.curr[params->grp_id];
	curr_cfg->bw = params->bw;
	curr_cfg->nss = params->nss;

	curr_cfg->valid_bw_mask = 1;
	curr_cfg->valid_nss_mask = 1;

	qdf_set_bit(params->grp_id,
		    (unsigned long *)&pcfr->rcc_param.modified_in_curr_session);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * Copy user provided group parameters( TA, RA, TA_MASK, RA_MASK )
 * into curr_cfg instance of ta_ra_cfr_cfg.
 * Set valid mask for the provided configuration.
 * Set modified_in_this_session for the particular group.
 */

QDF_STATUS ucfg_cfr_set_tara_config(struct wlan_objmgr_vdev *vdev,
				    struct cfr_wlanconfig_param *params)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct ta_ra_cfr_cfg *curr_cfg = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	curr_cfg = &pcfr->rcc_param.curr[params->grp_id];
	qdf_mem_copy(curr_cfg->tx_addr, params->ta, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(curr_cfg->rx_addr, params->ra, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(curr_cfg->tx_addr_mask,
		     params->ta_mask, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(curr_cfg->rx_addr_mask,
		     params->ra_mask, QDF_MAC_ADDR_SIZE);

	curr_cfg->valid_ta = 1;
	curr_cfg->valid_ta_mask = 1;
	curr_cfg->valid_ra = 1;
	curr_cfg->valid_ra_mask = 1;

	qdf_set_bit(params->grp_id,
		    (unsigned long *)
		    &pcfr->rcc_param.modified_in_curr_session);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

static bool cfr_is_filter_enabled(struct cfr_rcc_param *rcc_param)
{
	if (rcc_param->m_directed_ftm ||
	    rcc_param->m_all_ftm_ack ||
	    rcc_param->m_ndpa_ndp_directed ||
	    rcc_param->m_ndpa_ndp_all ||
	    rcc_param->m_ta_ra_filter ||
	    rcc_param->m_all_packet)
		return true;
	else
		return false;
}

QDF_STATUS ucfg_cfr_get_cfg(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct ta_ra_cfr_cfg *glbl_cfg = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t grp_id;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;
	if (!cfr_is_filter_enabled(&pcfr->rcc_param)) {
		cfr_err(" All RCC modes are disabled.\n");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);
		return status;
	}

	cfr_err("CAPTURE MODE:\n");

	cfr_err("m_directed_ftm is : %s\n",
		pcfr->rcc_param.m_directed_ftm ?
		"enabled" : "disabled");
	cfr_err("m_all_ftm_ack is : %s\n",
		pcfr->rcc_param.m_all_ftm_ack ?
		"enabled" : "disabled");
	cfr_err("m_ndpa_ndp_directed is: %s\n",
		pcfr->rcc_param.m_ndpa_ndp_directed ?
		"enabled" : "disabled");
	cfr_err("m_ndpa_ndp_all is : %s\n",
		pcfr->rcc_param.m_ndpa_ndp_all ?
		"enabled" : "disabled");
	cfr_err("m_ta_ra_filter is : %s\n",
		pcfr->rcc_param.m_ta_ra_filter ?
		"enabled" : "disabled");
	cfr_err("m_all_packet is : %s\n",
		pcfr->rcc_param.m_all_packet ?
		"enabled" : "disabled");

	cfr_err("capture duration : %u usec\n",
		pcfr->rcc_param.capture_duration);
	cfr_err("capture interval : %u usec\n",
		pcfr->rcc_param.capture_interval);
	cfr_err("UL MU User mask lower : %u\n",
		pcfr->rcc_param.ul_mu_user_mask_lower);
	cfr_err("UL MU User mask upper : %u\n",
		pcfr->rcc_param.ul_mu_user_mask_upper);
	cfr_err("Freeze TLV delay count is : %s\n",
		pcfr->rcc_param.freeze_tlv_delay_cnt_en ?
		"enabled" : "disabled");
	cfr_err("Freeze TLV delay count threshold : %u\n",
		pcfr->rcc_param.freeze_tlv_delay_cnt_thr);
	cfr_err("Enabled CFG id bitmap : 0x%x\n",
		pcfr->rcc_param.filter_group_bitmap);
	cfr_err(" Modified cfg id bitmap : 0x%x\n",
		pcfr->rcc_param.modified_in_curr_session);

	cfr_err("TARA_CONFIG details:\n");

	for (grp_id = 0; grp_id < MAX_TA_RA_ENTRIES; grp_id++) {
		glbl_cfg = &pcfr->global[grp_id];

		cfr_err("Config ID: %d\n", grp_id);
		cfr_err("Bandwidth :0x%x\n", glbl_cfg->bw);
		cfr_err("NSS : 0x%x\n", glbl_cfg->nss);
		cfr_err("valid_ta: %d\n", glbl_cfg->valid_ta);
		cfr_err("valid_ta_mask: %d\n", glbl_cfg->valid_ta_mask);
		cfr_err("valid_ra: %d\n", glbl_cfg->valid_ra);
		cfr_err("valid_ra_mask: %d\n", glbl_cfg->valid_ra_mask);
		cfr_err("valid_bw_mask: %d\n", glbl_cfg->valid_bw_mask);
		cfr_err("valid_nss_mask: %d\n", glbl_cfg->valid_nss_mask);
		cfr_err("valid_mgmt_subtype: %d\n",
			glbl_cfg->valid_mgmt_subtype);
		cfr_err("valid_ctrl_subtype: %d\n",
			glbl_cfg->valid_ctrl_subtype);
		cfr_err("valid_data_subtype: %d\n",
			glbl_cfg->valid_data_subtype);
		cfr_err("Mgmt subtype : 0x%x\n",
			glbl_cfg->mgmt_subtype_filter);
		cfr_err("CTRL subtype : 0x%x\n",
			glbl_cfg->ctrl_subtype_filter);
		cfr_err("Data subtype : 0x%x\n",
			glbl_cfg->data_subtype_filter);
		cfr_err("TX Addr: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(glbl_cfg->tx_addr));
		cfr_err("TX Addr Mask: " QDF_FULL_MAC_FMT,
			QDF_FULL_MAC_REF(glbl_cfg->tx_addr_mask));
		cfr_err("RX Addr: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(glbl_cfg->rx_addr));
		cfr_err("RX Addr Mask: " QDF_FULL_MAC_FMT,
			QDF_FULL_MAC_REF(glbl_cfg->rx_addr_mask));
	}

	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

static const char *chan_capture_status_to_str(enum chan_capture_status type)
{
	switch (type) {
	case CAPTURE_IDLE:
		return "CAPTURE_IDLE";
	case CAPTURE_BUSY:
		return "CAPTURE_BUSY";
	case CAPTURE_ACTIVE:
		return "CAPTURE_ACTIVE";
	case CAPTURE_NO_BUFFER:
		return "CAPTURE_NO_BUFFER";
	default:
		return "INVALID";
	}
}

static const
char *mac_freeze_reason_to_str(enum mac_freeze_capture_reason type)
{
	switch (type) {
	case FREEZE_REASON_TM:
		return "FREEZE_REASON_TM";
	case FREEZE_REASON_FTM:
		return "FREEZE_REASON_FTM";
	case FREEZE_REASON_ACK_RESP_TO_TM_FTM:
		return "FREEZE_REASON_ACK_RESP_TO_TM_FTM";
	case FREEZE_REASON_TA_RA_TYPE_FILTER:
		return "FREEZE_REASON_TA_RA_TYPE_FILTER";
	case FREEZE_REASON_NDPA_NDP:
		return "FREEZE_REASON_NDPA_NDP";
	case FREEZE_REASON_ALL_PACKET:
		return "FREEZE_REASON_ALL_PACKET";
	default:
		return "INVALID";
	}
}

QDF_STATUS ucfg_cfr_rcc_dump_dbg_counters(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct cdp_cfr_rcc_stats *cfr_rcc_stats = NULL;
	uint8_t stats_cnt;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		cfr_err("psoc is null!");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	cfr_err("total_tx_evt_cnt = %llu\n",
		pcfr->total_tx_evt_cnt);
	cfr_err("dbr_evt_cnt = %llu\n",
		pcfr->dbr_evt_cnt);
	cfr_err("rx_tlv_evt_cnt = %llu\n",
		pcfr->rx_tlv_evt_cnt);
	cfr_err("release_cnt = %llu\n",
		pcfr->release_cnt);
	cfr_err("Error cnt:\n");
	cfr_err("flush_dbr_cnt = %llu\n",
		pcfr->flush_dbr_cnt);
	cfr_err("invalid_dma_length_cnt = %llu\n",
		pcfr->invalid_dma_length_cnt);
	cfr_err("flush_timeout_dbr_cnt = %llu\n",
		pcfr->flush_timeout_dbr_cnt);
	cfr_err("PPDU id mismatch for same cookie:\n");
	cfr_err("clear_txrx_event = %llu\n",
		pcfr->clear_txrx_event);
	cfr_err("cfr_dma_aborts = %llu\n",
		pcfr->cfr_dma_aborts);

	cfr_rcc_stats = qdf_mem_malloc(sizeof(struct cdp_cfr_rcc_stats));
	if (!cfr_rcc_stats) {
		wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);
		return QDF_STATUS_E_NOMEM;
	}

	cdp_get_cfr_dbg_stats(wlan_psoc_get_dp_handle(psoc),
			      wlan_objmgr_pdev_get_pdev_id(pdev),
			      cfr_rcc_stats);

	cfr_err("bb_captured_channel_cnt: %llu\n",
		cfr_rcc_stats->bb_captured_channel_cnt);
	cfr_err("bb_captured_timeout_cnt: %llu\n",
		cfr_rcc_stats->bb_captured_timeout_cnt);
	cfr_err("rx_loc_info_valid_cnt: %llu\n",
		cfr_rcc_stats->rx_loc_info_valid_cnt);

	cfr_err("Channel capture status:\n");
	for (stats_cnt = 0; stats_cnt < CAPTURE_MAX; stats_cnt++) {
		cfr_err("%s = %llu\n",
			chan_capture_status_to_str(stats_cnt),
			cfr_rcc_stats->chan_capture_status[stats_cnt]);
	}

	cfr_err("Freeze reason:\n");
	for (stats_cnt = 0; stats_cnt < FREEZE_REASON_MAX; stats_cnt++) {
		cfr_err("%s = %llu\n",
			mac_freeze_reason_to_str(stats_cnt),
			cfr_rcc_stats->reason_cnt[stats_cnt]);
	}

	qdf_mem_free(cfr_rcc_stats);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

QDF_STATUS ucfg_cfr_rcc_clr_dbg_counters(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		cfr_err("psoc is null!");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}
	cdp_cfr_clr_dbg_stats(wlan_psoc_get_dp_handle(psoc),
			      wlan_objmgr_pdev_get_pdev_id(pdev));

	pcfr->dbr_evt_cnt = 0;
	pcfr->release_cnt = 0;
	pcfr->total_tx_evt_cnt = 0;
	pcfr->rx_tlv_evt_cnt = 0;
	pcfr->flush_dbr_cnt = 0;
	pcfr->flush_timeout_dbr_cnt = 0;
	pcfr->invalid_dma_length_cnt = 0;
	pcfr->clear_txrx_event = 0;
	pcfr->cfr_dma_aborts = 0;
	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

QDF_STATUS ucfg_cfr_rcc_dump_lut(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev) {
		cfr_err("vdev is NULL\n");
		return QDF_STATUS_E_INVAL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		cfr_err("pdev is NULL\n");
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_CFR_ID) !=
	    QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_INVAL;
	}

	cfr_err("LUT table:\n");
	tgt_cfr_dump_lut_enh(pdev);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

static void cfr_set_filter(struct wlan_objmgr_pdev *pdev, bool enable,
			   struct cdp_monitor_filter *filter_val)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	cfr_info("pdev_id=%d\n", wlan_objmgr_pdev_get_pdev_id(pdev));

	cdp_cfr_filter(wlan_psoc_get_dp_handle(psoc),
		       wlan_objmgr_pdev_get_pdev_id(pdev),
		       enable,
		       filter_val);
}

/*
 * With the initiation of commit command, this handler will be triggered.
 *
 * Starts the procedure of forming the TLVs.
 * If Host succeeds to send WMI command to FW, after TLV processing, then it
 * will save the previous CFR configurations into one instance ta_ra_cfr_cfg,
 * called glbl_cfg and update the current config to default state for the
 * next commit session.
 *
 * Finally, reset the counter (modified_in_this_session) to 0 before moving to
 * next commit session.
 *
 */

QDF_STATUS ucfg_cfr_committed_rcc_config(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct cdp_monitor_filter filter_val = {0};

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc) {
		cfr_err("psoc is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pcfr->rcc_param.vdev_id = wlan_vdev_get_id(vdev);

	/*
	 * If capture mode is valid, then Host:
	 * Subscribes for PPDU status TLVs in monitor status ring.
	 * Sets filter type to either FP or MO, based on the capture mode.
	 * Starts the LUT_AGE_TIMER of 1sec.
	 *
	 * If capture mode is disabled, then Host:
	 * unsubscribes for PPDU status TLVs in monitor status ring.
	 * Sets filter type to 0.
	 * Stops the LUT_AGE_TIMER.
	 *
	 */

	if (cfr_is_filter_enabled(&pcfr->rcc_param)) {
		if (pcfr->rcc_param.m_all_ftm_ack) {
			filter_val.mode |= MON_FILTER_PASS |
					   MON_FILTER_OTHER;
			filter_val.fp_mgmt |= FILTER_MGMT_ACTION;
			filter_val.mo_mgmt |= FILTER_MGMT_ACTION;
		}

		if (pcfr->rcc_param.m_ndpa_ndp_all) {
			filter_val.mode |= MON_FILTER_PASS |
					   MON_FILTER_OTHER;
			filter_val.fp_ctrl |= FILTER_CTRL_VHT_NDP;
			filter_val.mo_ctrl |= FILTER_CTRL_VHT_NDP;
		}

		if (pcfr->rcc_param.m_all_packet) {
			filter_val.mode |= MON_FILTER_PASS |
					   MON_FILTER_OTHER;
			filter_val.fp_mgmt |= FILTER_MGMT_ALL;
			filter_val.mo_mgmt |= FILTER_MGMT_ALL;
			filter_val.fp_ctrl |= FILTER_CTRL_ALL;
			filter_val.mo_ctrl |= FILTER_CTRL_ALL;
			filter_val.fp_data |= FILTER_DATA_ALL;
			filter_val.mo_data |= FILTER_DATA_ALL;
		}

		/*
		 * M_TA_RA in monitor other is as intensive as M_ALL pkt
		 * Support only FP in M_TA_RA mode
		 */
		if (pcfr->rcc_param.m_ta_ra_filter) {
			filter_val.mode |= MON_FILTER_PASS |
					   MON_FILTER_OTHER;
			filter_val.fp_mgmt |= FILTER_MGMT_ALL;
			filter_val.mo_mgmt |= FILTER_MGMT_ALL;
			filter_val.fp_ctrl |= FILTER_CTRL_ALL;
			filter_val.mo_ctrl |= FILTER_CTRL_ALL;
			filter_val.fp_data |= FILTER_DATA_ALL;
			filter_val.mo_data |= FILTER_DATA_ALL;
		}

		if (pcfr->rcc_param.m_directed_ftm) {
			filter_val.mode |= MON_FILTER_PASS;
			filter_val.fp_mgmt |= FILTER_MGMT_ACTION;
		}

		if (pcfr->rcc_param.m_ndpa_ndp_directed) {
			filter_val.mode |= MON_FILTER_PASS;
			filter_val.fp_ctrl |= FILTER_CTRL_VHT_NDP;
		}

		if (!cdp_get_cfr_rcc(wlan_psoc_get_dp_handle(psoc),
				    wlan_objmgr_pdev_get_pdev_id(pdev)))
			tgt_cfr_start_lut_age_timer(pdev);
		cfr_set_filter(pdev, 1, &filter_val);
	} else {
		if (cdp_get_cfr_rcc(wlan_psoc_get_dp_handle(psoc),
				    wlan_objmgr_pdev_get_pdev_id(pdev)))
			tgt_cfr_stop_lut_age_timer(pdev);
		cfr_set_filter(pdev, 0, &filter_val);
	}

	/* Trigger wmi to start the TLV processing. */
	status = tgt_cfr_config_rcc(pdev, &pcfr->rcc_param);
	if (status == QDF_STATUS_SUCCESS) {
		cfr_info("CFR commit done\n");
		/* Update global config */
		tgt_cfr_update_global_cfg(pdev);

		/* Bring curr_cfg to default state for next commit session */
		tgt_cfr_default_ta_ra_cfg(pdev, &pcfr->rcc_param,
					  false, MAX_RESET_CFG_ENTRY);
	} else {
		cfr_err("CFR commit failed\n");
	}

	pcfr->rcc_param.num_grp_tlvs = 0;
	pcfr->rcc_param.modified_in_curr_session = 0;
	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

/*
 * This handler is used to enable / disable the capture mode.
 *
 */
QDF_STATUS ucfg_cfr_set_rcc_mode(struct wlan_objmgr_vdev *vdev,
				 enum capture_type mode, uint8_t value)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	switch (mode) {
	case RCC_DIRECTED_FTM_FILTER:
		pcfr->rcc_param.m_directed_ftm = value;
		break;
	case RCC_ALL_FTM_ACK_FILTER:
		pcfr->rcc_param.m_all_ftm_ack = value;
		break;
	case RCC_DIRECTED_NDPA_NDP_FILTER:
		pcfr->rcc_param.m_ndpa_ndp_directed = value;
		break;
	case RCC_NDPA_NDP_ALL_FILTER:
		pcfr->rcc_param.m_ndpa_ndp_all = value;
		break;
	case RCC_TA_RA_FILTER:
		pcfr->rcc_param.m_ta_ra_filter = value;
		break;
	case RCC_ALL_PACKET_FILTER:
		pcfr->rcc_param.m_all_packet = value;
		break;
	case RCC_DIS_ALL_MODE:
		pcfr->rcc_param.m_directed_ftm = value;
		pcfr->rcc_param.m_all_ftm_ack = value;
		pcfr->rcc_param.m_ndpa_ndp_directed = value;
		pcfr->rcc_param.m_ndpa_ndp_all = value;
		pcfr->rcc_param.m_ta_ra_filter = value;
		pcfr->rcc_param.m_all_packet = value;
		break;

	default:
		break;
	}

	cfr_debug("<CFR_UMAC> Capture mode set by user: 0x%x\n", value);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return status;
}

bool ucfg_cfr_get_rcc_enabled(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_cfr *pcfr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool rcc_enabled = false;

	status = dev_sanity_check(vdev, &pdev, &pcfr);
	if (status != QDF_STATUS_SUCCESS)
		return false;

	rcc_enabled = cfr_is_filter_enabled(&pcfr->rcc_param);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

	return rcc_enabled;
}

QDF_STATUS ucfg_cfr_subscribe_ppdu_desc(struct wlan_objmgr_pdev *pdev,
					bool is_subscribe)
{
	return tgt_cfr_subscribe_ppdu_desc(pdev, is_subscribe);
}
#endif
