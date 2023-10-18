/* Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: contains MLO manager ap related functionality
 */
#include "wlan_mlo_mgr_cmn.h"
#include "wlan_mlo_mgr_main.h"
#ifdef WLAN_MLO_MULTI_CHIP
#include "wlan_lmac_if_def.h"
#include <cdp_txrx_mlo.h>
#endif
#include <wlan_mgmt_txrx_rx_reo_utils_api.h>

#ifdef WLAN_MLO_MULTI_CHIP
static inline
bool mlo_psoc_get_index_id(struct wlan_objmgr_psoc *psoc,
			   uint8_t grp_id,
			   uint8_t *index,
			   bool teardown)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id;

	if (!mlo_ctx)
		return false;

	if (!psoc)
		return false;

	if (!index)
		return false;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return false;
	}

	for (id = 0; id < mlo_ctx->setup_info[grp_id].tot_socs; id++)
		if (mlo_ctx->setup_info[grp_id].curr_soc_list[id] == psoc) {
			*index = id;
			return true;
		}

	if (teardown)
		return false;

	for (id = 0; id < mlo_ctx->setup_info[grp_id].tot_socs; id++)
		if (!mlo_ctx->setup_info[grp_id].curr_soc_list[id]) {
			*index = id;
			return true;
		}

	return false;
}

bool mlo_psoc_get_grp_id(struct wlan_objmgr_psoc *psoc, uint8_t *ret_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t grp_id;
	uint8_t tot_socs;
	uint8_t id;

	if (!mlo_ctx)
		return false;

	if (!psoc)
		return false;

	if (!ret_id)
		return false;

	for (grp_id = 0; grp_id < mlo_ctx->total_grp; grp_id++) {
		tot_socs = mlo_ctx->setup_info[grp_id].tot_socs;
		for (id = 0; id < tot_socs; id++)
			if (mlo_ctx->setup_info[grp_id].soc_list[id] == psoc) {
				*ret_id = grp_id;
				return true;
			}
	}

	return false;
}

qdf_export_symbol(mlo_psoc_get_grp_id);

bool mlo_is_ml_soc(struct wlan_objmgr_psoc *psoc, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id;

	if (!mlo_ctx)
		return false;

	if (!psoc)
		return false;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return false;
	}

	for (id = 0; id < mlo_ctx->setup_info[grp_id].tot_socs; id++)
		if (mlo_ctx->setup_info[grp_id].curr_soc_list[id] == psoc)
			return true;

	return false;
}

qdf_export_symbol(mlo_is_ml_soc);

static void mlo_set_soc_list(uint8_t grp_id, struct wlan_objmgr_psoc *psoc)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t idx;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	for (idx = 0; idx < mlo_ctx->setup_info[grp_id].tot_socs; idx++) {
		if (mlo_ctx->setup_info[grp_id].soc_id_list[idx] ==
				psoc->soc_objmgr.psoc_id) {
			mlo_ctx->setup_info[grp_id].soc_list[idx] = psoc;
		}
	}
}

void mlo_get_soc_list(struct wlan_objmgr_psoc **soc_list,
		      uint8_t grp_id,
		      uint8_t total_socs,
		      enum MLO_SOC_LIST curr)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t chip_idx;

	if (!mlo_ctx)
		goto err_case;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		goto err_case;
	}

	if (total_socs != mlo_ctx->setup_info[grp_id].tot_socs) {
		mlo_err("Mismatch in number of socs in the grp id %d, expected %d observed %d",
			grp_id, total_socs,
			mlo_ctx->setup_info[grp_id].tot_socs);
		goto err_case;
	}

	if (curr == WLAN_MLO_GROUP_CURRENT_SOC_LIST) {
		for (chip_idx = 0; chip_idx < total_socs; chip_idx++)
			soc_list[chip_idx] =
			mlo_ctx->setup_info[grp_id].curr_soc_list[chip_idx];
	} else {
		for (chip_idx = 0; chip_idx < total_socs; chip_idx++)
			soc_list[chip_idx] =
				mlo_ctx->setup_info[grp_id].soc_list[chip_idx];
	}

	return;

err_case:
		for (chip_idx = 0; chip_idx < total_socs; chip_idx++)
			soc_list[chip_idx] = NULL;

		return;
}

qdf_export_symbol(mlo_get_soc_list);

void mlo_cleanup_asserted_soc_setup_info(struct wlan_objmgr_psoc *psoc,
					 uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t link_idx;
	struct wlan_objmgr_pdev *pdev;
	struct mlo_setup_info *setup_info;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->num_links)
		return;

	if (!psoc) {
		mlo_info("NULL psoc");
		return;
	}

	for (link_idx = 0; link_idx < MAX_MLO_LINKS; link_idx++) {
		pdev = setup_info->pdev_list[link_idx];
		if (pdev) {
			if (wlan_pdev_get_psoc(pdev) == psoc) {
				setup_info->pdev_list[link_idx] = NULL;
				setup_info->state[link_idx] = MLO_LINK_TEARDOWN;
				setup_info->num_links--;
			}
		}
	}
}

qdf_export_symbol(mlo_cleanup_asserted_soc_setup_info);

void mlo_setup_update_soc_id_list(uint8_t grp_id, uint8_t *soc_id_list)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint32_t tot_socs;
	uint32_t num_soc;
	uint8_t *soc_list;

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	tot_socs = mlo_ctx->setup_info[grp_id].tot_socs;
	soc_list = mlo_ctx->setup_info[grp_id].soc_id_list;

	for (num_soc = 0; num_soc < tot_socs; num_soc++)
		soc_list[num_soc] = soc_id_list[num_soc];
}

qdf_export_symbol(mlo_setup_update_soc_id_list);

uint8_t mlo_setup_get_total_socs(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return 0;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return 0;
	}

	return mlo_ctx->setup_info[grp_id].tot_socs;
}

qdf_export_symbol(mlo_setup_get_total_socs);

void mlo_setup_update_total_socs(uint8_t grp_id, uint8_t tot_socs)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	mlo_ctx->setup_info[grp_id].tot_socs = tot_socs;
	mlo_ctx->setup_info[grp_id].ml_grp_id = grp_id;
}

qdf_export_symbol(mlo_setup_update_total_socs);

static QDF_STATUS mlo_find_pdev_idx(struct wlan_objmgr_pdev *pdev,
				    uint8_t *link_idx, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t idx;

	if (!mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	if (!link_idx)
		return QDF_STATUS_E_FAILURE;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return QDF_STATUS_E_FAILURE;
	}

	for (idx = 0; idx < mlo_ctx->setup_info[grp_id].tot_links; idx++) {
		if (mlo_ctx->setup_info[grp_id].pdev_list[idx] == pdev) {
			*link_idx = idx;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

#define WLAN_SOC_ID_NOT_INITIALIZED -1
bool mlo_vdevs_check_single_soc(struct wlan_objmgr_vdev **wlan_vdev_list,
				uint8_t vdev_count)
{
	int i;
	uint8_t soc_id = WLAN_SOC_ID_NOT_INITIALIZED;

	for (i = 0; i < vdev_count; i++) {
		uint8_t vdev_soc_id = wlan_vdev_get_psoc_id(wlan_vdev_list[i]);

		if (i == 0)
			soc_id = vdev_soc_id;
		else if (soc_id != vdev_soc_id)
			return false;
	}

	return true;
}

qdf_export_symbol(mlo_vdevs_check_single_soc);

static void mlo_check_state(struct wlan_objmgr_psoc *psoc,
			    void *obj, void *args)
{
	struct wlan_objmgr_pdev *pdev;
	uint8_t link_idx;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_state_params *params = (struct mlo_state_params *)args;

	uint8_t grp_id = params->grp_id;
	pdev = (struct wlan_objmgr_pdev *)obj;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) != QDF_STATUS_SUCCESS) {
		mlo_info("Failed to find pdev");
		return;
	}

	if (mlo_ctx->setup_info[grp_id].state[link_idx] != params->check_state)
		params->link_state_fail = 1;
}

QDF_STATUS mlo_check_all_pdev_state(struct wlan_objmgr_psoc *psoc,
				    uint8_t grp_id,
				    enum MLO_LINK_STATE state)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct mlo_state_params params = {0};

	params.check_state = state;
	params.grp_id = grp_id;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
				     mlo_check_state, &params,
				     0, WLAN_MLME_NB_ID);

	if (params.link_state_fail)
		status = QDF_STATUS_E_INVAL;
	else
		status = QDF_STATUS_SUCCESS;

	return status;
}

void mlo_setup_init(uint8_t total_grp)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t id;

	if (!mlo_ctx)
		return;

	if (!total_grp && total_grp > WLAN_MAX_MLO_GROUPS) {
		mlo_err("Total number of groups (%d) is greater than MAX (%d), MLD Setup failed!!",
			total_grp, WLAN_MAX_MLO_GROUPS);
		return;
	}

	mlo_ctx->total_grp = total_grp;
	setup_info = qdf_mem_malloc(sizeof(struct mlo_setup_info) *
					      total_grp);

	if (!setup_info)
		return;

	mlo_ctx->setup_info = setup_info;
	mlo_ctx->setup_info[0].ml_grp_id = 0;
	for (id = 0; id < total_grp; id++) {
		if (qdf_event_create(&mlo_ctx->setup_info[id].event) !=
							QDF_STATUS_SUCCESS)
			mlo_err("Unable to create teardown event");
	}
}

qdf_export_symbol(mlo_setup_init);

void mlo_setup_deinit(void)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id;

	if (!mlo_ctx)
		return;

	if (!mlo_ctx->setup_info)
		return;

	for (id = 0; id < mlo_ctx->total_grp; id++)
		qdf_event_destroy(&mlo_ctx->setup_info[id].event);

	qdf_mem_free(mlo_ctx->setup_info);
	mlo_ctx->setup_info = NULL;
}

qdf_export_symbol(mlo_setup_deinit);

void mlo_setup_update_num_links(struct wlan_objmgr_psoc *psoc,
				uint8_t grp_id, uint8_t num_links)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	mlo_ctx->setup_info[grp_id].tot_links += num_links;
}

qdf_export_symbol(mlo_setup_update_num_links);

void mlo_setup_update_soc_ready(struct wlan_objmgr_psoc *psoc, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t chip_idx, tot_socs;
	struct cdp_mlo_ctxt *dp_mlo_ctxt;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->tot_socs)
		return;

	tot_socs = setup_info->tot_socs;
	if (!mlo_psoc_get_index_id(psoc, grp_id, &chip_idx, 0))  {
		mlo_err("Unable to fetch chip idx for psoc id %d grp id %d",
			psoc->soc_objmgr.psoc_id,
			grp_id);
		return;
	}

	if (!(chip_idx < tot_socs)) {
		mlo_err("Invalid chip index, SoC setup failed");
		return;
	}

	setup_info->curr_soc_list[chip_idx] = psoc;
	mlo_set_soc_list(grp_id, psoc);
	setup_info->num_soc++;

	mlo_debug("SoC updated to mld grp %d , chip idx %d num soc %d",
		  grp_id, chip_idx, setup_info->num_soc);

	if (setup_info->num_soc != tot_socs)
		return;

	dp_mlo_ctxt = cdp_mlo_ctxt_attach(wlan_psoc_get_dp_handle(psoc),
			(struct cdp_ctrl_mlo_mgr *)mlo_ctx);
	wlan_objmgr_set_dp_mlo_ctx(dp_mlo_ctxt, grp_id);

	for (chip_idx = 0; chip_idx < tot_socs; chip_idx++) {
		struct wlan_objmgr_psoc *tmp_soc =
			setup_info->curr_soc_list[chip_idx];
		if (tmp_soc)
			cdp_soc_mlo_soc_setup(wlan_psoc_get_dp_handle(tmp_soc),
					      setup_info->dp_handle);
	}

	cdp_mlo_setup_complete(wlan_psoc_get_dp_handle(psoc),
			       setup_info->dp_handle);
}

qdf_export_symbol(mlo_setup_update_soc_ready);

void mlo_setup_link_ready(struct wlan_objmgr_pdev *pdev, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;
	uint16_t link_id;

	if (!mlo_ctx)
		return;

	if (!pdev)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->tot_links) {
		mlo_err("Setup info total links %d for grp id %d",
			setup_info->tot_links, grp_id);
		return;
	}

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) == QDF_STATUS_SUCCESS) {
		mlo_debug("Pdev already part of list link idx %d", link_idx);
		return;
	}

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (!setup_info->pdev_list[link_idx])
			break;

	if (link_idx >= setup_info->tot_links) {
		mlo_err("Exceeding max total mld links");
		return;
	}

	setup_info->pdev_list[link_idx] = pdev;
	setup_info->state[link_idx] = MLO_LINK_SETUP_INIT;
	setup_info->num_links++;

	link_id = wlan_mlo_get_pdev_hw_link_id(pdev);
	if (link_id == INVALID_HW_LINK_ID) {
		mlo_err("Invalid HW link id for the pdev");
		return;
	}
	setup_info->valid_link_bitmap |= (1 << link_id);

	mlo_debug("Pdev updated to Grp id %d mld link %d num_links %d  hw link id %d Valid link bitmap %d",
		  grp_id, link_idx, setup_info->num_links,
		  link_id, setup_info->valid_link_bitmap);

	qdf_assert_always(link_idx < MAX_MLO_LINKS);

	if (setup_info->num_links == setup_info->tot_links &&
	    setup_info->num_soc == setup_info->tot_socs) {
		struct wlan_objmgr_psoc *psoc;
		struct wlan_lmac_if_tx_ops *tx_ops;
		QDF_STATUS status;

		psoc = wlan_pdev_get_psoc(pdev);
		tx_ops = wlan_psoc_get_lmac_if_txops(psoc);

		status = wlan_mgmt_rx_reo_validate_mlo_link_info(psoc);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("Failed to validate MLO HW link info");
			qdf_assert_always(0);
		}

		mlo_debug("Trigger MLO Setup request");
		if (tx_ops && tx_ops->mops.target_if_mlo_setup_req) {
			tx_ops->mops.target_if_mlo_setup_req(
					setup_info->pdev_list,
					setup_info->num_links,
					grp_id);
		}
	}
}

qdf_export_symbol(mlo_setup_link_ready);

void mlo_link_setup_complete(struct wlan_objmgr_pdev *pdev, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;

	if (!mlo_ctx)
		return;

	if (!pdev)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (setup_info->pdev_list[link_idx] == pdev) {
			setup_info->state[link_idx] =
							MLO_LINK_SETUP_DONE;
			break;
		}

	mlo_debug("Setup complete for pdev id %d mlo group %d",
		  pdev->pdev_objmgr.wlan_pdev_id, grp_id);

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (setup_info->state[link_idx] == MLO_LINK_SETUP_DONE)
			continue;
		else
			break;

	if (link_idx == setup_info->tot_links) {
		struct wlan_objmgr_psoc *psoc;
		struct wlan_lmac_if_tx_ops *tx_ops;

		psoc = wlan_pdev_get_psoc(pdev);
		tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
		mlo_debug("Trigger MLO ready");
		if (tx_ops && tx_ops->mops.target_if_mlo_ready) {
			tx_ops->mops.target_if_mlo_ready(
					setup_info->pdev_list,
					setup_info->num_links);
		}
	}
}

qdf_export_symbol(mlo_link_setup_complete);

static void mlo_setup_link_down(struct wlan_objmgr_psoc *psoc,
				void *obj, void *args)
{
	struct wlan_objmgr_pdev *pdev;
	uint8_t link_idx;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint16_t link_id;
	uint8_t grp_id = *(uint8_t *)args;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	pdev = (struct wlan_objmgr_pdev *)obj;

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) != QDF_STATUS_SUCCESS) {
		mlo_info("Failed to find pdev");
		return;
	}

	setup_info->pdev_list[link_idx] = NULL;
	setup_info->state[link_idx] = MLO_LINK_UNINITIALIZED;
	setup_info->num_links--;

	link_id = wlan_mlo_get_pdev_hw_link_id(pdev);
	if (link_id == INVALID_HW_LINK_ID) {
		mlo_err("Invalid HW link id for the pdev");
		return;
	}
	setup_info->valid_link_bitmap &= ~(1 << link_id);

	mlo_debug("Pdev link down grp_id %d link_idx %d num_links %d",
		  grp_id, link_idx, setup_info->num_links);
}

void mlo_setup_update_soc_down(struct wlan_objmgr_psoc *psoc, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t chip_idx;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (setup_info->num_links) {
		wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
					     mlo_setup_link_down, &grp_id,
					     0, WLAN_MLME_NB_ID);
	}

	if (!mlo_psoc_get_index_id(psoc, grp_id, &chip_idx, 1)) {
		mlo_err("Unable to fetch chip idx for psoc id %d grp id %d",
			psoc->soc_objmgr.psoc_id,
			grp_id);
		return;
	}

	if (!(chip_idx < MAX_MLO_CHIPS)) {
		mlo_err("Invalid chip index, SoC setup down failed");
		return;
	}

	setup_info->curr_soc_list[chip_idx] = NULL;
	setup_info->num_soc--;

	mlo_debug("Soc down, mlo group %d num soc %d num links %d",
		  grp_id, setup_info->num_soc,
		  setup_info->num_links);
}

qdf_export_symbol(mlo_setup_update_soc_down);

static void mlo_dp_ctxt_detach(struct wlan_objmgr_psoc *psoc,
			       uint8_t grp_id,
			       struct cdp_mlo_ctxt *dp_mlo_ctxt)
{
	if (!psoc)
		return;

	wlan_objmgr_set_dp_mlo_ctx(NULL, grp_id);
	if (dp_mlo_ctxt)
		cdp_mlo_ctxt_detach(wlan_psoc_get_dp_handle(psoc), dp_mlo_ctxt);
}

void mlo_link_teardown_complete(struct wlan_objmgr_pdev *pdev, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;
	struct wlan_objmgr_psoc *soc;
	uint8_t chip_idx;
	uint8_t num_soc = 0;

	if (!mlo_ctx)
		return;

	if (!pdev)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->num_links) {
		mlo_err("Delayed response ignore");
		return;
	}

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) != QDF_STATUS_SUCCESS) {
		mlo_info("Failed to find pdev");
		return;
	}

	mlo_debug("Teardown link idx = %d", link_idx);
	setup_info->state[link_idx] = MLO_LINK_TEARDOWN;

	/* Waiting for teardown on other links */
	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (setup_info->state[link_idx] != MLO_LINK_TEARDOWN)
			return;

	mlo_debug("Teardown complete");

	for (chip_idx = 0; chip_idx < setup_info->tot_socs; chip_idx++) {
		soc = setup_info->curr_soc_list[chip_idx];
		if (soc) {
			num_soc++;
			cdp_soc_mlo_soc_teardown(wlan_psoc_get_dp_handle(soc),
						 setup_info->dp_handle,
						 false);
			if (num_soc == setup_info->tot_socs)
				mlo_dp_ctxt_detach(soc, grp_id,
						   setup_info->dp_handle);
		}
	}

	qdf_event_set(&setup_info->event);
}

qdf_export_symbol(mlo_link_teardown_complete);

static void mlo_force_teardown(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct wlan_objmgr_psoc *soc;
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;
	uint8_t chip_idx;
	uint8_t num_soc = 0;

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		setup_info->state[link_idx] = MLO_LINK_TEARDOWN;

	for (chip_idx = 0; chip_idx < setup_info->tot_socs; chip_idx++) {
		soc = setup_info->curr_soc_list[chip_idx];
		if (soc) {
			num_soc++;
			cdp_soc_mlo_soc_teardown(wlan_psoc_get_dp_handle(soc),
						 setup_info->dp_handle,
						 true);
			if (num_soc == setup_info->tot_socs)
				mlo_dp_ctxt_detach(soc, grp_id,
						   setup_info->dp_handle);
		}
	}
}

#define MLO_MGR_TEARDOWN_TIMEOUT 3000
QDF_STATUS mlo_link_teardown_link(struct wlan_objmgr_psoc *psoc,
				  uint8_t grp_id,
				  uint32_t reason)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	QDF_STATUS status;
	struct mlo_setup_info *setup_info;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return QDF_STATUS_E_INVAL;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	mlo_debug("Teardown req with grp_id %d num_soc %d num_link %d",
		  grp_id, setup_info->num_soc, setup_info->num_links);

	if (!setup_info->num_soc)
		return QDF_STATUS_SUCCESS;

	if (!mlo_check_all_pdev_state(psoc, grp_id, MLO_LINK_TEARDOWN))
		return QDF_STATUS_SUCCESS;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	/* Trigger MLO teardown */
	if (tx_ops && tx_ops->mops.target_if_mlo_teardown_req) {
		tx_ops->mops.target_if_mlo_teardown_req(
				setup_info->pdev_list,
				setup_info->num_links,
				reason);
	}

	if (reason == WMI_MLO_TEARDOWN_REASON_SSR) {
		/* do not wait for teardown event completion here for SSR */
		return QDF_STATUS_SUCCESS;
	}

	status = qdf_wait_for_event_completion(
			&setup_info->event,
			MLO_MGR_TEARDOWN_TIMEOUT);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_debug("Teardown timeout");
		mlo_force_teardown(grp_id);
	}

	return status;
}

qdf_export_symbol(mlo_link_teardown_link);
#endif /*WLAN_MLO_MULTI_CHIP*/
